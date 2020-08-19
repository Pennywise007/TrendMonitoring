#include "pch.h"

#include <assert.h>
#include <memory>

#include "Logger.h"
#include "MonitoringTasksServiceImpl.h"

// включение подробного логирования
#define DETAILED_LOGGING

////////////////////////////////////////////////////////////////////////////////
IMonitoringTasksService* get_monitoing_tasks_service()
{
    return &get_service<MonitoringTasksServiceImpl>();
}

//----------------------------------------------------------------------------//
MonitoringTasksServiceImpl::MonitoringTasksServiceImpl()
{
    // делаем не через конструктор чтобы все переменные инициализировались
    m_monitoringTasksExecutorThread = std::thread(std::bind(&MonitoringTasksServiceImpl::tasksExecutorThread, this));
}

//----------------------------------------------------------------------------//
MonitoringTasksServiceImpl::~MonitoringTasksServiceImpl()
{
    // взводим флаг прерывания потока
    m_interruptThread = true;
    // оповещаем о новом событии
    m_cvTasks.notify_all();
    // ждем пока поток прервется
    m_monitoringTasksExecutorThread.join();
}

//----------------------------------------------------------------------------//
// IMonitoringTasksService
TaskId MonitoringTasksServiceImpl::addTaskList(const std::list<CString>& channelNames,
                                               const CTime& intervalStart,
                                               const CTime& intervalEnd,
                                               const TaskPriority priority)
{
    // список параметров заданий
    std::list<TaskParameters::Ptr> listTaskParams;

    // запоминаем конец интервала по которому были загружены данные
    for (auto& channelName : channelNames)
    {
        listTaskParams.emplace_back(new TaskParameters(channelName, intervalStart, intervalEnd));
    }

    return addTaskList(listTaskParams, priority);
}

//----------------------------------------------------------------------------//
// IMonitoringTasksService
TaskId MonitoringTasksServiceImpl::addTaskList(const std::list<TaskParameters::Ptr>& listTaskParams,
                                               const TaskPriority priority)
{
#ifdef DETAILED_LOGGING
    OUTPUT_LOG_FUNC_ENTER;
#endif // DETAILED_LOGGING

    // генерим идентификатор задания
    TaskId newTaskId;
    if (!SUCCEEDED(CoCreateGuid(&newTaskId)))
        assert(!"Не удалось создать гуид!");

    // инициализируем в списке результатов
    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        m_resultsList.try_emplace(newTaskId, listTaskParams.size(), newTaskId);
    }

    {
        // вставляем в очередь заданий
        std::lock_guard<std::mutex> lock(m_queueMutex);

        // итератор куда будем вставлять
        decltype(m_queueTasks)::iterator insertIter =
            std::find_if(m_queueTasks.begin(), m_queueTasks.end(),
                         [priority](const MonitoringTaskPtr& task)
                         {
                             return task->m_priority > priority;
                         });

        for (const auto& taskParams : listTaskParams)
        {
            insertIter = ++m_queueTasks.emplace(insertIter, new MonitoringTask(taskParams,
                                                                               priority,
                                                                               newTaskId));
        }

#ifdef DETAILED_LOGGING
        // собираем информацию для логирования
        CString channelsInfo;
        for (const auto& taskParams : listTaskParams)
        {
            channelsInfo.AppendFormat(L" канал %s, интервал %s - %s;",
                                      taskParams->channelName.GetString(),
                                      taskParams->startTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString(),
                                      taskParams->endTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString());
        }

        OUTPUT_LOG_SET_TEXT(L"Загрузка данных%s. TaskId = %s",
                            channelsInfo.GetString(),
                            CString(CComBSTR(newTaskId)).GetString());
#endif // DETAILED_LOGGING

    }

    m_cvTasks.notify_one();

    return newTaskId;
}

//----------------------------------------------------------------------------//
// IMonitoringTasksService
void MonitoringTasksServiceImpl::removeTask(const TaskId& taskId)
{
#ifdef DETAILED_LOGGING

    OUTPUT_LOG_FUNC_ENTER;
    OUTPUT_LOG_SET_TEXT(L"TaskId = %s", CString(CComBSTR(taskId)).GetString());

#endif // DETAILED_LOGGING

    // удаляем из списка результатов
    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        m_resultsList.erase(taskId);
    }

    // удаляем все задания мониторинга с таким идентификатором
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queueTasks.erase(std::remove_if(m_queueTasks.begin(), m_queueTasks.end(),
                                          [&taskId](const MonitoringTaskPtr& task)
                                          {
                                              return task->m_taskId == taskId;
                                          }),
                           m_queueTasks.end());
    }
}

