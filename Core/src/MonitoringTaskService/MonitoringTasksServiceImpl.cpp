#include "pch.h"

#include <cassert>
#include <memory>

#include <ext/trace/tracer.h>

#include "MonitoringTasksServiceImpl.h"

#include "ChannelDataGetter/ChannelDataGetter.h"

#include <mutex>
#include <ext/core/dependency_injection.h>
#include <ext/std/string.h>

// enable verbose logging
#define DETAILED_LOGGING

MonitoringTasksServiceImpl::MonitoringTasksServiceImpl()
    : m_threadPool(nullptr, 1)
{}

TaskId MonitoringTasksServiceImpl::AddTaskList(const std::list<std::wstring>& channelNames,
                                               const CTime& intervalStart,
                                               const CTime& intervalEnd,
                                               const TaskPriority priority)
{
    // list of job parameters
    std::list<TaskParameters::Ptr> listTaskParams;

    // remember the end of the interval for which the data was loaded
    for (const auto& channelName : channelNames)
    {
        listTaskParams.emplace_back(new TaskParameters(channelName, intervalStart, intervalEnd));
    }

    return AddTaskList(listTaskParams, priority);
}

TaskId MonitoringTasksServiceImpl::AddTaskList(const std::list<TaskParameters::Ptr>& listTaskParams,
                                               const TaskPriority priority)
{
#ifdef DETAILED_LOGGING
    EXT_TRACE_SCOPE() << EXT_TRACE_FUNCTION;
#endif // DETAILED_LOGGING

    EXT_EXPECT(!listTaskParams.empty()) << EXT_TRACE_FUNCTION << "Empty parameters list";

    const auto taskId = TaskIdHelper::Create();

    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        EXT_EXPECT(m_resultsList.try_emplace(taskId, listTaskParams.size()).second) << EXT_TRACE_FUNCTION "Failed to emplace task id";
    }

    for (auto taskParams : listTaskParams)
    {
        m_threadPool.add_task_by_id(taskId, [&, taskId, taskParams]()
        {
            IMonitoringTaskEvent::ResultDataPtr result = ExecuteTask(taskParams);
            if (!result) // interruption
            {
                EXT_EXPECT(ext::this_thread::interruption_requested()) << EXT_TRACE_FUNCTION << "Result empty but thread not interrupted";
                return;
            }
            EXT_ASSERT(taskParams->channelName == result->taskParameters->channelName) << "Get data for wrong channel name!";

            // collect all results for task
            std::lock_guard<std::mutex> lock(m_resultsMutex);
            auto it = m_resultsList.find(taskId);
            if (it == m_resultsList.end())
                // task might be deleted
                return;

            auto& taskResults = it->second.results;
            taskResults.emplace_back(std::move(result));

            // if we have completed all the tasks - notify about it
            if (taskResults.size() == it->second.taskCount)
            {
                // notification of the completion of the task
                ext::send_event_async(&IMonitoringTaskEvent::OnCompleteTask, taskId, std::move(taskResults));
                // remove a task from the local list of tasks
                m_resultsList.erase(it);
            }
        }, priority);
    }

#ifdef DETAILED_LOGGING
    {
        // collect information for logging
        std::wstring channelsInfo;
        for (const auto& taskParams : listTaskParams)
        {
            channelsInfo = std::string_swprintf(L" channel %s, interval %s - %s;",
                                                taskParams->channelName.c_str(),
                                                taskParams->startTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString(),
                                                taskParams->endTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString());
        }

        EXT_TRACE() << EXT_TRACE_FUNCTION << std::string_swprintf(L"Loading data %s. TaskId = %s",
            channelsInfo.c_str(),
            std::wstring(CComBSTR(taskId)).c_str()).c_str();
    }
#endif // DETAILED_LOGGING

    return taskId;
}

