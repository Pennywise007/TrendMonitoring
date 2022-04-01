#pragma once

#include <mutex>

#include <gtest/gtest.h>

#include <include/IMonitoringTasksService.h>

////////////////////////////////////////////////////////////////////////////////
// �������� ������� � ���������� ������ ����� ����� MonitoringTasksService
class MonitoringTasksTestClass
    : public testing::Test
    , ext::events::ScopeAsyncSubscription<IMonitoringTaskEvent>
{
protected:
    // ��������� ������ (�������������)
    void SetUp() override;

// IMonitoringTaskEvent
public:
    // ���������� � ������������ �������
    void OnCompleteTask(const TaskId& taskId, IMonitoringTaskEvent::ResultsPtrList monitoringResult) override;

protected:
    // ������ ��������� ���� �������� ������� ����������� � m_taskParams
    void fillTaskParams();
    // ������� ���������� �����, ���������� ������ ���� ��������� �������, ���� ���� ��� �������
    bool waitForTaskResult(std::unique_lock<std::mutex>& lock, bool bNoTimeOut);

protected:
    // ������������� �������� �������
    TaskId m_currentTask = {};
    // ��������� �������
    MonitoringResult::Ptr m_taskResult;
    // ��������������� ������ ��� �������� �������� ������
    std::condition_variable m_resultTaskCV;
    std::mutex m_resultMutex;

protected:
    // ��������� ������� �����������
    std::list<TaskParameters::Ptr> m_taskParams;
    // ��� �����
    enum class TestType
    {
        eTestAddTaskList,
        eTestAddTaskParams,
        eTestRemoveTask
    } m_testType = TestType::eTestAddTaskList;
};
