#pragma once

#include <include/IMonitoringTasksService.h>

#include "ApplicationConfiguration.h"

// ��������� � ����������� �������
struct MonitoringTaskInfo
{
    // ��� ������������ �������
    enum class TaskType
    {
        eIntervalInfo = 0,  // ������ ������ �� ��������� ��������(� �����������)
        eUpdatingInfo,      // ������ ����� ������(���������� ������������ ����������)
        eEveryDayReport     // ������ ������ ��� ����������� ������
    } taskType = TaskType::eIntervalInfo;

    // ��������� ������� �� ������� ������������ �������
    ChannelParametersList channelParameters;
};

struct MonitoringTaskResultHandler
{
    // ��������� ����������� ����������� ��� ������
    // @param monitoringResult - ��������� �����������
    // @param channelParameters - ��������� ������
    // @param alertText - ����� � ������� ����� ��������
    // @return true - � ������ ������������� �������� ������ �� �������
    static bool HandleIntervalInfoResult(const MonitoringTaskInfo::TaskType taskType,
                                         const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                                         ChannelParameters* channelParameters,
                                         std::wstring& alertText);
};
