#pragma once

#include <afx.h>
#include <afxwin.h>
#include <cassert>
#include <map>
#include <optional>

#include <ext/core/check.h>
#include <ext/std/string.h>

struct ChannelStateManager;

namespace testing {
inline void SetFirstErrorTime(ChannelStateManager& manager, CTime time);
} // namespace testing

constexpr LONGLONG kCountOfHoursForIgnoringSimilarError = 3;

struct ChannelStateManager
{
    // Состояние канала
    enum ReportErrors
    {
        eFallenOff,     // users were notified about the sensor falling off
        eExcessOfValue, // users were notified about exceeding the allowed value
        eLotOfEmptyData // users were notified about a large number of passes
    };

    void OnAddChannelErrorReport(const ReportErrors error, std::wstring& errorMessage,
                                 std::wstring&& newErrorMessage, CTime firstErrorTime = CTime::GetCurrentTime()) EXT_NOEXCEPT;

    // true - если надо оповестить о новом состоянии
    void OnRemoveChannelErrorReport(const ReportErrors error, std::wstring& errorMessage, std::wstring&& newErrorMessage) EXT_NOEXCEPT;

    // true - если надо оповестить о новом состоянии
    void OnRemoveChannelErrorReport(const ReportErrors error) EXT_NOEXCEPT;

    bool dataLoaded = false;
    bool loadingDataError = false;

    bool operator==(const ChannelStateManager& other) const;

private:
    struct ErrorInfo
    {
        explicit ErrorInfo(const size_t _countOfSuccessToDeleteState,
                           const size_t _countOfIgnoringErrors,
                           CTime&& timeOfFirstError) EXT_NOEXCEPT
            : m_countOfSuccessBeforeDeleteError(_countOfSuccessToDeleteState)
            , m_countOfIgnoringErrors(_countOfIgnoringErrors)
            , m_timeOfFirstError(std::move(timeOfFirstError))
        {}

        // notify about error
        void OnAddError(std::wstring& errorMessage, std::wstring&& newError)
        {
            auto curTime = CTime::GetCurrentTime();
            // ignore errors that appear more than a day, information about them will be in the reports
            const auto hoursSinceFirstError = (curTime - m_timeOfFirstError).GetTotalHours();
            if (hoursSinceFirstError > 23)
                return;

            if (m_countOfIgnoringErrors > 0 && m_countOfIgnoringErrors-- != 0)
                return;

            if (!m_timeOfLastReporting.has_value())
            {
                if (!errorMessage.empty())
                    errorMessage += L' ';

                m_timeOfLastReporting = std::move(curTime);
                errorMessage += std::move(newError);
            }
            else if (const auto errorTime = (curTime - m_timeOfLastReporting.value()).GetTotalHours();
                     errorTime > kCountOfHoursForIgnoringSimilarError)
            {
                if (!errorMessage.empty())
                    errorMessage += L' ';

                errorMessage += std::string_swprintf(L"В течениe %lld часов наблюдается ошибка: ", hoursSinceFirstError);
                m_timeOfLastReporting = std::move(curTime);
                errorMessage += std::move(newError);
            }
        }

        // true если пользователя можно оповестить что проблема решена и удалить информацию об ошибке
        bool OnRemove()
        {
            // if m_countOfIgnoringErrors was setted
            if (!m_timeOfLastReporting.has_value())
                return true;

            EXT_ASSERT(m_countOfSuccessBeforeDeleteError > m_currentCountOfDeletingState);
            return ++m_currentCountOfDeletingState == m_countOfSuccessBeforeDeleteError;
        }

        bool OnRemove(std::wstring& errorMessage, std::wstring&& newError)
        {
            const auto res = OnRemove();
            if (res)
            {
                if (!errorMessage.empty())
                    errorMessage += L' ';
                errorMessage += std::move(newError);
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
        CTime m_timeOfFirstError;
        std::optional<CTime> m_timeOfLastReporting;

        // некоторые сообщения будем фильтровать
        size_t m_countOfIgnoringErrors;
        // некоторые состояния могут стрелять часто, например о превышении уровня, не даём спамить ими
        size_t m_countOfSuccessBeforeDeleteError;
        size_t m_currentCountOfDeletingState = 0;

        friend void testing::SetFirstErrorTime(ChannelStateManager& manager, CTime time);
    };

    std::map<ReportErrors, ErrorInfo> channelState;

    friend void testing::SetFirstErrorTime(ChannelStateManager& manager, CTime time);
};

inline void ChannelStateManager::OnAddChannelErrorReport(const ReportErrors error,
                                                         std::wstring& errorMessage,
                                                         std::wstring&& newErrorMessage,
                                                         CTime firstErrorTime) EXT_NOEXCEPT
{
    auto it = channelState.find(error);
    if (it == channelState.end())
    {
        switch (error)
        {
        case ReportErrors::eFallenOff:
            it = channelState.emplace(error, ErrorInfo(1, 3, std::move(firstErrorTime))).first;
            break;
        case ReportErrors::eExcessOfValue:
        case ReportErrors::eLotOfEmptyData:
            it = channelState.emplace(error, ErrorInfo(3, 0, std::move(firstErrorTime))).first;
            break;
        default:
            EXT_ASSERT(!"Не опознанный тип репорта");
            return;
        }
    }

    it->second.OnAddError(errorMessage, std::move(newErrorMessage));
}

inline void ChannelStateManager::OnRemoveChannelErrorReport(const ReportErrors error,
                                                            std::wstring& errorMessage,
                                                            std::wstring&& newErrorMessage) EXT_NOEXCEPT
{
    auto it = channelState.find(error);
    if (it == channelState.end())
        return;

    if (it->second.OnRemove(errorMessage, std::move(newErrorMessage)))
        channelState.erase(it);
}

inline void ChannelStateManager::OnRemoveChannelErrorReport(const ReportErrors error) EXT_NOEXCEPT
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