//----------------------------------------------------------------------------//
void MonitoringTasksServiceImpl::tasksExecutorThread()
{
    for (; !m_interruptThread; )
    {
        MonitoringTaskPtr pMonitoringTask = nullptr;
        {
            // ждем пока будет добавлено сообщение в очередь
            std::unique_lock<std::mutex> lk(m_queueMutex);

            // ждем пока не появится хоть одно задание
            while (m_queueTasks.empty())
            {
                m_cvTasks.wait(lk);

                // если нас пробудили для прерывания - прерываемся
                if (m_interruptThread)
                    return;
            }

            // получаем след задание для выполенения из очереди
            pMonitoringTask = std::move(m_queueTasks.front());
            // удаляем его из очереди заданий
            m_queueTasks.pop_front();
        }

        // выполняем задание
        executeTask(pMonitoringTask);
    }
}

//----------------------------------------------------------------------------//
void MonitoringTasksServiceImpl::executeTask(MonitoringTaskPtr pMonitoringTask)
{
#ifdef DETAILED_LOGGING
    OUTPUT_LOG_FUNC_ENTER;
#endif // DETAILED_LOGGING

    assert(pMonitoringTask);
    if (!pMonitoringTask)
        return;

    MonitoringResult::ResultData taskResult(pMonitoringTask->m_pTaskParams);

    // параметры задания
    const CString& channelName = pMonitoringTask->m_pTaskParams->channelName;
    const CTime& startTime  = pMonitoringTask->m_pTaskParams->startTime;
    const CTime& endTime    = pMonitoringTask->m_pTaskParams->endTime;

#ifdef DETAILED_LOGGING
    OUTPUT_LOG_CALCTIME_ENABLE(true);
    OUTPUT_LOG_TEXT(L"Загрузка данных по каналу %s, интервал %s - %s.",
                    channelName.GetString(),
                    startTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString(),
                    endTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString());
#endif // DETAILED_LOGGING

    // сообщение об ошибке
    CString exceptionMessage;
    try
    {
        // создаем класс для получения данных по каналу
        CChannelDataGetter channelDataGetter(channelName, startTime, endTime);
        // частота канала, считаем что она постоянная
        double frequency = channelDataGetter.getChannelInfo()->GetFrequency();

        // будем грузить порциями, максимум по 10 000 000 точек
        // для канала с частотой 10 это 11,5 дней, или ~40 мб памяти
        const double maxPartSize = 1e7;

        bool bDataLoaded = false;

        // данные по каналу
        std::vector<float> channelData;

        // последнее загруженное время по каналу
        CTime lastLoadedTime = startTime;
        while (lastLoadedTime != endTime && !m_interruptThread)
        {
            // временной промежуток который осталось загрузить
            CTimeSpan allTimeSpan = endTime - lastLoadedTime;

            // рассчитываем количество секунд которые можем загрузить
            LONGLONG loadingSeconds = std::min<LONGLONG>(allTimeSpan.GetTotalSeconds(),
                                                            LONGLONG(maxPartSize / frequency));

            try
            {
                // загружаем данные текущией порции
                channelDataGetter.getSourceChannelData(channelName, lastLoadedTime,
                                                        lastLoadedTime + loadingSeconds,
                                                        channelData);

                // количество пустых значений
                LONGLONG emptyValuesCount = 0;

                // Индекс в данных и индекс последних не нулевых данных
                LONGLONG index = 0, lastNotEmptyValueIndex = -1;

                // анализируем данные
                // делаем через : потому что данных много и доступ к ним через [] будет занимать много времени
                for (const auto& value : channelData)
                {
                    // прерываемся если поток прервали
                    if (m_interruptThread)
                        return;

                    // абсолютный ноль считаем пропуском данных
                    if (abs(value) > FLT_MIN)
                    {
                        if (!bDataLoaded)
                        {
                            // инициализируем значениями
                            taskResult.startValue = value;
                            taskResult.minValue = value;
                            taskResult.maxValue = value;

                            bDataLoaded = true;
                        }
                        else
                        {
                            if (taskResult.minValue > value)
                                taskResult.minValue = value;
                            if (taskResult.maxValue < value)
                                taskResult.maxValue = value;
                        }

                        // запоминаем индекс последних не нулевых данных
                        lastNotEmptyValueIndex = index;
                    }
                    else
                        emptyValuesCount += 1;

                    ++index; // отдельно считаем текущий индекс в данных
                }

#ifdef DETAILED_LOGGING
                OUTPUT_LOG_TEXT(L"Данные канала %s(частота %.02lf) загружены в промежутке %s - %s. Текущее количество пропусков %lld - новое  %lld. Всего данных %u, пустых %u.",
                                channelName.GetString(), frequency,
                                lastLoadedTime.Format(L"%d.%m.%Y").GetString(),
                                (lastLoadedTime + loadingSeconds).Format(L"%d.%m.%Y").GetString(),
                                taskResult.emptyDataTime,
                                emptyValuesCount,
                                channelData.size(),
                                std::count_if(channelData.begin(), channelData.end(),
                                              [](const auto& val)
                                              {
                                                  return abs(val) <= FLT_MIN;
                                              }));
#endif // DETAILED_LOGGING

                // если были не пустые данные
                if (lastNotEmptyValueIndex != -1)
                {
                    // запоминаем последнее не пустое значение
                    taskResult.currentValue = channelData[(UINT)lastNotEmptyValueIndex];

                    // находим время последнего не пустого значения
                    taskResult.lastDataExistTime =
                        lastLoadedTime + CTimeSpan(__time64_t(lastNotEmptyValueIndex / frequency));
                }

                // рассчитываем количество секунд без данных
                taskResult.emptyDataTime += LONGLONG(emptyValuesCount / frequency);
            }
            catch (const CString& logMessage)
            {
                // в каком-то промежутке данных может вообще не быть, помечаем их пустыми
                taskResult.emptyDataTime += loadingSeconds;

#ifdef DETAILED_LOGGING
                OUTPUT_LOG_TEXT(L"Возникла ошибка при получении данных. Текущее количество пропусков %lld. Ошибка: %s",
                                taskResult.emptyDataTime,
                                logMessage.GetString());
#endif // DETAILED_LOGGING
            }

            // обновляем последнее загруженное время
            lastLoadedTime += loadingSeconds;
        }

        // если не было загружено данных - сообщаем об этом
        if (!bDataLoaded)
            taskResult.resultType = MonitoringResult::Result::eNoData;
    }
    catch (CException* e)
    {
        TCHAR logMessage[MAX_PATH];
        e->GetErrorMessage(logMessage, MAX_PATH);
        exceptionMessage = logMessage;
    }
    catch (const CString& logMessage)
    {
        exceptionMessage = logMessage;
    }

    // Если было сообщение об ошибке - показываем его
    if (!exceptionMessage.IsEmpty())
    {
        taskResult.errorText.
            Format(L"Возникла ошибка при получении данных канала \"%s\" в промежутке %s - %s.\n%s",
                   channelName.GetString(),
                   startTime.Format(L"%d.%m.%Y").GetString(),
                   endTime.Format(L"%d.%m.%Y").GetString(),
                   exceptionMessage.GetString());

        // сообщаем что есть текст ошибки
        taskResult.resultType = MonitoringResult::Result::eErrorText;

        // если возникло исключени данным верить нельзя - считаем что весь интервал пустой
        taskResult.emptyDataTime = endTime - startTime;
    }

#ifdef DETAILED_LOGGING
    CString loadingResult;
    switch (taskResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:
        loadingResult.Format(L"Данные получены, curVal = %.02f minVal = %.02f maxVal = %.02f, emptyDataSeconds = %lld",
                             taskResult.currentValue, taskResult.minValue, taskResult.maxValue,
                             taskResult.emptyDataTime.GetTotalSeconds());
        break;
    case MonitoringResult::Result::eErrorText:
        loadingResult = taskResult.errorText;
        break;
    case MonitoringResult::Result::eNoData:
        loadingResult = L"Данных нет";
        break;
    default:
        loadingResult = L"Не известный тип резултата!";
        assert(false);
        break;
    }

    OUTPUT_LOG_ADD_COMMENT(L"Загрузка данных по каналу %s завершена. Результат : %s",
                           channelName.GetString(), loadingResult.GetString());
#endif // DETAILED_LOGGING

    // заносим результат задания в список результатов
    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);

        auto resultIt = m_resultsList.find(pMonitoringTask->m_taskId);
        // проверяем что задание не удалили пока мы его выполняли
        if (resultIt != m_resultsList.end())
        {
            MonitoringResult::ResultsList& resultsList = resultIt->second.m_pMonitoringResult->m_taskResults;
            // вставляем результат
            resultsList.emplace_back(std::move(taskResult));
            // если выполнили все задания которые должны оповещаем об этом
            if (resultsList.size() == resultIt->second.m_taskCount)
            {
                // оповещаем о выполнении задания
                get_service<CMassages>().postMessage(onCompletingMonitoringTask, 0.f,
                                                     std::static_pointer_cast<IEventData>(resultIt->second.m_pMonitoringResult));

                // удаляем задание из локального спика заданий
                m_resultsList.erase(resultIt);
            }
        }
    }
}
