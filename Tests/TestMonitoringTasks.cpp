// ������������ ��������� ������ �����������

#include "pch.h"

#include <ext/scope/on_exit.h>

#include <include/ITrendMonitoring.h>

#include "TestMonitoringTasks.h"

#define DONT_TEST_MONITORING_TASKS

#define GTEST_COUT(Text) std::cerr << "[          ] [ INFO ] " << Text << std::endl;

// ��� ����������� ������ �� �������� ��� ������
const CString kRandomChannelName = L"����������";

// �� ����� ���� ������ ������ �� 07.08.2020, �� �������� ������� ��������
const CTime kExistDataStartTime(2020, 6, 7, 0, 0, 0);
const CTime kExistDataStopTime(2020, 10, 7, 0, 0, 0);


////////////////////////////////////////////////////////////////////////////////
// ������������ ������� � ���������� � �������� ������ ��� �����������
#ifdef DONT_TEST_MONITORING_TASKS
TEST_F(MonitoringTasksTestClass, DISABLED_AddTaskList)
#else
TEST_F(MonitoringTasksTestClass, AddTaskList)
#endif
{
    m_testType = TestType::eTestAddTaskList;

    // ��������� ������ ������ ��� �����������
    fillTaskParams();

    // �������� ������ ��� �������
    std::list<CString> channelNames;
    for (auto& task : m_taskParams)
    {
        channelNames.emplace_back(task->channelName);
        task->startTime = kExistDataStartTime;
        task->endTime   = kExistDataStopTime;
    }

    // ��������� ������� ������� ����������
    IMonitoringTasksService* pMonitoringService = GetInterface<IMonitoringTasksService>();

    // ��������� ������� �����������
    std::unique_lock<std::mutex> lock(m_resultMutex);
    m_currentTask = pMonitoringService->AddTaskList(channelNames, kExistDataStartTime, kExistDataStopTime,
                                                    IMonitoringTasksService::eHigh);

    // ��� ���� ����� ���������
    ASSERT_TRUE(waitForTaskResult(lock, true)) << "���� ��������� ���������� ������� ������� ���� ��������";
}

////////////////////////////////////////////////////////////////////////////////
#ifdef DONT_TEST_MONITORING_TASKS
TEST_F(MonitoringTasksTestClass, DISABLED_AddTaskParams)
#else
TEST_F(MonitoringTasksTestClass, AddTaskParams)
#endif
{
    m_testType = TestType::eTestAddTaskParams;

    // ��������� ������ ������ ��� �����������
    fillTaskParams();

    // ��������� ������� ������� ����������
    IMonitoringTasksService* pMonitoringService = GetInterface<IMonitoringTasksService>();

    // ���������� ������� �����������
    std::unique_lock<std::mutex> lock(m_resultMutex);
    m_currentTask = pMonitoringService->AddTaskList(m_taskParams, IMonitoringTasksService::eHigh);

    // ��� ���� ����� ���������
    ASSERT_TRUE(waitForTaskResult(lock, true)) << "�� ������� �������� ������ ����������� �� 1 ������";
}

////////////////////////////////////////////////////////////////////////////////
#ifdef DONT_TEST_MONITORING_TASKS
TEST_F(MonitoringTasksTestClass, DISABLED_RemoveTask)
#else
TEST_F(MonitoringTasksTestClass, RemoveTask)
#endif
{
    m_testType = TestType::eTestRemoveTask;

    // ��������� ������ ������ ��� �����������
    fillTaskParams();

    // ��������� ������� ������� ����������
    IMonitoringTasksService* pMonitoringService = GetInterface<IMonitoringTasksService>();

    // ���������� ������� �����������
    std::unique_lock<std::mutex> lock(m_resultMutex);
    m_currentTask = pMonitoringService->AddTaskList(m_taskParams, IMonitoringTasksService::eHigh);
    pMonitoringService->RemoveTask(m_currentTask);

    // ��� ���� ����� ���������
    ASSERT_FALSE(waitForTaskResult(lock, false)) << "�� ������� �������� ������ ����������� �� 1 ������";
}

////////////////////////////////////////////////////////////////////////////////
void MonitoringTasksTestClass::SetUp()
{
}

