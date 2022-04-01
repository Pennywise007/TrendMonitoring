#pragma once

#include <afx.h>
#include <list>
#include <memory>

#include <ext/core/dispatcher.h>

#include "ChannelStateManager.h"
#include "ITelegramBot.h"

struct IMonitoringListEvents : ext::events::IBaseEvent
{
    // notification of a change in the list of watched channels
    virtual void OnChanged() = 0;
};

struct ILogEvents : ext::events::IBaseEvent
{
    struct LogMessageData
    {
        enum class MessageType
        {
            eError,         // error message, marked in red in the log
            eOrdinary,
        };
        MessageType messageType = MessageType::eOrdinary;
        std::wstring logMessage;
    };

    // notification if it is necessary to add a message to the log, see LogMessageData and send_message_to_log
    virtual void OnNewLogMessage(const std::shared_ptr<LogMessageData>& logMessage) = 0;
};

struct IMonitoringErrorEvents : ext::events::IBaseEvent
{
    struct EventData
    {
        std::wstring errorTextForAllChannels;
        std::vector<std::wstring> problemChannelNames;
        GUID errorGUID;
    };

    // notification in case of an error in monitoring, see MonitoringErrorEventData
    virtual void OnError(const std::shared_ptr<EventData>& errorData) = 0;
};

struct IReportEvents : ext::events::IBaseEvent
{
    // notification when generating a report, see Message Text Data
    virtual void OnReportDone(std::wstring messageText) = 0;
};

// Trend data
struct TrendChannelData
{
    // initial value at the beginning of the interval
    float startValue = 0.f;
    // current value
    float currentValue = 0.f;
    // maximum value for the observed interval
    float maxValue = 0.f;
    // minimum value for the observed interval
    float minValue = 0.f;
    // time without data (data gaps) for the observed interval
    CTimeSpan emptyDataTime = 0;
    // time of the last "existing" data on the channel
    CTime lastDataExistTime;
};

enum class MonitoringInterval
{
    eOneDay,
    eOneWeek,
    eTwoWeeks,
    eOneMonth,
    eThreeMonths,
    eHalfYear,
    eOneYear,
    eLast
};

// Structure with channel data
struct MonitoringChannelData
{
    std::wstring channelName;
    // Notify users of changes
    bool bNotify = true;
    MonitoringInterval monitoringInterval = MonitoringInterval::eOneMonth;
    // value to be notified when reached. Do Not Notify - NAN
    float alarmingValue = NAN;
    // data on the observed channel, not yet channelState.dataLoaded - not valid/empty
    TrendChannelData trendData;
    // channel status, data availability, error messages
    ChannelStateManager channelState;
};

// Interface for data monitoring, used to get and manage the list of channels, see also IMonitoringListEvents
interface ITrendMonitoring
{
    virtual ~ITrendMonitoring() = default;

#pragma region General functions above the channel list
    /// <summary>Get a list of names of all channels available for monitoring.</summary>
    EXT_NODISCARD virtual std::list<std::wstring> GetNamesOfAllChannels() const = 0;
    /// <summary>Get a list of channel names that are being monitored.</summary>
    EXT_NODISCARD virtual std::list<std::wstring> GetNamesOfMonitoringChannels() const = 0;

    /// <summary>Update data for all channels.</summary>
    virtual void UpdateDataForAllChannels() = 0;

    /// <summary>Get the number of observed channels.</summary>
    EXT_NODISCARD virtual size_t GetNumberOfMonitoringChannels() const = 0;
    /// <summary>Get data for the monitored channel by index.</summary>
    /// <param name="channelIndex">Index in the list of channels.</param>
    /// <returns>Data for the channel.</returns>
    EXT_NODISCARD virtual const MonitoringChannelData& GetMonitoringChannelData(const size_t channelIndex) const = 0;
#pragma endregion General functions above the channel list

#pragma region Channel list management
    /// <summary>Add a channel for monitoring.</summary>
    /// <returns>Index of the added channel in the list.</returns>
    virtual size_t AddMonitoringChannel() = 0;
    /// <summary>Remove the observed channel from the list by index.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <returns>Index of selection after deletion.</returns>
    virtual size_t RemoveMonitoringChannelByIndex(const size_t channelIndex) = 0;

    /// <summary>Change the notification flag of the channel by number.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <param name="newNotifyState">New notification state.</param>
    virtual void ChangeMonitoringChannelNotify(const size_t channelIndex,
                                               const bool newNotifyState) = 0;
    /// <summary>Change the name of the monitored channel.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <param name="newChannelName">New channel name.</param>
    virtual void ChangeMonitoringChannelNotify(const size_t channelIndex,
                                             const std::wstring& newChannelName) = 0;
    /// <summary>Change the data monitoring interval for the observed channel.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <param name="newInterval">New monitoring interval.</param>
    virtual void ChangeMonitoringChannelInterval(const size_t channelIndex,
                                                 const MonitoringInterval newInterval) = 0;
    /// <summary>Change the value upon reaching which a notification will be generated.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <param name="newValue">New value with notification.</param>
    virtual void ChangeMonitoringChannelAlarmingValue(const size_t channelIndex,
                                                      const float newValue) = 0;

