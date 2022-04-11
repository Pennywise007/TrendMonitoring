// тестирование получение данных мониторинга

#include "pch.h"

#include <ext/core/dependency_injection.h>
#include <ext/scope/on_exit.h>

#include <include/ITrendMonitoring.h>

#include "TestMonitoringTasks.h"

#define DONT_TEST_MONITORING_TASKS

#define GTEST_COUT(Text) std::cerr << "[          ] [ INFO ] " << Text << std::endl;

// имя выдуманного канала по которому нет данных
const std::wstring kRandomChannelName = L"Краказябра";

// на самом деле данные только за 07.08.2020, но запросим большой инетрвал
const CTime kExistDataStartTime(2020, 6, 7, 0, 0, 0);
const CTime kExistDataStopTime(2020, 10, 7, 0, 0, 0);


////////////////////////////////////////////////////////////////////////////////
// тестирование сервиса с получением и анализом данных для мониторинга
#ifdef DONT_TEST_MONITORING_TASKS
TEST_F(MonitoringTasksTestClass, DISABLED_AddTaskList)
#else
TEST_F(MonitoringTasksTestClass, AddTaskList)
#endif
{
    m_testType = TestType::eTestAddTaskList;

    // заполняем списки данных для мониторинга
    fillTaskParams();

    // получаем список имён каналов
    std::list<std::wstring> channelNames;
    for (auto& task : m_taskParams)
    {
        channelNames.emplace_back(task->channelName);
        task->startTime = kExistDataStartTime;
        task->endTime   = kExistDataStopTime;
    }

    // запускаем задание мониторинга
    std::unique_lock<std::mutex> lock(m_resultMutex);
    m_currentTask = m_monitoringService->AddTaskList(channelNames, kExistDataStartTime, kExistDataStopTime,
                                                     IMonitoringTasksService::TaskPriority::eHigh);

    // ждём пока придёт результат
    ASSERT_TRUE(waitForTaskResult(lock, true)) << "Были полученны результаты задания которое было отменено";
}

////////////////////////////////////////////////////////////////////////////////
#ifdef DONT_TEST_MONITORING_TASKS
TEST_F(MonitoringTasksTestClass, DISABLED_AddTaskParams)
#else
TEST_F(MonitoringTasksTestClass, AddTaskParams)
#endif
{
    m_testType = TestType::eTestAddTaskParams;

    // заполняем списки данных для мониторинга
    fillTaskParams();

    // запцускаем задание мониторинга
    std::unique_lock<std::mutex> lock(m_resultMutex);
    m_currentTask = m_monitoringService->AddTaskList(m_taskParams, IMonitoringTasksService::TaskPriority::eHigh);

    // ждём пока придёт результат
    ASSERT_TRUE(waitForTaskResult(lock, true)) << "Не удалось получить данные мониторинга за 1 минуту";
}

////////////////////////////////////////////////////////////////////////////////
#ifdef DONT_TEST_MONITORING_TASKS
TEST_F(MonitoringTasksTestClass, DISABLED_RemoveTask)
#else
TEST_F(MonitoringTasksTestClass, RemoveTask)
#endif
{
    m_testType = TestType::eTestRemoveTask;

    // заполняем списки данных для мониторинга
    fillTaskParams();

    // запцускаем задание мониторинга
    std::unique_lock<std::mutex> lock(m_resultMutex);
    m_currentTask = m_monitoringService->AddTaskList(m_taskParams, IMonitoringTasksService::TaskPriority::eHigh);
    m_monitoringService->RemoveTask(m_currentTask);

    // ждём пока придёт результат
    ASSERT_FALSE(waitForTaskResult(lock, false)) << "Не удалось получить данные мониторинга за 1 минуту";
}

