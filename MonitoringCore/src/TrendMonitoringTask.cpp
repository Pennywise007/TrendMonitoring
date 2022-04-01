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
    case IMonitoringTaskEvent::Result::eSucceeded:  // ������ ������� ��������
    {
        // ����������� ����� ������ �� �������
        channelParameters->SetTrendChannelData(*monitoringResult);

        channelParameters->channelState.dataLoaded = true;
        channelParameters->channelState.loadingDataError = false;
    }
    break;
    case IMonitoringTaskEvent::Result::eNoData:     // � ���������� ��������� ��� ������
    case IMonitoringTaskEvent::Result::eErrorText:  // �������� ������
    {
        // ��������� � ��������� ������
        if (monitoringResult->resultType == IMonitoringTaskEvent::Result::eErrorText &&
            !monitoringResult->errorText.empty())
            alertText = monitoringResult->errorText;
        else
            alertText = L"��� ������ � ����������� ���������.";

        // ��������� ����� ��� ������
        channelParameters->trendData.emptyDataTime = monitoringResult->emptyDataTime;
        // �������� ��� �������� ������ ��������
        channelParameters->channelState.loadingDataError = true;
    }
    break;
    default:
        EXT_UNREACHABLE("�� ��������� ��� ����������");
        break;
    }

    // ������ ���� ��� ����������
    return true;
}

bool handle_updating_result(const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                            ChannelParameters* channelParameters,
                            std::wstring& alertText)
{
    bool bDataChanged = false;
    switch (monitoringResult->resultType)
    {
    case IMonitoringTaskEvent::Result::eSucceeded:  // ������ ������� ��������
    {
        if (channelParameters->channelState.loadingDataError)
        {
            // ���� ���� ������ ��� �������� ������ ������� ���� ������ � �������� ��� ������ ���������
            channelParameters->channelState.loadingDataError = false;

            // �.�. ������ � ��� ��� ������ �� ������� �������� - ������� ��� ������ ��������
            send_message_to_log(ILogEvents::LogMessageData::MessageType::eOrdinary,
                                L"����� \"%s\": ������ ��������.",
                                channelParameters->channelName.c_str());
        }

        // ���� ������ ��� �� ���� ��������
        if (!channelParameters->channelState.dataLoaded)
        {
            // ����������� ����� ������ �� �������
            channelParameters->SetTrendChannelData(*monitoringResult);
            channelParameters->channelState.dataLoaded = true;
        }
        else
        {
            // ������ ������ � ����� ������
            channelParameters->trendData.currentValue = monitoringResult->currentValue;

            if (channelParameters->trendData.maxValue < monitoringResult->maxValue)
                channelParameters->trendData.maxValue = monitoringResult->maxValue;
            if (channelParameters->trendData.minValue > monitoringResult->minValue)
                channelParameters->trendData.minValue = monitoringResult->minValue;

            channelParameters->trendData.emptyDataTime += monitoringResult->emptyDataTime;
            channelParameters->trendData.lastDataExistTime = monitoringResult->lastDataExistTime;
        }

        // ����������� ����� ������ TODO TESTS
        {
            // ���� ����������� ����������
            if (_finite(channelParameters->alarmingValue) != 0)
            {
                // ���� �� �������� ��� �������� ����� �� ����������
                if ((channelParameters->alarmingValue >= 0 &&
                    monitoringResult->minValue >= channelParameters->alarmingValue) ||
                    (channelParameters->alarmingValue < 0 &&
                    monitoringResult->maxValue <= channelParameters->alarmingValue))
                {
                    channelParameters->channelState.OnAddChannelErrorReport(ChannelStateManager::eExcessOfValue, alertText,
                                                                            L"���������� ���������� ��������. ���������� �������� %.02f, �������� [%.02f..%.02f].",
                                                                            channelParameters->alarmingValue, monitoringResult->minValue, monitoringResult->maxValue);
                }
                else
                {
                    channelParameters->channelState.OnRemoveChannelErrorReport(ChannelStateManager::eExcessOfValue, alertText,
                                                                               L"������ ��������� � �����, ������� �������� [%.02f..%.02f].", monitoringResult->minValue, monitoringResult->maxValue);
                }
            }

            // ��������� ���������� ���������, ��������� ���� ������ ��� ������ ������ ��� �������� ������� ���������� ���������
            if (const auto emptySeconds = monitoringResult->emptyDataTime.GetTotalMinutes();
                emptySeconds > TrendMonitoring::UpdateDataInterval.count() / 2)
            {
                channelParameters->channelState.OnAddChannelErrorReport(ChannelStateManager::eLotOfEmptyData, alertText,
                                                                        L"����� ��������� ������.");
            }
            else
                channelParameters->channelState.OnRemoveChannelErrorReport(ChannelStateManager::eLotOfEmptyData);
        }

        // ���� ������������ ������� ��� ������ ��� � �� �������� - �������� ��������� �������.
        channelParameters->channelState.OnRemoveChannelErrorReport(ChannelStateManager::eFallenOff, alertText, L"������ ��������.");

        // ��������� �� ��������� � ������ ��� �����������
        bDataChanged = true;
    }
    break;
    case IMonitoringTaskEvent::Result::eNoData:     // � ���������� ��������� ��� ������
    case IMonitoringTaskEvent::Result::eErrorText:  // �������� ������
    {
        // ��������� ����� ��� ������
        channelParameters->trendData.emptyDataTime += monitoringResult->emptyDataTime;

        // �������� ��� �������� ������ ��������
        if (!channelParameters->channelState.loadingDataError)
        {
            channelParameters->channelState.loadingDataError = true;

            send_message_to_log(ILogEvents::LogMessageData::MessageType::eOrdinary,
                                L"����� \"%s\": �� ������� �������� ������.",
                                channelParameters->channelName.c_str());
        }

        // ���� �� ������ ���� ��������� ������, � ������ ��������� �� ���������� ������ ��������� ����������
        if (channelParameters->channelState.dataLoaded)
        {
            // ��������� ��������� ������ ���� ��� ���� ��������� ������, ��������� �� ��������� � ������ ��� �����������
            bDataChanged = true;
            channelParameters->channelState.OnAddChannelErrorReport(ChannelStateManager::eFallenOff, alertText, L"������� ������ �� ������.");
        }
    }
    break;
    default:
        EXT_UNREACHABLE("�� ��������� ��� ����������");
        break;
    }

    return bDataChanged;
}

