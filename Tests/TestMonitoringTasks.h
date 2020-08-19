#pragma once

#include <mutex>

#include <gtest/gtest.h>

#include <Messages.h>

#include <src/IMonitoringTasksService.h>

////////////////////////////////////////////////////////////////////////////////
// ѕроверка сервиса с получением данных через таски MonitoringTasksService
class MonitoringTasksTestClass
    : public testing::Test
    , public EventRecipientImpl
{
protected:
    // настройка класса (инициализаци€)
    void SetUp() override;

// IEventRecipient
public:
    // оповещение о произошедшем событии
    void onEvent(const EventId& code, float eventValue,
                 std::shared_ptr<IEventData> eventData) override;

protected:
    // задаем параметры всех тестовых каналов мониторинга в m_taskParams
    void fillTaskParams();

protected:
    // идентификатор текущего задани€
    TaskId m_currentTask;
    // результат задани€
    MonitoringResult::Ptr m_taskResult;
    // ¬спомогательный объект дл€ ожидани€ загрузки данных
    std::condition_variable m_resultTaskCV;
    std::mutex m_resultMutex;

protected:
    // параметры задани€ мониторинга
    std::list<TaskParameters::Ptr> m_taskParams;
    // “ип теста
    enum class TestType
    {
        eTestAddTaskList,
        eTestAddTaskParams,
        eTestRemoveTask
    } m_testType;
};

