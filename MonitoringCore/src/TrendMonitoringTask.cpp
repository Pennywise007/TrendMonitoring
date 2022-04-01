#include "pch.h"

#include <ext/core/check.h>

#include "TrendMonitoringTask.h"
#include "TrendMonitoring.h"

namespace {

bool handle_interval_info_result(const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                                 ChannelParameters* channelParameters,
                                 std::wstring& alertText)
{
    switch (monitoringResult->resultType)
    {
    case IMonitoringTaskEvent::Result::eSucceeded:  // данные успешно получены
    {
        // проставляем новые данные из задания
        channelParameters->SetTrendChannelData(*monitoringResult);

        channelParameters->channelState.dataLoaded = true;
        channelParameters->channelState.loadingDataError = false;
    }
    break;
    case IMonitoringTaskEvent::Result::eNoData:     // в переданном интервале нет данных
    case IMonitoringTaskEvent::Result::eErrorText:  // возникла ошибка
    {
        // оповещаем о возникшей ошибке
        if (monitoringResult->resultType == IMonitoringTaskEvent::Result::eErrorText &&
            !monitoringResult->errorText.empty())
            alertText = monitoringResult->errorText;
        else
            alertText = L"Нет данных в запрошенном интервале.";

        // обновляем время без данных
        channelParameters->trendData.emptyDataTime = monitoringResult->emptyDataTime;
        // сообщаем что возникла ошибка загрузки
        channelParameters->channelState.loadingDataError = true;
    }
    break;
    default:
        EXT_UNREACHABLE("Не известный тип результата");
        break;
    }

    // всегда есть что изменилось
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
            // если была ошибка при загрузке данных убираем флаг ошибки и сообщаем что данные загружены
            channelParameters->channelState.loadingDataError = false;

            // т.к. писали о том что данные не удалось обновить - напишем что данные получены
            send_message_to_log(ILogEvents::LogMessageData::MessageType::eOrdinary,
                                L"Канал \"%s\": Данные получены.",
                                channelParameters->channelName.c_str());
        }

        // если данные ещё не были получены
        if (!channelParameters->channelState.dataLoaded)
        {
            // проставляем новые данные из задания
            channelParameters->SetTrendChannelData(*monitoringResult);
            channelParameters->channelState.dataLoaded = true;
        }
        else
        {
            // мержим старые и новые данные
            channelParameters->trendData.currentValue = monitoringResult->currentValue;

            if (channelParameters->trendData.maxValue < monitoringResult->maxValue)
                channelParameters->trendData.maxValue = monitoringResult->maxValue;
            if (channelParameters->trendData.minValue > monitoringResult->minValue)
                channelParameters->trendData.minValue = monitoringResult->minValue;

            channelParameters->trendData.emptyDataTime += monitoringResult->emptyDataTime;
            channelParameters->trendData.lastDataExistTime = monitoringResult->lastDataExistTime;
        }

        // анализируем новые данные TODO TESTS
        {
            // если установлено оповещение
            if (_finite(channelParameters->alarmingValue) != 0)
            {
                // если за интервал все значения вышли за допустимое
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

            // проверяем количество пропусков, оповещаем если секунд без данных больше чем половина времени обновления интервала
            if (const auto emptySeconds = monitoringResult->emptyDataTime.GetTotalMinutes();
                emptySeconds > TrendMonitoring::UpdateDataInterval.count() / 2)
            {
                channelParameters->channelState.OnAddChannelErrorReport(ChannelStateManager::eLotOfEmptyData, alertText,
                                                                        L"Много пропусков данных.");
            }
            else
                channelParameters->channelState.OnRemoveChannelErrorReport(ChannelStateManager::eLotOfEmptyData);
        }

        // если пользователю сказали что данных нет и их получили - сообщаем радостную новость.
        channelParameters->channelState.OnRemoveChannelErrorReport(ChannelStateManager::eFallenOff, alertText, L"Данные получены.");

        // оповещаем об изменении в списке для мониторинга
        bDataChanged = true;
    }
    break;
    case IMonitoringTaskEvent::Result::eNoData:     // в переданном интервале нет данных
    case IMonitoringTaskEvent::Result::eErrorText:  // возникла ошибка
    {
        // обновляем время без данных
        channelParameters->trendData.emptyDataTime += monitoringResult->emptyDataTime;

        // сообщаем что возникла ошибка загрузки
        if (!channelParameters->channelState.loadingDataError)
        {
            channelParameters->channelState.loadingDataError = true;

            send_message_to_log(ILogEvents::LogMessageData::MessageType::eOrdinary,
                                L"Канал \"%s\": не удалось обновить данные.",
                                channelParameters->channelName.c_str());
        }

        // Если по каналу были загружены данные, а сейчас загрузить не получилось значит произошло отключение
        if (channelParameters->channelState.dataLoaded)
        {
            // произошли изменения только если уже были загружены данные, оповещаем об изменении в списке для мониторинга
            bDataChanged = true;
            channelParameters->channelState.OnAddChannelErrorReport(ChannelStateManager::eFallenOff, alertText, L"Пропали данные по каналу.");
        }
    }
    break;
    default:
        EXT_UNREACHABLE("Не известный тип результата");
        break;
    }

    return bDataChanged;
}

bool handle_every_day_report_result(const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                                    ChannelParameters* channelParameters,
                                    std::wstring& alertText)
{
    // данные мониторинга
    const MonitoringChannelData& monitoringData = channelParameters->GetMonitoringData();

    switch (monitoringResult->resultType)
    {
    case IMonitoringTaskEvent::Result::eSucceeded:  // данные успешно получены
    {
        // если установлено оповещение при превышении значения
        if (_finite(monitoringData.alarmingValue) != 0)
        {
            // если за интервал одно из значений вышло за допустимые
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

        // если много пропусков данных
        if (monitoringResult->emptyDataTime.GetTotalHours() > 2)
            alertText += std::string_swprintf(L"Много пропусков данных (%lld ч).", monitoringResult->emptyDataTime.GetTotalHours());
    }
    break;
    case IMonitoringTaskEvent::Result::eNoData:     // в переданном интервале нет данных
    case IMonitoringTaskEvent::Result::eErrorText:  // возникла ошибка
                                                // сообщаем что данных нет
        alertText = L"Нет данных.";
        break;
    default:
        EXT_ASSERT(!"Не известный тип результата");
        break;
    }

    // данные обновляются при получении других результатов
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