//----------------------------------------------------------------------------//
void MonitoringTasksTestClass::OnCompleteTask(const TaskId& taskId, IMonitoringTaskEvent::ResultsPtrList monitoringResult)
{
    if (taskId != m_currentTask)
        return;

    EXT_SCOPE_ON_EXIT_F((cv = &m_resultTaskCV),
    {
        cv->notify_one();
    });

    ASSERT_TRUE(m_testType != TestType::eTestRemoveTask) << "Пришёл результат теста с удалением заданий мониторинга";

    // проверяем что у заданий совпадают параметры с ожидаемыми
    ASSERT_EQ(m_taskParams.size(), monitoringResult.size()) << "Количество результатов не соответсвтует количеству параметров при запуске задания";
    auto taskParamit = m_taskParams.begin(), taskParamitEnd = m_taskParams.end();
    auto taskResultit = monitoringResult.begin(), taskResultEnd = monitoringResult.end();
    for (; taskParamit != taskParamitEnd && taskResultit != taskResultEnd; ++taskParamit, ++taskResultit)
    {
        EXPECT_EQ((*taskParamit)->channelName, (*taskResultit)->taskParameters->channelName);
        EXPECT_EQ((*taskParamit)->startTime, (*taskResultit)->taskParameters->startTime);
        EXPECT_EQ((*taskParamit)->endTime, (*taskResultit)->taskParameters->endTime);
    }

    // проверяем результаты мониторинга
    for (auto& channelResult : monitoringResult)
    {
        if (channelResult->taskParameters->channelName == kRandomChannelName)
        {
            EXPECT_EQ(channelResult->resultType, IMonitoringTaskEvent::Result::eErrorText) << "Получены результаты для не существующего канала";
            continue;
        }

        EXPECT_EQ(channelResult->resultType, IMonitoringTaskEvent::Result::eSucceeded) << "Возникла ошибка при получении данных";

        // текущее имя проверяемого канала для вывода
        std::wstring curChannelName = channelResult->taskParameters->channelName;

        // из-за того что в данных может быть рандом надо учесть сколько времени было в рандоме
        CTimeSpan idialEmptyDataTime(0, 23, 56, 44);
        idialEmptyDataTime += channelResult->taskParameters->endTime - channelResult->taskParameters->startTime -
            (kExistDataStopTime - kExistDataStartTime);

        // у всех одинаковое время записи
        EXPECT_EQ(channelResult->emptyDataTime.GetHours(),   idialEmptyDataTime.GetHours()) << curChannelName;
        EXPECT_EQ(channelResult->emptyDataTime.GetMinutes(), idialEmptyDataTime.GetMinutes()) << curChannelName;
        EXPECT_EQ(channelResult->emptyDataTime.GetSeconds(), idialEmptyDataTime.GetSeconds()) << curChannelName;

        if (channelResult->taskParameters->channelName == L"Прогибометр №1")
        {
            EXPECT_NEAR(channelResult->startValue, -79.19, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->currentValue, -130.95, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->maxValue, 241.9, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->minValue, -317.45, 0.01) << curChannelName;

            EXPECT_EQ(channelResult->lastDataExistTime.Format(L"%d.%m.%Y %H:%M:%S"), L"07.08.2020 15:56:35") << curChannelName;
        }
        else if (channelResult->taskParameters->channelName == L"Прогибометр №2")
        {
            EXPECT_NEAR(channelResult->startValue, 10.76, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->currentValue, -8.63, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->maxValue, 39.8, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->minValue, -35.76, 0.01) << curChannelName;

            EXPECT_EQ(channelResult->lastDataExistTime.Format(L"%d.%m.%Y %H:%M:%S"), L"07.08.2020 15:56:35") << curChannelName;
        }
        else
        {
            EXPECT_EQ(channelResult->taskParameters->channelName, L"Прогибометр №3") <<
                "Неожиданное имя канала";

            EXPECT_NEAR(channelResult->startValue, -0.97, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->currentValue, -0.7, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->maxValue, 0.99, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->minValue, -1, 0.01) << curChannelName;

            EXPECT_EQ(channelResult->lastDataExistTime.Format(L"%d.%m.%Y %H:%M:%S"), L"07.08.2020 15:56:35") << curChannelName;
        }
    }
}

//----------------------------------------------------------------------------//
void MonitoringTasksTestClass::fillTaskParams()
{
    ASSERT_TRUE(m_taskParams.empty()) << "Список параметров не пуст";

    for (const auto& channel : ext::GetInterface<ITrendMonitoring>(m_serviceProvider)->GetNamesOfAllChannels())
    {
        m_taskParams.emplace_back(new TaskParameters(channel,
                                                     kExistDataStartTime - CTimeSpan(0, 1 * std::rand() % 10, 0, 0),
                                                     kExistDataStopTime + CTimeSpan(0, 1 * std::rand() % 10, 0, 0)));
    }

    m_taskParams.emplace_back(new TaskParameters(kRandomChannelName,
                                                 kExistDataStartTime,
                                                 kExistDataStopTime));
}

//----------------------------------------------------------------------------//
bool MonitoringTasksTestClass::waitForTaskResult(std::unique_lock<std::mutex>& lock,
                                                 bool bNoTimeOut)
{
    GTEST_COUT("Ожидание результата задания мониторинга (1 минута).");

    // ждём пока придёт результат, не делаем через std::cv_status т.к. боремся с ложными unblocked spuriously
    return m_resultTaskCV.wait_for(lock, std::chrono::minutes(1),
                                   [bNoTimeOut]{ return bNoTimeOut; });
}
