#pragma once

#include <optional>

#include <gtest/gtest.h>

#include <Messages.h>

#include <src/IMonitoringTasksService.h>

////////////////////////////////////////////////////////////////////////////////
// �������� ������� � ���������� ������ ����� ����� MonitoringTasksService
class MonitoringTasksTestClass
    : public testing::Test
    , public EventRecipientImpl
{
protected:
    void SetUp() override;

protected:

// IEventRecipient
public:
    // ���������� � ������������ �������
    void onEvent(const EventId& code, float eventValue,
                 std::shared_ptr<IEventData> eventData) override;

protected:
    // ������ ��������� ���� �������� ������� ����������� � m_taskParams
    void fillTaskParams();

protected:
    // ������������� �������� �������
    TaskId m_currentTask;
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
    } m_testType;
};