//----------------------------------------------------------------------------//
void MonitoringTasksTestClass::OnCompleteTask(const TaskId& taskId, IMonitoringTaskEvent::ResultsPtrList monitoringResult)
{
    if (taskId != m_currentTask)
        return;

    EXT_SCOPE_ON_EXIT_F((&m_resultTaskCV),
    {
        m_resultTaskCV.notify_one();
    });

    ASSERT_TRUE(m_testType != TestType::eTestRemoveTask) << "������ ��������� ����� � ��������� ������� �����������";

    // ��������� ��� � ������� ��������� ��������� � ����������
    ASSERT_EQ(m_taskParams.size(), monitoringResult.size()) << "���������� ����������� �� ������������� ���������� ���������� ��� ������� �������";
    auto taskParamit = m_taskParams.begin(), taskParamitEnd = m_taskParams.end();
    auto taskResultit = monitoringResult.begin(), taskResultEnd = monitoringResult.end();
    for (; taskParamit != taskParamitEnd && taskResultit != taskResultEnd; ++taskParamit, ++taskResultit)
    {
        EXPECT_EQ((*taskParamit)->channelName, (*taskResultit)->taskParameters->channelName);
        EXPECT_EQ((*taskParamit)->startTime, (*taskResultit)->taskParameters->startTime);
        EXPECT_EQ((*taskParamit)->endTime, (*taskResultit)->taskParameters->endTime);
    }

    // ��������� ���������� �����������
    for (auto& channelResult : monitoringResult)
    {
        if (channelResult->taskParameters->channelName == kRandomChannelName)
        {
            EXPECT_EQ(channelResult->resultType, IMonitoringTaskEvent::Result::eErrorText) << "�������� ���������� ��� �� ������������� ������";
            continue;
        }

        EXPECT_EQ(channelResult->resultType, IMonitoringTaskEvent::Result::eSucceeded) << "�������� ������ ��� ��������� ������";

        // ������� ��� ������������ ������ ��� ������
        CStringA curChannelName = channelResult->taskParameters->channelName;

        // ��-�� ���� ��� � ������ ����� ���� ������ ���� ������ ������� ������� ���� � �������
        CTimeSpan idialEmptyDataTime(0, 23, 56, 44);
        idialEmptyDataTime += channelResult->taskParameters->endTime - channelResult->taskParameters->startTime -
            (kExistDataStopTime - kExistDataStartTime);

        // � ���� ���������� ����� ������
        EXPECT_EQ(channelResult->emptyDataTime.GetHours(),   idialEmptyDataTime.GetHours()) << curChannelName;
        EXPECT_EQ(channelResult->emptyDataTime.GetMinutes(), idialEmptyDataTime.GetMinutes()) << curChannelName;
        EXPECT_EQ(channelResult->emptyDataTime.GetSeconds(), idialEmptyDataTime.GetSeconds()) << curChannelName;

        if (channelResult->taskParameters->channelName == L"����������� �1")
        {
            EXPECT_NEAR(channelResult->startValue, -79.19, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->currentValue, -130.95, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->maxValue, 241.9, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->minValue, -317.45, 0.01) << curChannelName;

            EXPECT_EQ(channelResult->lastDataExistTime = std::string_swprintf(L"%d.%m.%Y %H:%M:%S"), L"07.08.2020 15:56:35") << curChannelName;
        }
        else if (channelResult->taskParameters->channelName == L"����������� �2")
        {
            EXPECT_NEAR(channelResult->startValue, 10.76, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->currentValue, -8.63, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->maxValue, 39.8, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->minValue, -35.76, 0.01) << curChannelName;

            EXPECT_EQ(channelResult->lastDataExistTime = std::string_swprintf(L"%d.%m.%Y %H:%M:%S"), L"07.08.2020 15:56:35") << curChannelName;
        }
        else
        {
            EXPECT_EQ(channelResult->taskParameters->channelName, L"����������� �3") <<
                "����������� ��� ������";

            EXPECT_NEAR(channelResult->startValue, -0.97, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->currentValue, -0.7, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->maxValue, 0.99, 0.01) << curChannelName;
            EXPECT_NEAR(channelResult->minValue, -1, 0.01) << curChannelName;

            EXPECT_EQ(channelResult->lastDataExistTime = std::string_swprintf(L"%d.%m.%Y %H:%M:%S"), L"07.08.2020 15:56:35") << curChannelName;
        }
    }
}

//----------------------------------------------------------------------------//
void MonitoringTasksTestClass::fillTaskParams()
{
    ASSERT_TRUE(m_taskParams.empty()) << "������ ���������� �� ����";

    for (const auto& channel : GetInterface<ITrendMonitoring>()->GetNamesOfAllChannels())
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
    GTEST_COUT("�������� ���������� ������� ����������� (1 ������).");

    // ��� ���� ����� ���������, �� ������ ����� std::cv_status �.�. ������� � ������� unblocked spuriously
    return m_resultTaskCV.wait_for(lock, std::chrono::minutes(1),
                                   [bNoTimeOut]{ return bNoTimeOut; });
}
