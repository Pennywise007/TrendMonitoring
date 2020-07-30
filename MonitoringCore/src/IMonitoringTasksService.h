#pragma once

#include <afx.h>
#include <list>

#include "Messages.h"

#include "ITrendMonitoring.h"

// идентификатор события окончания загрузки данных для канала с мониторингом
// {C6C55763-BC3F-41B0-95D0-D1BC52A149AE}
static const EventId onCompletingMonitoringTask =
{ 0xc6c55763, 0xbc3f, 0x41b0, { 0x95, 0xd0, 0xd1, 0xbc, 0x52, 0xa1, 0x49, 0xae } };

////////////////////////////////////////////////////////////////////////////////
// Идентификатор задания
typedef GUID TaskId;
// Структура для сравнения идентификаторов заданий
struct TaskComparer
{
    bool operator()(const TaskId& Left, const TaskId& Right) const
    {
        // comparison logic goes here
        return memcmp(&Left, &Right, sizeof(Right)) < 0;
    }
};

// Параметры запускаемого задания
struct TaskParameters
{
    typedef std::shared_ptr<TaskParameters> Ptr;

    TaskParameters(const CString& chanName, const CTime& start, const CTime& end)
        : channelName(chanName), startTime(start), endTime(end)
    {}

    CString channelName;        // имя канала по которому нужно запустить мониторинг
    CTime startTime;            // начало интервала мониторинга данных канала
    CTime endTime;              // конец  интервала мониторинга данных каналов
};

////////////////////////////////////////////////////////////////////////////////
// интерфейс сервиса для управления потоком получения данных используется для
// управления заданиями монитроинга которые выполняются в отдельном потоке
// результаты выполнения заданий получаются через onCompletingMonitoringTask
interface IMonitoringTasksService
{
    virtual ~IMonitoringTasksService() = default;

    // приоритетность заданий, должны быть отсортированы в порядке важности
    enum TaskPriority : unsigned
    {
        eHigh,     // высокая
        eNormal    // обычная
    };

    // добавляем список заданий для мониторинга, результат по ним должен быть получен единовременно
    // @param channelNames - список имен каналов для которых нужно произвести мониторинг
    // @param intervalStart - начало интервала по которому надо произвести мониторинг
    // @param intervalEnd - конец интервала по которому надо произвести мониторинг
    // @param priority - приоритет задания, определяет в какой очередности будут выполняться задания
    // @return идентификатор задания
    virtual TaskId addTaskList(const std::list<CString>& channelNames,
                               const CTime& intervalStart,
                               const CTime& intervalEnd,
                               const TaskPriority priority) = 0;

    // добавляем список заданий для мониторинга, результат по ним должен быть получен единовременно
    // @param listTaskParams - список параметров для задания
    // @param priority - приоритет задания, определяет в какой очередности будут выполняться задания
    // @return идентификатор задания
    virtual TaskId addTaskList(const std::list<TaskParameters::Ptr>& listTaskParams,
                               const TaskPriority priority) = 0;

    // прервать выполнение задания и удалить его из очереди
    virtual void removeTask(const TaskId& taskId) = 0;
};

// получение сервиса заданий монитринга
IMonitoringTasksService* get_monitoing_tasks_service();

////////////////////////////////////////////////////////////////////////////////
// результат задания мониторинга
class MonitoringResult
    : public IEventData
{
public:
    typedef std::shared_ptr<MonitoringResult> Ptr;

    MonitoringResult(const TaskId& taskId)
        : m_taskId(taskId) {}

public:
    // Идентификатор задания, по которому получен результат
    TaskId m_taskId;

    // результат запроса данных
    enum class Result
    {
        eSucceeded = 0, // Успешная загрузка
        eErrorText,     // Возникла ошибка, в m_errorText есть её текст
        eNoData         // Нет данных в запрошенном интервале
    };

    // структура с результатом мониторинга
    struct ResultData : public TrendChannelData
    {
        ResultData(const TaskParameters::Ptr params) : pTaskParameters(params) {}

        // результат выполнения задания
        Result resultType = Result::eSucceeded;
        // текст возникшей ошибки
        CString errorText;
        // параметры задания по которым получен результат
        TaskParameters::Ptr pTaskParameters;
    };

    typedef std::list<ResultData> ResultsList;
    typedef std::list<ResultData>::const_iterator ResultIt;

    // список результатов мониторинга для каждого канала из задания
    ResultsList m_taskResults;
};