bool handle_every_day_report_result(const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                                    ChannelParameters* channelParameters,
                                    std::wstring& alertText)
{
    // ������ �����������
    const MonitoringChannelData& monitoringData = channelParameters->GetMonitoringData();

    switch (monitoringResult->resultType)
    {
    case IMonitoringTaskEvent::Result::eSucceeded:  // ������ ������� ��������
    {
        // ���� ����������� ���������� ��� ���������� ��������
        if (_finite(monitoringData.alarmingValue) != 0)
        {
            // ���� �� �������� ���� �� �������� ����� �� ����������
            if ((monitoringData.alarmingValue >= 0 &&
                monitoringResult->maxValue >= monitoringData.alarmingValue) ||
                (monitoringData.alarmingValue < 0 &&
                monitoringResult->minValue <= monitoringData.alarmingValue))
            {
                alertText += std::string_swprintf(L"���������� ������� ��� ��������. ���������� �������� %.02f, �������� �� ���� [%.02f..%.02f]. ",
                                       monitoringData.alarmingValue,
                                       monitoringResult->minValue, monitoringResult->maxValue);
            }
        }

        // ���� ����� ��������� ������
        if (monitoringResult->emptyDataTime.GetTotalHours() > 2)
            alertText += std::string_swprintf(L"����� ��������� ������ (%lld �).", monitoringResult->emptyDataTime.GetTotalHours());
    }
    break;
    case IMonitoringTaskEvent::Result::eNoData:     // � ���������� ��������� ��� ������
    case IMonitoringTaskEvent::Result::eErrorText:  // �������� ������
                                                // �������� ��� ������ ���
        alertText = L"��� ������.";
        break;
    default:
        EXT_ASSERT(!"�� ��������� ��� ����������");
        break;
    }

    // ������ ����������� ��� ��������� ������ �����������
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
