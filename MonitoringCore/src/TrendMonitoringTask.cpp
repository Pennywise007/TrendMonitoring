#include "pch.h"

#include "TrendMonitoringTask.h"
#include "TrendMonitoring.h"

namespace {

bool handle_interval_info_result(const MonitoringResult::ResultData& monitoringResult,
                                 ChannelParameters* channelParameters,
                                 CString& alertText)
{
    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // ������ ������� ��������
    {
        // ����������� ����� ������ �� �������
        channelParameters->setTrendChannelData(monitoringResult);

        channelParameters->channelState.dataLoaded = true;
        channelParameters->channelState.loadingDataError = false;
    }
    break;
    case MonitoringResult::Result::eNoData:     // � ���������� ��������� ��� ������
    case MonitoringResult::Result::eErrorText:  // �������� ������
    {
        // ��������� � ��������� ������
        if (monitoringResult.resultType == MonitoringResult::Result::eErrorText &&
            !monitoringResult.errorText.IsEmpty())
            alertText = monitoringResult.errorText;
        else
            alertText = L"��� ������ � ����������� ���������.";

        // ��������� ����� ��� ������
        channelParameters->trendData.emptyDataTime = monitoringResult.emptyDataTime;
        // �������� ��� �������� ������ ��������
        channelParameters->channelState.loadingDataError = true;
    }
    break;
    default:
        assert(!"�� ��������� ��� ����������");
        break;
    }

    // ������ ���� ��� ����������
    return true;
}

bool handle_updating_result(const MonitoringResult::ResultData& monitoringResult,
                            ChannelParameters* channelParameters,
                            CString& alertText)
{
    bool bDataChanged = false;
    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // ������ ������� ��������
    {
        if (channelParameters->channelState.loadingDataError)
        {
            // ���� ���� ������ ��� �������� ������ ������� ���� ������ � �������� ��� ������ ���������
            channelParameters->channelState.loadingDataError = false;

            // �.�. ������ � ��� ��� ������ �� ������� �������� - ������� ��� ������ ��������
            send_message_to_log(LogMessageData::MessageType::eOrdinary,
                                L"����� \"%s\": ������ ��������.",
                                channelParameters->channelName.GetString());
        }

        // ���� ������ ��� �� ���� ��������
        if (!channelParameters->channelState.dataLoaded)
        {
            // ����������� ����� ������ �� �������
            channelParameters->setTrendChannelData(monitoringResult);
            channelParameters->channelState.dataLoaded = true;
        }
        else
        {
            // ������ ������ � ����� ������
            channelParameters->trendData.currentValue = monitoringResult.currentValue;

            if (channelParameters->trendData.maxValue < monitoringResult.maxValue)
                channelParameters->trendData.maxValue = monitoringResult.maxValue;
            if (channelParameters->trendData.minValue > monitoringResult.minValue)
                channelParameters->trendData.minValue = monitoringResult.minValue;

            channelParameters->trendData.emptyDataTime += monitoringResult.emptyDataTime;
            channelParameters->trendData.lastDataExistTime = monitoringResult.lastDataExistTime;
        }

        // ����������� ����� ������ TODO TESTS
        {
            // ���� ����������� ����������
            if (_finite(channelParameters->alarmingValue) != 0)
            {
                // ���� �� �������� ��� �������� ����� �� ����������
                if ((channelParameters->alarmingValue >= 0 &&
                    monitoringResult.minValue >= channelParameters->alarmingValue) ||
                    (channelParameters->alarmingValue < 0 &&
                    monitoringResult.maxValue <= channelParameters->alarmingValue))
                {
                    channelParameters->channelState.OnAddChannelErrorReport(ChannelStateManager::eExcessOfValue, alertText,
                                                                            L"���������� ���������� ��������. ���������� �������� %.02f, �������� [%.02f..%.02f].",
                                                                            channelParameters->alarmingValue, monitoringResult.minValue, monitoringResult.maxValue);
                }
                else
                {
                    channelParameters->channelState.OnRemoveChannelErrorReport(ChannelStateManager::eExcessOfValue, alertText,
                                                                               L"������ ��������� � �����, ������� �������� [%.02f..%.02f].", monitoringResult.minValue, monitoringResult.maxValue);
                }
            }

            // ��������� ���������� ���������, ��������� ���� ������ ��� ������ ������ ��� �������� ������� ���������� ���������
            if (const auto emptySeconds = monitoringResult.emptyDataTime.GetTotalMinutes();
                emptySeconds > TrendMonitoring::getUpdateDataInterval().count() / 2)
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
    case MonitoringResult::Result::eNoData:     // � ���������� ��������� ��� ������
    case MonitoringResult::Result::eErrorText:  // �������� ������
    {
        // ��������� ����� ��� ������
        channelParameters->trendData.emptyDataTime += monitoringResult.emptyDataTime;

        // �������� ��� �������� ������ ��������
        if (!channelParameters->channelState.loadingDataError)
        {
            channelParameters->channelState.loadingDataError = true;

            send_message_to_log(LogMessageData::MessageType::eOrdinary,
                                L"����� \"%s\": �� ������� �������� ������.",
                                channelParameters->channelName.GetString());
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
        assert(!"�� ��������� ��� ����������");
        break;
    }

    return bDataChanged;
}

bool handle_every_day_report_result(const MonitoringResult::ResultData& monitoringResult,
                                    ChannelParameters* channelParameters,
                                    CString& alertText)
{
    // ������ �����������
    const MonitoringChannelData& monitoringData = channelParameters->getMonitoringData();

    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // ������ ������� ��������
    {
        // ���� ����������� ���������� ��� ���������� ��������
        if (_finite(monitoringData.alarmingValue) != 0)
        {
            // ���� �� �������� ���� �� �������� ����� �� ����������
            if ((monitoringData.alarmingValue >= 0 &&
                monitoringResult.maxValue >= monitoringData.alarmingValue) ||
                (monitoringData.alarmingValue < 0 &&
                monitoringResult.minValue <= monitoringData.alarmingValue))
            {
                alertText.AppendFormat(L"���������� ������� ��� ��������. ���������� �������� %.02f, �������� �� ���� [%.02f..%.02f]. ",
                                       monitoringData.alarmingValue,
                                       monitoringResult.minValue, monitoringResult.maxValue);
            }
        }

        // ���� ����� ��������� ������
        if (monitoringResult.emptyDataTime.GetTotalHours() > 2)
            alertText.AppendFormat(L"����� ��������� ������ (%lld �).",
                                   monitoringResult.emptyDataTime.GetTotalHours());
    }
    break;
    case MonitoringResult::Result::eNoData:     // � ���������� ��������� ��� ������
    case MonitoringResult::Result::eErrorText:  // �������� ������
                                                // �������� ��� ������ ���
        alertText = L"��� ������.";
        break;
    default:
        assert(!"�� ��������� ��� ����������");
        break;
    }

    // ������ ����������� ��� ��������� ������ �����������
    return false;
}
} // namespace

bool MonitoringTaskResultHandler::HandleIntervalInfoResult(const MonitoringTaskInfo::TaskType taskType,
                                                           const MonitoringResult::ResultData& monitoringResult,
                                                           ChannelParameters* channelParameters,
                                                           CString& alertText)
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
        assert(false);
        return false;
    }
}