void MonitoringTasksServiceImpl::RemoveTask(const TaskId& taskId)
{
#ifdef DETAILED_LOGGING
    EXT_TRACE() << EXT_TRACE_FUNCTION << std::string_swprintf(L"TaskId = %s", std::wstring(CComBSTR(taskId)).c_str()).c_str();
#endif // DETAILED_LOGGING

    m_threadPool.erase_task(taskId);

    // removing from results list
    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        m_resultsList.erase(taskId);
    }
}

IMonitoringTaskEvent::ResultDataPtr MonitoringTasksServiceImpl::ExecuteTask(const TaskParameters::Ptr& taskParams)
{
#ifdef DETAILED_LOGGING
    EXT_TRACE_SCOPE() << EXT_TRACE_FUNCTION;
#endif // DETAILED_LOGGING

    EXT_ASSERT(!!taskParams);
    if (!taskParams)
        return nullptr;

    IMonitoringTaskEvent::ResultDataPtr taskResult = std::make_shared<IMonitoringTaskEvent::ResultData>(taskParams);

    // параметры задания
    const std::wstring& channelName = taskParams->channelName;
    const CTime& startTime  = taskParams->startTime;
    const CTime& endTime    = taskParams->endTime;

#ifdef DETAILED_LOGGING
    EXT_TRACE() << EXT_TRACE_FUNCTION << std::string_swprintf(L"Загрузка данных по каналу %s, интервал %s - %s.",
                    channelName.c_str(),
                    startTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString(),
                    endTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString()).c_str();
#endif // DETAILED_LOGGING

    // error message
    std::wstring exceptionMessage;
    try
    {
        // create a class to receive data on the channel
        ChannelDataGetter channelDataGetter(channelName.c_str(), startTime, endTime);
        // channel frequency, we assume that it is constant
        double frequency = channelDataGetter.getChannelInfo()->GetFrequency();

        // we will load in portions, a maximum of 10,000,000 points
        // for a channel with a frequency of 10, this is 11.5 days, or ~40 MB of memory
        const double maxPartSize = 1e7;

        bool bDataLoaded = false;

        // channel data
        std::vector<float> channelData;

        // last loaded time per channel
        CTime lastLoadedTime = startTime;
        while (lastLoadedTime != endTime && !ext::this_thread::interruption_requested())
        {
            // time period left to load
            CTimeSpan allTimeSpan = endTime - lastLoadedTime;

            // count the number of seconds we can load
            const LONGLONG loadingSeconds = std::min<LONGLONG>(allTimeSpan.GetTotalSeconds(), LONGLONG(maxPartSize / frequency));

            try
            {
                // load the data of the current portion
                channelDataGetter.getSourceChannelData(lastLoadedTime, lastLoadedTime + loadingSeconds, channelData);

                // number of empty values
                LONGLONG emptyValuesCount = 0;

                // Index in data and index of last non-null data
                LONGLONG index = 0, lastNotEmptyValueIndex = -1;

                // parse the data
                // do it via : because there is a lot of data and accessing it via [] will take a lot of time
                for (const auto& value : channelData)
                {
                    // break if the thread is interrupted
                    if (ext::this_thread::interruption_requested())
                        return nullptr;

                    // absolute zero is considered a data gap
                    if (abs(value) > FLT_MIN)
                    {
                        if (!bDataLoaded)
                        {
                            // initialize with values
                            taskResult->startValue = value;
                            taskResult->minValue = value;
                            taskResult->maxValue = value;

                            bDataLoaded = true;
                        }
                        else
                        {
                            if (taskResult->minValue > value)
                                taskResult->minValue = value;
                            if (taskResult->maxValue < value)
                                taskResult->maxValue = value;
                        }

                        // remember the index of the last non-null data
                        lastNotEmptyValueIndex = index;
                    }
                    else
                        emptyValuesCount += 1;

                    ++index; // separately calculate the current index in the data
                }

#ifdef DETAILED_LOGGING
                EXT_TRACE() << EXT_TRACE_FUNCTION << std::string_swprintf(
                    L"Данные канала %s(частота %.02lf) загружены в промежутке %s - %s. Текущее количество пропусков %lld - новое  %lld. Всего данных %u, пустых %u.",
                    channelName.c_str(), frequency,
                    lastLoadedTime.Format(L"%d.%m.%Y").GetString(),
                    (lastLoadedTime + loadingSeconds).Format(L"%d.%m.%Y").GetString(),
                    taskResult->emptyDataTime,
                    emptyValuesCount,
                    channelData.size(),
                    std::count_if(channelData.begin(), channelData.end(),
                        [](const auto& val)
                        {
                            return abs(val) <= FLT_MIN;
                        })).c_str();
        #endif // DETAILED_LOGGING

                // if there was non-empty data
                if (lastNotEmptyValueIndex != -1)
                {
                    // remember the last non-empty value
                    taskResult->currentValue = channelData[(UINT)lastNotEmptyValueIndex];

                    // find the time of the last non-empty value
                    taskResult->lastDataExistTime =
                        lastLoadedTime + CTimeSpan(__time64_t(lastNotEmptyValueIndex / frequency));
                }

                // calculate the number of seconds without data
                taskResult->emptyDataTime += LONGLONG(emptyValuesCount / frequency);
            }
            catch (const std::exception& exception)
            {
                // in some interval there may be no data at all, mark them empty
                taskResult->emptyDataTime += loadingSeconds;

#ifdef DETAILED_LOGGING
                EXT_TRACE() << EXT_TRACE_FUNCTION << std::string_swprintf(L"Возникла ошибка при получении данных. Текущее количество пропусков %lld. Ошибка: %s",
                                taskResult->emptyDataTime,
                                std::widen(exception.what()).c_str()).c_str();
#endif // DETAILED_LOGGING
            }

            // update last loaded time
            lastLoadedTime += loadingSeconds;
        }

        // if no data was loaded - report it
        if (!bDataLoaded)
            taskResult->resultType = IMonitoringTaskEvent::Result::eNoData;
    }
    catch (CException*)
    {
        exceptionMessage = ext::ManageExceptionText(L"Failed to load data");
    }
    catch (const std::exception&)
    {
        exceptionMessage = ext::ManageExceptionText(L"Failed to load data");
    }

    // If there was an error message, show it
    if (!exceptionMessage.empty())
    {
        taskResult->errorText = std::string_swprintf(
            L"Возникла ошибка при получении данных канала \"%s\" в промежутке %s - %s.\n%s",
            channelName.c_str(),
            startTime.Format(L"%d.%m.%Y").GetString(),
            endTime.Format(L"%d.%m.%Y").GetString(),
            exceptionMessage.c_str());

        // report that there is an error text
        taskResult->resultType = IMonitoringTaskEvent::Result::eErrorText;

        // if an exception occurs, the data cannot be trusted - we consider that the entire interval is empty
        taskResult->emptyDataTime = endTime - startTime;
    }

#ifdef DETAILED_LOGGING
    std::wstring loadingResult;
    switch (taskResult->resultType)
    {
    case IMonitoringTaskEvent::Result::eSucceeded:
        loadingResult = std::string_swprintf(L"Данные получены, curVal = %.02f minVal = %.02f maxVal = %.02f, emptyDataSeconds = %lld",
                             taskResult->currentValue, taskResult->minValue, taskResult->maxValue,
                             taskResult->emptyDataTime.GetTotalSeconds());
        break;
    case IMonitoringTaskEvent::Result::eErrorText:
        loadingResult = taskResult->errorText;
        break;
    case IMonitoringTaskEvent::Result::eNoData:
        loadingResult = L"Данных нет";
        break;
    default:
        loadingResult = L"Не известный тип результата!";
        EXT_ASSERT(false);
        break;
    }

    EXT_TRACE() << EXT_TRACE_FUNCTION << std::string_swprintf(L"Загрузка данных по каналу %s завершена. Результат : %s",
                           channelName.c_str(), loadingResult.c_str()).c_str();
#endif // DETAILED_LOGGING

    return taskResult;
}
