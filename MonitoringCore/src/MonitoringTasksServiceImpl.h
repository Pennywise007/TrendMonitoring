#pragma once

/*
    Реализация потока с получением данных из трендов
*/

#include <afx.h>
#include <future>
#include <thread>
#include <list>
#include <map>

#include <Messages.h>
#include "Singleton.h"

#include <ChannelDataCorrector/ChannelDataGetter.h>

#include "IMonitoringTasksService.h"

////////////////////////////////////////////////////////////////////////////////
// сервис для управления потоком получения данных используется для
// управления заданиями монитроинга которые выполняются в отдельном потоке
// результаты выполнения заданий получаются через onCompletingMonitoringTask
class MonitoringTasksServiceImpl : public IMonitoringTasksService
{
    friend class CSingleton<MonitoringTasksServiceImpl>;

public:
    MonitoringTasksServiceImpl();
    virtual ~MonitoringTasksServiceImpl();

// IMonitoringTasksService
public:
    // добавляем список заданий для мониторинга, результат по ним должен быть получен единовременно
    // @param channelNames - список имен каналов для которых нужно произвести мониторинг
    // @param intervalStart - начало интервала по которому надо произвести мониторинг
    // @param intervalEnd - конец интервала по которому надо произвести мониторинг
    // @param priority - приоритет задания, определяет в какой очередности будут выполняться задания
    // @return идентификатор задания
    TaskId addTaskList(const std::list<CString>& channelNames,
                       const CTime& intervalStart,
                       const CTime& intervalEnd,
                       const TaskPriority priority) override;

    // добавляем список заданий для мониторинга, результат по ним должен быть получен единовременно
    // @param listTaskParams - список параметров для задания
    // @param priority - приоритет задания, определяет в какой очередности будут выполняться задания
    // @return идентификатор задания
    TaskId addTaskList(const std::list<TaskParameters::Ptr>& listTaskParams,
                       const TaskPriority priority) override;

    // прервать выполнение задания и удалить из очереди
    void removeTask(const TaskId& taskId);

private:
    // класс с параметрами задания для мониторинга
    class MonitoringTask;
    typedef std::shared_ptr<MonitoringTask> MonitoringTaskPtr;
    // вспомогательный класс для хранения результатов мониторинга
    class MonitoringResultHelper;
private:
    // поток, выполняющий задания для мониторинга
    void tasksExecutorThread();
    // выполнить задание
    void executeTask(MonitoringTaskPtr pMonitoringTask);

private:
    // флаг прерывания работы потока
    std::atomic_bool m_interruptThread = false;

    // поток мониторинга данных
    std::thread m_monitoringTasksExecutorThread;

    // очередь заданий
    std::list<MonitoringTaskPtr> m_queueTasks;

    // мьютекс на очередь заданий, используется в двух случаях
    // 1. Синхронизация действий над m_queueTasks
    // 2. Для m_cvTasks
    std::mutex m_queueMutex;
    // используется при необходимости отработать потоком что-либо
    std::condition_variable m_cvTasks;

    // список идентификаторов заданий и результатов мониторинга
    std::map<TaskId, MonitoringResultHelper, TaskComparer> m_resultsList;
    // мьютекс на список результатов
    std::mutex m_resultsMutex;
};

////////////////////////////////////////////////////////////////////////////////
// задание для мониторинга
class MonitoringTasksServiceImpl::MonitoringTask
{
public:
    MonitoringTask(const TaskParameters::Ptr& taskParameters,
                   const TaskPriority priority,
                   const TaskId& taskId)
        : m_pTaskParams(taskParameters)
        , m_priority(priority)
        , m_taskId(taskId)
    {}

public:
    // параметры задания
    TaskParameters::Ptr m_pTaskParams;
    // приоритет задания
    TaskPriority m_priority;
    // идентификатор задания
    TaskId m_taskId;
};

////////////////////////////////////////////////////////////////////////////////
// вспомогательный класс для хранения результатов мониторинга
class MonitoringTasksServiceImpl::MonitoringResultHelper
{
public:
    MonitoringResultHelper(const size_t taskCount, const TaskId& taskId)
        : m_taskCount(taskCount)
        , m_pMonitoringResult(new MonitoringResult(taskId))
    {}

public:
    // количество заданий которое было запущено
    size_t m_taskCount;
    // результат мониторинга
    MonitoringResult::Ptr m_pMonitoringResult;
};