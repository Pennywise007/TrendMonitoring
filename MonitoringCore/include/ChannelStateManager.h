#pragma once

#include <afx.h>
#include <afxwin.h>
#include <cassert>
#include <map>
#include <optional>

#include <ext/core/check.h>
#include <ext/std/string.h>

constexpr LONGLONG kCountOfHoursForIgnoringSimilarError = 3;

// TODO ADD TESTS
struct ChannelStateManager
{
    // —осто€ние канала
    enum ReportErrors
    {
        eFallenOff,     // users were notified about the sensor falling off
        eExcessOfValue, // users were notified about exceeding the allowed value
        eLotOfEmptyData // users were notified about a large number of passes
    };

    template<class... Args>
    void OnAddChannelErrorReport(const ReportErrors error, std::wstring& errorMessage, LPCWSTR newErrorMessageFormat, Args&&... args);

    // true - если надо оповестить о новом состо€нии
    template<class... Args>
    void OnRemoveChannelErrorReport(const ReportErrors error, std::wstring& errorMessage, LPCWSTR newErrorMessageFormat, Args&&... args);

    // true - если надо оповестить о новом состо€нии
    void OnRemoveChannelErrorReport(const ReportErrors error);

    bool dataLoaded = false;
    bool loadingDataError = false;

    bool operator==(const ChannelStateManager& other) const;

private:
    struct ErrorInfo
    {
        explicit ErrorInfo(const size_t _countOfIgnoringErrors,
                           const size_t _countOfSuccessBeforeDeleteError)
            : m_countOfIgnoringErrors(_countOfIgnoringErrors)
            , m_countOfSuccessBeforeDeleteError(_countOfSuccessBeforeDeleteError)
        {}

        explicit ErrorInfo(const size_t _countOfSuccessToDeleteState)
            : m_countOfSuccessBeforeDeleteError(_countOfSuccessToDeleteState)
        {}

        template<class... Args>
        void OnAddError(std::wstring& errorMessage, LPCWSTR newErrorFormat, Args&&... args)
        {
            if (m_countOfIgnoringErrors > 0 && m_countOfIgnoringErrors-- != 0)
                return;

            const auto curTime = CTime::GetCurrentTime();
            if (!m_timeOfLastReporting.has_value())
            {
                if (!errorMessage.empty())
                    errorMessage += L' ';

                m_timeOfLastReporting = std::move(curTime);
                errorMessage += std::string_swprintf(newErrorFormat, std::forward<Args>(args)...);
            }
            else if ((curTime - m_timeOfLastReporting.value()).GetTotalHours() > kCountOfHoursForIgnoringSimilarError)
            {
                if (!errorMessage.empty())
                    errorMessage += L' ';

                errorMessage += std::string_swprintf(L"¬ течениe %lld часов наблюдаетс€ ошибка: ", (curTime - m_timeOfFirstError).GetTotalHours());
                m_timeOfLastReporting = std::move(curTime);
                errorMessage += std::string_swprintf(newErrorFormat, std::forward<Args>(args)...);
            }
        }

        // true если пользовател€ можно оповестить что проблема решена и удалить информацию об ошибке
        bool OnRemove()
        {
            // if m_countOfIgnoringErrors was setted
            if (!m_timeOfLastReporting.has_value())
                return true;

            EXT_ASSERT(m_currentCountOfDeletingState + 1 > m_countOfSuccessBeforeDeleteError);
            return ++m_currentCountOfDeletingState == m_countOfSuccessBeforeDeleteError;
        }

        template<class... Args>
        bool OnRemove(std::wstring& errorMessage, LPCWSTR newErrorFormat, Args&&... args)
        {
            const auto res = OnRemove();
            if (res)
            {
                if (!errorMessage.empty())
                    errorMessage += L' ';
                errorMessage += std::string_swprintf(newErrorFormat, std::forward<Args>(args)...);
            }
            return res;
        }

        bool operator==(const ErrorInfo& other) const
        {
            auto wrap_fields = [](const ErrorInfo& info)
            {
                return std::tie(info.m_countOfIgnoringErrors, info.m_countOfSuccessBeforeDeleteError, info.m_currentCountOfDeletingState);
            };

            return wrap_fields(*this) == wrap_fields(other);
        }
    private:
        CTime m_timeOfFirstError = CTime::GetCurrentTime();
        std::optional<CTime> m_timeOfLastReporting;

        // некоторые сообщени€ будем фильтровать
        size_t m_countOfIgnoringErrors = 0;
        // некоторые состо€ни€ могут стрел€ть часто, например о превышении уровн€, не даЄм спамить ими
        size_t m_countOfSuccessBeforeDeleteError = 1;
        size_t m_currentCountOfDeletingState = 0;
    };

    std::map<ReportErrors, ErrorInfo> channelState;
};

template<class... Args>
inline void ChannelStateManager::OnAddChannelErrorReport(const ReportErrors error, std::wstring& errorMessage,
                                                         LPCWSTR newErrorMessageFormat, Args&&... args)
{
    auto it = channelState.find(error);
    if (it == channelState.end())
    {
        switch (error)
        {
        case ReportErrors::eFallenOff:
            it = channelState.emplace(error, ErrorInfo(3, 1)).first;
            break;
        case ReportErrors::eExcessOfValue:
        case ReportErrors::eLotOfEmptyData:
            it = channelState.emplace(error, 3).first;
            break;
        default:
            EXT_ASSERT(!"Ќе опознанный тип репорта");
            return;
        }
    }

    it->second.OnAddError(errorMessage, newErrorMessageFormat, std::forward<Args>(args)...);
}

template<class... Args>
inline void ChannelStateManager::OnRemoveChannelErrorReport(const ReportErrors error, std::wstring& errorMessage,
                                                            LPCWSTR newErrorMessageFormat, Args&&... args)
{
    auto it = channelState.find(error);
    if (it == channelState.end())
        return;

    if (it->second.OnRemove(errorMessage, newErrorMessageFormat, std::forward<Args>(args)...))
        channelState.erase(it);
}

inline void ChannelStateManager::OnRemoveChannelErrorReport(const ReportErrors error)
{
    if (auto it = channelState.find(error); it != channelState.end() && it->second.OnRemove())
        channelState.erase(it);
}

inline bool ChannelStateManager::operator==(const ChannelStateManager& other) const
{
    auto wrap_fields = [](const ChannelStateManager& manager)
    {
        return std::tie(manager.dataLoaded, manager.loadingDataError, manager.channelState);
    };

    return wrap_fields(*this) == wrap_fields(other);
}