    /// <summary>Move the channel up the list by index.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <returns>New channel index.</returns>
    virtual size_t MoveUpMonitoringChannelByIndex(const size_t channelIndex) = 0;
    /// <summary>Move the channel down the list by index.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <returns>New channel index.</returns>
    virtual size_t MoveDownMonitoringChannelByIndex(const size_t channelIndex) = 0;
#pragma endregion Channel list management
};

// converting monitoring interval to text
EXT_NODISCARD inline std::wstring monitoring_interval_to_string(const MonitoringInterval interval)
{
    switch (interval)
    {
    default:
        EXT_ASSERT(false) << "Неизвестный интервал мониторинга";
        [[fallthrough]];
    case MonitoringInterval::eOneDay:
        return L"День";
    case MonitoringInterval::eOneWeek:
        return L"Неделя";
    case MonitoringInterval::eTwoWeeks:
        return L"Две недели";
    case MonitoringInterval::eOneMonth:
        return L"Месяц";
    case MonitoringInterval::eThreeMonths:
        return L"Три месяца";
    case MonitoringInterval::eHalfYear:
        return L"Пол года";
    case MonitoringInterval::eOneYear:
        return L"Год";
    }
}

// conversion of monitoring interval into time interval
EXT_NODISCARD inline CTimeSpan monitoring_interval_to_timespan(const MonitoringInterval interval)
{
    switch (interval)
    {
    default:
        EXT_ASSERT(false) << "Неизвестный интервал мониторинга";
        [[fallthrough]];
    case MonitoringInterval::eOneDay:
        return CTimeSpan(1, 0, 0, 0);
    case MonitoringInterval::eOneWeek:
        return CTimeSpan(7, 0, 0, 0);
    case MonitoringInterval::eTwoWeeks:
        return CTimeSpan(14, 0, 0, 0);
    case MonitoringInterval::eOneMonth:
        return CTimeSpan(30, 0, 0, 0);
    case MonitoringInterval::eThreeMonths:
        return CTimeSpan(91, 0, 0, 0);
    case MonitoringInterval::eHalfYear:
        return CTimeSpan(183, 0, 0, 0);
    case MonitoringInterval::eOneYear:
        return CTimeSpan(365, 0, 0, 0);
    }
}

// convert time span to text
EXT_NODISCARD inline std::wstring time_span_to_string(const CTimeSpan& value)
{
    std::wstring res;

    if (const auto countDays = value.GetDays(); countDays > 0)
    {
        const LONGLONG countHours = (value - CTimeSpan((LONG)countDays, 0, 0, 0)).GetTotalHours();
        if (countHours == 0)
            res = std::string_swprintf(L"%lld дней", countDays);
        else
            res = std::string_swprintf(L"%lld дней %lld часов", countDays, countHours);
    }
    else if (const auto countHours = value.GetTotalHours(); countHours > 0)
    {
        const LONGLONG countMinutes = (value - CTimeSpan(0, (LONG)countHours, 0, 0)).GetTotalMinutes();
        if (countMinutes == 0)
            res = std::string_swprintf(L"%lld часов", countHours);
        else
            res = std::string_swprintf(L"%lld часов %lld минут", countHours, countMinutes);
    }
    else if (const auto countMinutes = value.GetTotalMinutes(); countMinutes > 0)
    {
        const LONGLONG countSeconds = (value - CTimeSpan(0, 0, (LONG)countMinutes, 0)).GetTotalSeconds();
        if (countSeconds == 0)
            res = std::string_swprintf(L"%lld минут", countMinutes);
        else
            res = std::string_swprintf(L"%lld минут %lld секунд", countMinutes, countSeconds);
    }
    else if (const auto countSeconds = value.GetTotalSeconds(); countSeconds > 0)
        res = std::string_swprintf(L"%lld секунд", countSeconds);
    else
        res = L"Нет пропусков";

    EXT_DUMP_IF(res.empty());

    return res;
}

//----------------------------------------------------------------------------//
template <typename... Args>
inline void send_message_to_log(const ILogEvents::LogMessageData::MessageType type, const std::wstring& format, Args&&... textFormat)
{
    // оповещаем о возникшей ошибке
    auto logMessage = std::make_shared<ILogEvents::LogMessageData>();
    logMessage->messageType = type;
    logMessage->logMessage = std::string_swprintf(format.c_str(), std::forward<Args>(textFormat)...);

    ext::send_event_async(&ILogEvents::OnNewLogMessage, logMessage);
}
