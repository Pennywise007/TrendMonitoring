#pragma once

#include <include/IMonitoringTasksService.h>

#include "gmock/gmock.h"

struct MonitoringTaskServiceMock : public IMonitoringTasksService
{
    MOCK_METHOD(TaskId, AddTaskList, (const std::list<std::wstring>& channelNames,
                                      const CTime& intervalStart,
                                      const CTime& intervalEnd,
                                      const TaskPriority priority), (override));
    MOCK_METHOD(TaskId, AddTaskList, (const std::list<TaskParameters::Ptr>& listTaskParams,
                                      const TaskPriority priority), (override));
    MOCK_METHOD(void,   RemoveTask,  (const TaskId& taskId), (override));
};

MATCHER_P(TaskIdComporator, taskId, "CompareTaskIds")
{
    return ext::task::TaskIdHelper::Compare(arg, taskId);
}