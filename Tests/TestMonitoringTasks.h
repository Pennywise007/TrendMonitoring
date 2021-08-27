#pragma once

#include <mutex>

#include <gtest/gtest.h>

#include <Messages.h>

#include <include/IMonitoringTasksService.h>

////////////////////////////////////////////////////////////////////////////////
// Проверка сервиса с получением данных через таски MonitoringTasksService
class MonitoringTasksTestClass
    : public testing::Test
    , public EventRecipientImpl
{
protected:
    // настройка класса (инициализация)
    void SetUp() override;

// IEventRecipient
public:
    // оповещение о произошедшем событии
    void onEvent(const EventId& code, float eventValue,
                 const std::shared_ptr<IEventData>& eventData) override;

protected:
    // задаем параметры всех тестовых каналов мониторинга в m_taskParams
    void fillTaskParams();
    // ожидать выполнения теста, возвращает истину если результат получен, ложь если был таймаут
    bool waitForTaskResult(std::unique_lock<std::mutex>& lock, bool bNoTimeOut);

protected:
    // идентификатор текущего задания
    TaskId m_currentTask = {};
    // результат задания
    MonitoringResult::Ptr m_taskResult;
    // Вспомогательный объект для ожидания загрузки данных
    std::condition_variable m_resultTaskCV;
    std::mutex m_resultMutex;

protected:
    // параметры задания мониторинга
    std::list<TaskParameters::Ptr> m_taskParams;
    // Тип теста
    enum class TestType
    {
        eTestAddTaskList,
        eTestAddTaskParams,
        eTestRemoveTask
    } m_testType = TestType::eTestAddTaskList;
};
