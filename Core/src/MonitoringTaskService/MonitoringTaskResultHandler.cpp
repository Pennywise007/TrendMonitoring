#include "pch.h"

#include <ext/core/check.h>

#include "MonitoringTaskResultHandler.h"
#include "src/TrendMonitoring.h"

namespace {

bool handle_interval_info_result(const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                                 ChannelParameters* channelParameters,
                                 std::wstring& alertText)
{
    switch (monitoringResult->resultType)
    {
    case IMonitoringTaskEvent::Result::eSucceeded: // data received successfully
    {
        // insert new data from the task
        channelParameters->SetTrendChannelData(*monitoringResult);

        channelParameters->channelState.dataLoaded = true;
        channelParameters->channelState.loadingDataError = false;
    }
    break;
    case IMonitoringTaskEvent::Result::eNoData: // there is no data in the passed interval
    case IMonitoringTaskEvent::Result::eErrorText: // an error occurred
    {
        // notify about the error
        if (monitoringResult->resultType == IMonitoringTaskEvent::Result::eErrorText &&
            !monitoringResult->errorText.empty())
            alertText = monitoringResult->errorText;
        else
            alertText = L"Нет данных в запрошенном интервале.";

        // update time without data
        channelParameters->trendData.emptyDataTime = monitoringResult->emptyDataTime;
        // report that a download error occurred
        channelParameters->channelState.loadingDataError = true;
    }
    break;
    default:
        EXT_UNREACHABLE("Unknown result type");
        break;
    }

    // there is always something changed
    return true;
}

bool handle_updating_result(const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                            ChannelParameters* channelParameters,
                            std::wstring& alertText)
{
    bool bDataChanged = false;
    switch (monitoringResult->resultType)
    {
    case IMonitoringTaskEvent::Result::eSucceeded:  // данные успешно получены
    {
        if (channelParameters->channelState.loadingDataError)
        {
            // if there was an error when loading data, remove the error flag and report that the data was loaded
            channelParameters->channelState.loadingDataError = false;

            // because wrote that the data could not be updated - we will write that the data was received
            send_message_to_log(ILogEvents::LogMessageData::MessageType::eOrdinary,
                                L"Канал \"%s\": Данные получены.",
                                channelParameters->channelName.c_str());
        }

        // if the data has not been received yet
        if (!channelParameters->channelState.dataLoaded)
        {
            // insert new data from the task
            channelParameters->SetTrendChannelData(*monitoringResult);
            channelParameters->channelState.dataLoaded = true;
        }
        else
        {
            // merge old and new data
            channelParameters->trendData.currentValue = monitoringResult->currentValue;

            if (channelParameters->trendData.maxValue < monitoringResult->maxValue)
                channelParameters->trendData.maxValue = monitoringResult->maxValue;
            if (channelParameters->trendData.minValue > monitoringResult->minValue)
                channelParameters->trendData.minValue = monitoringResult->minValue;

            channelParameters->trendData.emptyDataTime += monitoringResult->emptyDataTime;
            channelParameters->trendData.lastDataExistTime = monitoringResult->lastDataExistTime;
        }

        // parse new TODO TESTS data
        {
            // if notification is set
            if (_finite(channelParameters->alarmingValue) != 0)
            {
                // if during the interval all values are out of range
                if ((channelParameters->alarmingValue >= 0 &&
                    monitoringResult->minValue >= channelParameters->alarmingValue) ||
                    (channelParameters->alarmingValue < 0 &&
                    monitoringResult->maxValue <= channelParameters->alarmingValue))
                {
                    channelParameters->channelState.OnAddChannelErrorReport(ChannelStateManager::eExcessOfValue, alertText,
                                                                            L"Превышение допустимых значений. Допустимое значение %.02f, значения [%.02f..%.02f].",
                                                                            channelParameters->alarmingValue, monitoringResult->minValue, monitoringResult->maxValue);
                }
                else
                {
                    channelParameters->channelState.OnRemoveChannelErrorReport(ChannelStateManager::eExcessOfValue, alertText,
                                                                               L"Данные вернулись в норму, текущие значения [%.02f..%.02f].", monitoringResult->minValue, monitoringResult->maxValue);
                }
            }

            // check the number of gaps, notify if there are more than half of the interval update time without data
            if (const auto emptySeconds = monitoringResult->emptyDataTime.GetTotalMinutes();
                emptySeconds > TrendMonitoring::UpdateDataInterval.count() / 2)
            {
                channelParameters->channelState.OnAddChannelErrorReport(ChannelStateManager::eLotOfEmptyData, alertText,
                                                                        L"Много пропусков данных.");
            }
            else
                channelParameters->channelState.OnRemoveChannelErrorReport(ChannelStateManager::eLotOfEmptyData);
        }

        // if the user was told that there was no data and they were received, we report the good news.
        channelParameters->channelState.OnRemoveChannelErrorReport(ChannelStateManager::eFallenOff, alertText, L"Данные получены.");

        // notify about a change in the monitoring list
        bDataChanged = true;
    }
    break;
    case IMonitoringTaskEvent::Result::eNoData: // there is no data in the passed interval
    case IMonitoringTaskEvent::Result::eErrorText: // an error occurred
    {
        // update time without data
        channelParameters->trendData.emptyDataTime += monitoringResult->emptyDataTime;

        // report that a download error occurred
        if (!channelParameters->channelState.loadingDataError)
        {
            channelParameters->channelState.loadingDataError = true;

            send_message_to_log(ILogEvents::LogMessageData::MessageType::eOrdinary,
                                L"Канал \"%s\": не удалось обновить данные.",
                                channelParameters->channelName.c_str());
        }

        // If data was loaded on the channel, but now it was not possible to load it, then a shutdown has occurred
        if (channelParameters->channelState.dataLoaded)
        {
            bDataChanged = true; // update time without data
            channelParameters->channelState.OnAddChannelErrorReport(ChannelStateManager::eFallenOff, alertText, L"Пропали данные по каналу.");
        }
    }
    break;
    default:
        EXT_UNREACHABLE("Unknown result type");
        break;
    }

    return bDataChanged;
}

bool handle_every_day_report_result(const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                                    ChannelParameters* channelParameters,
                                    std::wstring& alertText)
{
    // monitoring data
    const MonitoringChannelData& monitoringData = channelParameters->GetMonitoringData();

    switch (monitoringResult->resultType)
    {
    case IMonitoringTaskEvent::Result::eSucceeded: // data received successfully
    {
        // if an alert is set when the value is exceeded
        if (_finite(monitoringData.alarmingValue) != 0)
        {
            // if during the interval one of the values is out of range
            if ((monitoringData.alarmingValue >= 0 &&
                monitoringResult->maxValue >= monitoringData.alarmingValue) ||
                (monitoringData.alarmingValue < 0 &&
                monitoringResult->minValue <= monitoringData.alarmingValue))
            {
                alertText += std::string_swprintf(L"Допустимый уровень был превышен. Допустимое значение %.02f, значения за день [%.02f..%.02f]. ",
                                       monitoringData.alarmingValue,
                                       monitoringResult->minValue, monitoringResult->maxValue);
            }
        }

        // if there are many data gaps
        if (monitoringResult->emptyDataTime.GetTotalHours() > 2)
            alertText += std::string_swprintf(L"Много пропусков данных (%lld ч).", monitoringResult->emptyDataTime.GetTotalHours());
    }
    break;
    case IMonitoringTaskEvent::Result::eNoData:     // there is no data in the passed interval
    case IMonitoringTaskEvent::Result::eErrorText:  // an error occurred
        // report that there is no data
        alertText = L"Нет данных.";
        break;
    default:
        EXT_ASSERT(false) << L"Не известный тип результата";
        break;
    }

    // data is updated when other results are received
    return false;
}
} // namespace

bool MonitoringTaskResultHandler::HandleIntervalInfoResult(const MonitoringTaskInfo::TaskType taskType,
                                                           const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                                                           ChannelParameters* channelParameters,
                                                           std::wstring& alertText)
{
    switch (taskType)
    {
    case MonitoringTaskInfo::TaskType::eIntervalInfo:
        return handle_interval_info_result(monitoringResult, channelParameters, alertText);
    case MonitoringTaskInfo::TaskType::eUpdatingInfo:
        return handle_updating_result(monitoringResult, channelParameters, alertText);
    case MonitoringTaskInfo::TaskType::eEveryDayReport:
        return handle_every_day_report_result(monitoringResult, channelParameters, alertText);
    default:
        EXT_ASSERT(false);
        return false;
    }
}
