#pragma once

#include <include/IMonitoringTasksService.h>

#include "src/ApplicationConfiguration.h"

// structure with job parameters¤
struct MonitoringTaskInfo
{
     // type of task being executed
     enum class TaskType
     {
         eIntervalInfo = 0, // data request for the specified interval (with overwriting)
         eUpdatingInfo,     // requesting new data (updating existing information)
         eEveryDayReport    // request data for a daily report
     } taskType = TaskType::eIntervalInfo;

     // parameters of the channels through which the task is performed
     ChannelParametersList channelParameters;
};

struct MonitoringTaskResultHandler
{
    // Processing monitoring results for a channel
    // @param monitoringResult - monitoring result
    // @param channelParameters - channel parameters
    // @param alertText - text to report
    // @return true - update data by channels if necessary
    static bool HandleIntervalInfoResult(const MonitoringTaskInfo::TaskType taskType,
                                         const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                                         ChannelParameters* channelParameters,
                                         std::wstring& alertText);
};
