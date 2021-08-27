#pragma once

#include <include/IMonitoringTasksService.h>

#include "gmock/gmock.h"

struct MonitoringTaskServiceMock : public IMonitoringTasksService
{
    MOCK_METHOD(TaskId, addTaskList, (const std::list<CString>& channelNames,
                                      const CTime& intervalStart,
                                      const CTime& intervalEnd,
                                      const TaskPriority priority), (override));
    MOCK_METHOD(TaskId, addTaskList, (const std::list<TaskParameters::Ptr>& listTaskParams,
                                      const TaskPriority priority), (override));
    MOCK_METHOD(void,   removeTask,  (const TaskId& taskId), (override));
};

MATCHER_P(TaskIdComporator, taskId, "CompareTaskIds")
{
    return TaskComparer::Compare(arg, taskId);
}