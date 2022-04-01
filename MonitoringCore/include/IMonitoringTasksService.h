#pragma once

#include <afx.h>
#include <list>

#include <ext/core/dispatcher.h>
#include <ext/thread/thread_pool.h>

#include <include/ITrendMonitoring.h>

using namespace ext::task;

struct TaskParameters
{
    typedef std::shared_ptr<TaskParameters> Ptr;

    TaskParameters(const std::wstring& chanName, const CTime& start, const CTime& end)
        : channelName(chanName), startTime(start), endTime(end)
    {}

    std::wstring channelName;        // the name of the channel on which you want to start monitoring
    CTime startTime;            // start of channel data monitoring interval
    CTime endTime;              // end   of channel data monitoring interval
};

// service interface to control the flow of receiving data is used to
// control monitoring jobs that run in a separate thread
// task execution results are obtained via IMonitoringTaskEvent
struct IMonitoringTasksService
{
    virtual ~IMonitoringTasksService() = default;

    using TaskPriority = ext::thread_pool::TaskPriority;

    // add a list of tasks for monitoring, the result for them should be received at a time
    // @param channelNames - list of channel names for which you want to monitor
    // @param intervalStart - start of channel data monitoring interval
    // @param intervalEnd - end   of channel data monitoring interval
    // @param priority - task priority determines the order in which jobs will be executed
    // @return task ID
    EXT_NODISCARD virtual TaskId AddTaskList(const std::list<std::wstring>& channelNames,
                                             const CTime& intervalStart,
                                             const CTime& intervalEnd,
                                             const TaskPriority priority) = 0;

    // add a list of tasks for monitoring, the result for them should be received at a time
    // @param listTaskParams - list of parameters for the task
    // @param priority - task priority determines the order in which jobs will be executed
    // @return task id
    EXT_NODISCARD virtual TaskId AddTaskList(const std::list<TaskParameters::Ptr>& listTaskParams,
                                             const TaskPriority  priority) = 0;

    // abort the task and remove it from the queue
    virtual void RemoveTask(const TaskId& taskId) = 0;
};

// Monitoring task notification
struct IMonitoringTaskEvent : ext::events::IBaseEvent
{
    // data query result
    enum class Result
    {
        eSucceeded = 0, // Successful upload
        eErrorText,     // An error occurred, m_errorText has its text
        eNoData         // No data in the requested interval
    };

    // structure with monitoring result
    struct ResultData : public TrendChannelData
    {
        explicit ResultData(const TaskParameters::Ptr& params) EXT_NOEXCEPT : taskParameters(params) {}

        // Task result
        Result resultType = Result::eSucceeded;
        // Error text(if Result == eErrorText)
        std::wstring errorText;
        // task parameters for which the result was obtained
        const TaskParameters::Ptr taskParameters;
    };

    typedef std::shared_ptr<ResultData> ResultDataPtr;
    typedef std::list<ResultDataPtr> ResultsPtrList;

    // Event about finishing monitoring task
    virtual void OnCompleteTask(const TaskId& taskId, ResultsPtrList monitoringResult) = 0;
};
