#pragma once

/*
    Реализация потока с получением данных из трендов
*/

#include <afx.h>
#include <future>
#include <list>
#include <map>

#include <include/IMonitoringTasksService.h>

#include <ext/core/noncopyable.h>
#include <ext/thread/thread_pool.h>

// service interface to control the flow of receiving data is used to
// control monitoring jobs that run in a separate thread
// task execution results are obtained via IMonitoringTaskEvent
class MonitoringTasksServiceImpl final
    : public IMonitoringTasksService
    , ext::NonCopyable
{
public:
    MonitoringTasksServiceImpl();

// IMonitoringTasksService
public:
    // add a list of tasks for monitoring, the result for them should be received at a time
    // @param channelNames - list of channel names for which you want to monitor
    // @param intervalStart - start of channel data monitoring interval
    // @param intervalEnd - end   of channel data monitoring interval
    // @param priority - task priority determines the order in which jobs will be executed
    // @return task ID
    TaskId AddTaskList(const std::list<std::wstring>& channelNames,
                       const CTime& intervalStart,
                       const CTime& intervalEnd,
                       const TaskPriority priority) override;

    // add a list of tasks for monitoring, the result for them should be received at a time
    // @param listTaskParams - list of parameters for the task
    // @param priority - task priority determines the order in which jobs will be executed
    // @return task id
    TaskId AddTaskList(const std::list<TaskParameters::Ptr>& listTaskParams,
                       const TaskPriority priority) override;

    // abort the task and remove it from the queue
    void RemoveTask(const TaskId& taskId) override;

private:
    // helper class for storing monitoring results
    struct  MonitoringResultHelper;
private:
    // execution function
    static IMonitoringTaskEvent::ResultDataPtr ExecuteTask(const TaskParameters::Ptr& taskParams);

private:
    // list of task IDs and monitoring results
    std::map<TaskId, MonitoringResultHelper, TaskIdHelper> m_resultsList;
    // results list mutex
    std::mutex m_resultsMutex;

    // task thread pool
    ext::thread_pool m_threadPool;
};

// helper class for storing monitoring results
struct MonitoringTasksServiceImpl::MonitoringResultHelper
{
    explicit MonitoringResultHelper(const size_t taskCount) EXT_NOEXCEPT
        : taskCount(taskCount)
    {}

    // the number of jobs that have been run
    const size_t taskCount;
    // monitoring result
    IMonitoringTaskEvent::ResultsPtrList results;
};