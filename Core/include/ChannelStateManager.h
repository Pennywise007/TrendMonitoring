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
            : m_timeOfFirstError(std::move(timeOfFirstError))
            , m_countOfIgnoringErrors(_countOfIgnoringErrors)
            , m_countOfSuccessBeforeDeleteError(_countOfSuccessToDeleteState)
        {}

        // notify about error
        void OnAddError(std::wstring& errorMessage, std::wstring&& newError) EXT_NOEXCEPT
        {
            auto curTime = CTime::GetCurrentTime();
            // ignore errors that appear more than a day, information about them will be in the reports
            const auto hoursSinceFirstError = (curTime - m_timeOfFirstError).GetTotalHours();
            if (hoursSinceFirstError > 23)
                return;

            if (m_countOfIgnoringErrors > 0 && m_countOfIgnoringErrors-- != 0)
                return;

            const auto needSendErrorMessage = [&]()
            {
                if (!IsUserNotifiedAboutError())
                    return true;
                if (const auto errorTime = (curTime - m_timeOfLastReporting.value()).GetTotalHours();
                    errorTime > kCountOfHoursForIgnoringSimilarError)
                {
                    newError = std::string_swprintf(L"В течениe %lld часов наблюдается ошибка: ", hoursSinceFirstError) + newError;
                    return true;
                }
                return false;
            };

            if (needSendErrorMessage())
            {
                if (!errorMessage.empty())
                    errorMessage += L' ';

                m_timeOfLastReporting = std::move(curTime);
                errorMessage += std::move(newError);
            }
        }

        EXT_NODISCARD bool CanRemove() EXT_NOEXCEPT
        {
            // if no report has been sent
            if (!IsUserNotifiedAboutError())
                return true;

            EXT_ASSERT(m_countOfSuccessBeforeDeleteError > 0);
            return --m_countOfSuccessBeforeDeleteError == 0;
        }

        EXT_NODISCARD bool CanRemove(std::wstring& errorMessage, std::wstring&& newError) EXT_NOEXCEPT
        {
            const auto res = CanRemove();
            if (res && IsUserNotifiedAboutError())
            {
                if (!errorMessage.empty())
                    errorMessage += L' ';
                errorMessage += std::move(newError);
            }
            return res;
        }

        EXT_NODISCARD bool operator==(const ErrorInfo& other) const EXT_NOEXCEPT
        {
            const auto wrapFields = [](const ErrorInfo& info)
            {
                return std::tie(info.m_countOfIgnoringErrors, info.m_countOfSuccessBeforeDeleteError);
            };

            return wrapFields(*this) == wrapFields(other);
        }

    private:
        EXT_NODISCARD bool IsUserNotifiedAboutError() const EXT_NOEXCEPT { return m_timeOfLastReporting.has_value(); }

    private:
        CTime m_timeOfFirstError;
        std::optional<CTime> m_timeOfLastReporting;

        // count of errors before sending error reports
        size_t m_countOfIgnoringErrors;
        // count of removing error notification before delete error, prevent from spam
        size_t m_countOfSuccessBeforeDeleteError;

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
            EXT_UNREACHABLE("Unknown report type");
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

    if (it->second.CanRemove(errorMessage, std::move(newErrorMessage)))
        channelState.erase(it);
}

inline void ChannelStateManager::OnRemoveChannelErrorReport(const ReportErrors error) EXT_NOEXCEPT
{
    if (auto it = channelState.find(error); it != channelState.end() && it->second.CanRemove())
        channelState.erase(it);
}

inline bool ChannelStateManager::operator==(const ChannelStateManager& other) const
{
    const auto wrapFields = [](const ChannelStateManager& manager)
    {
        return std::tie(manager.dataLoaded, manager.loadingDataError, manager.channelState);
    };

    return wrapFields(*this) == wrapFields(other);
}
