#pragma once

#include <mutex>

#include <gtest/gtest.h>

#include <ext/core/dependency_injection.h>

#include "helpers/TestHelper.h"
#include "mocks/MonitoringTaskServiceMock.h"

////////////////////////////////////////////////////////////////////////////////
// Проверка сервиса с получением данных через таски MonitoringTasksService
class MonitoringTasksTestClass
    : public testing::Test
    , ext::events::ScopeSubscription<IMonitoringTaskEvent>
{
protected:

    void SetUp() override
    {
        m_serviceProvider = ext::get_service<ext::ServiceCollection>().BuildServiceProvider();
        m_monitoringService = ext::GetInterface<IMonitoringTasksService>(m_serviceProvider);
    }

    void TearDown() override
    {
        ext::get_service<TestHelper>().ResetAll();
    }

// IMonitoringTaskEvent
public:
    // оповещение о произошедшем событии
    void OnCompleteTask(const TaskId& taskId, IMonitoringTaskEvent::ResultsPtrList monitoringResult) override;

protected:
    // задаем параметры всех тестовых каналов мониторинга в m_taskParams
    void fillTaskParams();
    // ожидать выполнения теста, возвращает истину если результат получен, ложь если был таймаут
    bool waitForTaskResult(std::unique_lock<std::mutex>& lock, bool bNoTimeOut);

protected:
    ext::ServiceProvider::Ptr m_serviceProvider;

    std::shared_ptr<IMonitoringTasksService> m_monitoringService;

    // идентификатор текущего задания
    TaskId m_currentTask = {};
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
