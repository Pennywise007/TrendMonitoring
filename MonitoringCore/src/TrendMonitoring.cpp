#include "pch.h"

#include <ctime>

#include <ext/thread/invoker.h>

#include "include/IDirService.h"
#include "MonitoringTaskService/ChannelDataGetter/ChannelDataGetter.h"
#include "TrendMonitoring.h"

#include <chrono>

#include "Utils.h"

// время в которое будет отсылаться отчёт каждый день часы + минуты (20:00)
const std::pair<int, int> kReportDataTime = std::make_pair(20, 00);

// Реализация сервиса для мониторинга каналов
TrendMonitoring::TrendMonitoring(ext::ServiceProvider::Ptr&& serviceProvider)
    : ServiceProviderHolder(std::move(serviceProvider))
    , m_appConfig(CreateObject<ApplicationConfiguration>())
{
    auto tt = std::make_shared<ApplicationConfiguration>(m_serviceProvider);
    tt;
    // load configuration from file
    LoadConfiguration();

    SaveConfiguration();

    // initialize the bot after loading the configuration
    m_telegramBot = GetInterface<telegram::bot::ITelegramBot>();

    // launch tasks for monitoring, make them separate tasks because they can
    // long time to load data for intervals
    for (auto& channel : m_appConfig->m_chanelParameters)
        AddMonitoringTaskForChannel(channel, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // connect the data update timer
    TickSubscriber::SubscribeInvokedTimer(UpdateDataInterval, TimerType::eUpdatingData);

    // connect the report timer
    {
        // calculate next report time
        time_t nextReportTime_t = time(NULL);

        tm nextReportTm;
        localtime_s(&nextReportTm, &nextReportTime_t);

        // check that this day is not yet the time for the report
        bool needReportToday = false;
        if (nextReportTm.tm_hour < kReportDataTime.first ||
            (nextReportTm.tm_hour == kReportDataTime.first &&
            nextReportTm.tm_min < kReportDataTime.second))
            needReportToday = true;

        // calculate the next report time
        nextReportTm.tm_hour = kReportDataTime.first;
        nextReportTm.tm_min = kReportDataTime.second;
        nextReportTm.tm_sec = 0;

        // if not today, then the next report is needed tomorrow
        if (!needReportToday)
            ++nextReportTm.tm_mday;

        // convert to std::chrono
        std::chrono::system_clock::time_point nextReportTime =
            std::chrono::system_clock::from_time_t(mktime(&nextReportTm));

        // Connect the timer with an interval until the countdown trace
        EXT_UNUSED(m_everyDayReportScheduler.SubscribeTaskAtTime([&]()
        {
            GenerateEveryDayReport();
        }, nextReportTime));
    }
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
std::list<std::wstring> TrendMonitoring::GetNamesOfAllChannels() const
{
    EXT_TRACE_SCOPE() << EXT_TRACE_FUNCTION;

    // заполняем сортированный список каналов
    std::list<CString> allChannelsNames;
    // ищем именно в директории с сжатыми сигналами ибо там меньше вложенность и поиск идёт быстрее
    ChannelDataGetter::FillChannelList(allChannelsNames, false);

    std::list<std::wstring> result;
    std::transform(allChannelsNames.begin(), allChannelsNames.end(), std::back_inserter(result), [](const auto& name) { return name.GetString(); });
    return result;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
std::list<std::wstring> TrendMonitoring::GetNamesOfMonitoringChannels() const
{
    // заполняем сортированный список каналов
    std::list<std::wstring> allChannelsNames;
    for (const auto& channel :  m_appConfig->m_chanelParameters)
        allChannelsNames.emplace_back(channel->channelName);

    return allChannelsNames;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::UpdateDataForAllChannels()
{
    // очищаем данные для всех каналов и добавляем задания на мониторинг
    for (auto& channel : m_appConfig->m_chanelParameters)
    {
        // удаляем задания мониторинга для канала
        DelMonitoringTaskForChannel(channel);
        // очищаем данные
        channel->ResetChannelData();
        // добавляем новое задание, делаем по одному чтобы мочь прервать конкретное
        AddMonitoringTaskForChannel(channel, MonitoringTaskInfo::TaskType::eIntervalInfo);
    }

    // сообщаем об изменении в списке каналов
    OnMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::GetNumberOfMonitoringChannels() const
{
    return m_appConfig->m_chanelParameters.size();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
const MonitoringChannelData& TrendMonitoring::GetMonitoringChannelData(const size_t channelIndex)  const
{
    EXT_ASSERT(channelIndex < m_appConfig->m_chanelParameters.size()) << "Количество каналов меньше чем индекс канала";
    return (*std::next(m_appConfig->m_chanelParameters.begin(), channelIndex))->GetMonitoringData();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::AddMonitoringChannel()
{
    EXT_TRACE_SCOPE() << EXT_TRACE_FUNCTION;

    const auto& channelsList = GetNamesOfAllChannels();

    if (channelsList.empty())
    {
        ::MessageBox(NULL, L"Каналы для мониторинга не найдены", L"Невозможно добавить канал для мониторинга", MB_OK | MB_ICONERROR);
        return 0;
    }

    m_appConfig->m_chanelParameters.push_back(std::make_shared<ChannelParameters>(*channelsList.begin()));

    // начинаем грузить данные
    AddMonitoringTaskForChannel(m_appConfig->m_chanelParameters.back(),
                                MonitoringTaskInfo::TaskType::eIntervalInfo);

    // сообщаем об изменении в списке каналов синхронно, чтобы в списке каналов успел появиться новый
    OnMonitoringChannelsListChanged(false);

    return m_appConfig->m_chanelParameters.size() - 1;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::RemoveMonitoringChannelByIndex(const size_t channelIndex)
{
    auto& channelsList = m_appConfig->m_chanelParameters;

    if (channelIndex >= channelsList.size())
    {
        EXT_ASSERT(!"Количество каналов меньше чем индекс удаляемого канала");
        return channelIndex;
    }

    // получаем итератор на канал
    ChannelIt channelIt = std::next(channelsList.begin(), channelIndex);

    // прерывааем задание мониторинга для канала
    DelMonitoringTaskForChannel(*channelIt);

    // удаляем из спика каналов
    channelIt = channelsList.erase(channelIt);
    if (channelIt == channelsList.end() && !channelsList.empty())
        --channelIt;

    size_t result = std::distance(channelsList.begin(), channelIt);

    // сообщаем об изменении в списке каналов
    OnMonitoringChannelsListChanged();

    return result;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::ChangeMonitoringChannelNotify(const size_t channelIndex,
                                                    const bool newNotifyState)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить нотификацию канала", MB_OK | MB_ICONERROR);
        return;
    }

    // получаем параметры канала по которому меняем имя
    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(),
                                                      channelIndex);
    if (!channelParams->ChangeNotification(newNotifyState))
        return;

    // сообщаем об изменении в списке каналов
    OnMonitoringChannelsListChanged(true);
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::ChangeMonitoringChannelNotify(const size_t channelIndex,
                                                  const std::wstring& newChannelName)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить имя канала", MB_OK | MB_ICONERROR);
        return;
    }

    // получаем параметры канала по которому меняем имя
    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->ChangeName(newChannelName))
        return;

    // если имя изменилось успешно прерываем возможное задание по каналу
    DelMonitoringTaskForChannel(channelParams);
    // добавляем новое задание для мониторинга
    AddMonitoringTaskForChannel(channelParams, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // сообщаем об изменении в списке каналов
    OnMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::ChangeMonitoringChannelInterval(const size_t channelIndex,
                                                     const MonitoringInterval newInterval)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить интервал наблюдения", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->ChangeInterval(newInterval))
        return;

    // если имя изменилось успешно прерываем возможное задание по каналу
    DelMonitoringTaskForChannel(channelParams);
    // добавляем новое задание для мониторинга
    AddMonitoringTaskForChannel(channelParams, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // сообщаем об изменении в списке каналов
    OnMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::ChangeMonitoringChannelAlarmingValue(const size_t channelIndex,
                                                           const float newValue)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить значение оповещения", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->ChangeAlarmingValue(newValue))
        return;

    // сообщаем об изменении в списке каналов
    OnMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::MoveUpMonitoringChannelByIndex(const size_t channelIndex)
{
    auto& channelsList = m_appConfig->m_chanelParameters;

    if (channelsList.size() < 2)
        return channelIndex;

    ChannelIt movingIt = std::next(channelsList.begin(), channelIndex);

    // индекс позиции канала после перемещения
    size_t resultIndex;

    if (movingIt == channelsList.begin())
    {
        // двигаем в конец
        channelsList.splice(channelsList.end(), channelsList, movingIt);
        resultIndex = channelsList.size() - 1;
    }
    else
    {
        // меняем с предыдущим местами
        std::iter_swap(movingIt, std::prev(movingIt));
        resultIndex = channelIndex - 1;
    }

    // сообщаем об изменении в списке каналов
    OnMonitoringChannelsListChanged();

    return resultIndex;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::MoveDownMonitoringChannelByIndex(const size_t channelIndex)
{
    auto& channelsList = m_appConfig->m_chanelParameters;

    if (channelsList.size() < 2)
        return channelIndex;

    ChannelIt movingIt = std::next(channelsList.begin(), channelIndex);

    // индекс позиции канала после перемещения
    size_t resultIndex;

    if (movingIt == --channelsList.end())
    {
        // двигаем в начало
        channelsList.splice(channelsList.begin(), channelsList, movingIt);
        resultIndex = 0;
    }
    else
    {
        // меняем со следующим местами
        std::iter_swap(movingIt, std::next(movingIt));
        resultIndex = channelIndex + 1;
    }

    // сообщаем об изменении в списке каналов
    OnMonitoringChannelsListChanged();

    return resultIndex;
}

void TrendMonitoring::OnChanged()
{
    // сохраняем изменения в конфиг
    SaveConfiguration();
}

void TrendMonitoring::OnBotSettingsChanged(const bool, const std::wstring&)
{
    // сохраняем изменения в конфиг
    SaveConfiguration();
}

void TrendMonitoring::OnCompleteTask(const TaskId& taskId, ResultsPtrList monitoringResult)
{
    auto it = m_monitoringTasksInfo.find(taskId);
    if (it == m_monitoringTasksInfo.end())
        // выполнено не наше задание
        return;

    EXT_ASSERT(!monitoringResult.empty() && !it->second.channelParameters.empty());

    // итераторы по параметрам каналов
    ChannelIt channelIt  = it->second.channelParameters.begin();
    const ChannelIt channelEnd = it->second.channelParameters.end();

    // итераторы по результатам задания
    auto resultIt  = monitoringResult.begin();
    const auto resultEnd = monitoringResult.end();

    // флаг что в списке мониторинга были изменения
    bool bMonitoringListChanged = false;

    std::vector<std::wstring> listOfProblemChannels;
    listOfProblemChannels.reserve(monitoringResult.size());

    std::wstring reportTextForAllChannels;
    // для каждого канала анализируем его результат задания
    for (; channelIt != channelEnd && resultIt != resultEnd; ++channelIt, ++resultIt)
    {
        EXT_ASSERT((*channelIt)->channelName == (*resultIt)->taskParameters->channelName) << "Wrong channel name data received!";

        std::wstring channelError;
        bMonitoringListChanged |= MonitoringTaskResultHandler::HandleIntervalInfoResult(it->second.taskType, *resultIt,
                                                                                        channelIt->get(), channelError);

        // если возникла ошибка при получении данных и можно о ней оповещать
        if (!channelError.empty() && (*channelIt)->bNotify)
        {
            reportTextForAllChannels += std::string_swprintf(L"Канал \"%s\": %s\n",
                (*channelIt)->channelName.c_str(),
                channelError.c_str());

            listOfProblemChannels.emplace_back((*channelIt)->channelName);
        }
    }

    std::string_trim_all(reportTextForAllChannels);

    // если возникли ошибки отрабатываем их по разному для каждого типа задания
    if (it->second.taskType == MonitoringTaskInfo::TaskType::eEveryDayReport)
    {
        // если не о чем сообщать говорим что все ок
        if (reportTextForAllChannels.empty())
            reportTextForAllChannels = L"Данные в порядке.";

        const std::wstring reportDelimer(L'*', 25);

        // создаем сообщение об отчёте
        std::wstring reportMessage;
        reportMessage = std::string_swprintf(L"%s\n\nЕжедневный отчёт за %s\n\n%s\n%s",
                             reportDelimer.c_str(),
                             CTime::GetCurrentTime().Format(L"%d.%m.%Y").GetString(),
                             reportTextForAllChannels.c_str(),
                             reportDelimer.c_str());

        // оповещаем о готовом отчёте
        ext::send_event_async(&IReportEvents::OnReportDone, reportMessage);

        // сообщаем пользователям телеграма
        m_telegramBot->SendMessageToAdmins(reportMessage);
    }
    else if (!reportTextForAllChannels.empty())
    {
        EXT_ASSERT(it->second.taskType == MonitoringTaskInfo::TaskType::eIntervalInfo ||
               it->second.taskType == MonitoringTaskInfo::TaskType::eUpdatingInfo);

        // сообщаем в лог что возникли проблемы
        send_message_to_log(ILogEvents::LogMessageData::MessageType::eError, reportTextForAllChannels);

        // оповещаем о возникшей ошибке
        auto errorMessage = std::make_shared<IMonitoringErrorEvents::EventData>();
        errorMessage->errorTextForAllChannels = std::move(reportTextForAllChannels);
        errorMessage->problemChannelNames = std::move(listOfProblemChannels);

        // генерим идентификатор ошибки
        EXT_DUMP_IF(FAILED(CoCreateGuid(&errorMessage->errorGUID)));
        ext::send_event(&IMonitoringErrorEvents::OnError, errorMessage);
    }

    if (bMonitoringListChanged)
        ext::send_event_async(&IMonitoringListEvents::OnChanged);

    // удаляем задание из списка
    m_monitoringTasksInfo.erase(it);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::OnTick(ext::tick::TickParam tickParam) EXT_NOEXCEPT
{
    switch (TimerType(tickParam))
    {
    case TimerType::eUpdatingData:
        {
            const CTime currentTime = CTime::GetCurrentTime();

            // т.к. каналы могли добавляться в разное время будем для каждого канала делать
            // свое задание с определенным интервалом, формиурем список параметров заданий
            std::list<TaskParameters::Ptr> listTaskParams;
            // список обновляемых каналов
            ChannelParametersList updatingDataChannels;

            // проходим по всем каналам и смотрим по каким надо обновить данные
            for (auto& channelParameters : m_appConfig->m_chanelParameters)
            {
                // данные ещё не загружены
                if (!channelParameters->channelState.dataLoaded &&
                    !channelParameters->channelState.loadingDataError)
                {
                    // проверяем как долго их нет
                    if (channelParameters->m_loadingParametersIntervalEnd.has_value() &&
                        (currentTime - *channelParameters->m_loadingParametersIntervalEnd).GetTotalMinutes() > 10)
                        send_message_to_log(ILogEvents::LogMessageData::MessageType::eError,
                                            L"Данные по каналу %s грузятся больше 10 минут",
                                            channelParameters->channelName);

                    continue;
                }

                // уже должны были грузить данные и заполнить
                EXT_ASSERT(channelParameters->m_loadingParametersIntervalEnd.has_value());

                // если с момента загрузки прошло не достаточно времени(только поставили на загрузку)
                if (!channelParameters->m_loadingParametersIntervalEnd.has_value() ||
                    (currentTime - *channelParameters->m_loadingParametersIntervalEnd).GetTotalMinutes() <
                    UpdateDataInterval.count() - 1)
                    continue;

                // добавляем канал в список обновляемых
                updatingDataChannels.emplace_back(channelParameters);
                listTaskParams.emplace_back(new TaskParameters(channelParameters->channelName,
                                                               *channelParameters->m_loadingParametersIntervalEnd,
                                                               currentTime));
            }

            if (!listTaskParams.empty())
                AddMonitoringTaskForChannels(updatingDataChannels, listTaskParams,
                                             MonitoringTaskInfo::TaskType::eUpdatingInfo);
        }
        break;
    default:
        EXT_UNREACHABLE("Неизвестный таймер!");
        break;
    }
}

//----------------------------------------------------------------------------//
void TrendMonitoring::SaveConfiguration() EXT_NOEXCEPT
{
    try
    {
        ext::serializable::serializer::Executor::SerializeObject(
            ext::serializable::serializer::Fabric::XMLSerializer(GetConfigurationXMLFilePath()),
            m_appConfig.get());
    }
    catch (...)
    { }
}

//----------------------------------------------------------------------------//
void TrendMonitoring::LoadConfiguration() EXT_NOEXCEPT
{
    try
    {
        ext::serializable::serializer::Executor::DeserializeObject(
            ext::serializable::serializer::Fabric::XMLDeserializer(GetConfigurationXMLFilePath()),
            m_appConfig.get());
    }
    catch (...)
    { }
}

//----------------------------------------------------------------------------//
std::wstring TrendMonitoring::GetConfigurationXMLFilePath() const
{
    return std::filesystem::get_exe_directory().append(kConfigFileName);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::OnMonitoringChannelsListChanged(bool bAsynchNotify /*= true*/)
{
    EXT_TRACE_SCOPE() << EXT_TRACE_FUNCTION;

    // сохраняем новый список мониторинга
    SaveConfiguration();

    // оповещаем об изменении в списке для мониторинга
    if (bAsynchNotify)
        ext::send_event_async(&IMonitoringListEvents::OnChanged);
    else
        ext::send_event(&IMonitoringListEvents::OnChanged);
}

void TrendMonitoring::GenerateEveryDayReport()
{
    // Подключаем таймер с интервалом до след отчёта
    if (!m_everyDayReportScheduler.IsTaskExists(TimerType::eEveryDayReporting))
    {
        EXT_EXPECT(m_everyDayReportScheduler.SubscribeTaskByPeriod([&]()
        {
            GenerateEveryDayReport();
        }, std::chrono::hours(24), TimerType::eEveryDayReporting) == TimerType::eEveryDayReporting);
    }

    ext::get_service<ext::invoke::MethodInvoker>().CallSync([&]()
    {
        if (!m_appConfig->m_chanelParameters.empty())
        {
            // копия текущих каналов по которым запускаем мониторинг
            ChannelParametersList channelsCopy;
            for (const auto& currentChannel : m_appConfig->m_chanelParameters)
            {
                channelsCopy.push_back(std::make_shared<ChannelParameters>(currentChannel->channelName));
                channelsCopy.back()->alarmingValue = currentChannel->alarmingValue;
            }

            // запускаем задание формирования отчёта за последний день
            AddMonitoringTaskForChannels(channelsCopy,
                                         MonitoringTaskInfo::TaskType::eEveryDayReport,
                                         CTimeSpan(1, 0, 0, 0));
        }
    });
}

//----------------------------------------------------------------------------//
void TrendMonitoring::AddMonitoringTaskForChannel(ChannelParameters::Ptr& channelParams,
                                                  const MonitoringTaskInfo::TaskType taskType,
                                                  CTimeSpan monitoringInterval /* = -1*/)
{
    EXT_TRACE() << EXT_TRACE_FUNCTION << std::string_sprintf("channel name = %s", channelParams->channelName.c_str()).c_str();

    if (monitoringInterval == -1)
        monitoringInterval = monitoring_interval_to_timespan(channelParams->monitoringInterval);

    AddMonitoringTaskForChannels({ channelParams }, taskType, monitoringInterval);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::AddMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                                   const MonitoringTaskInfo::TaskType taskType,
                                                   CTimeSpan monitoringInterval)
{
    // формируем интервалы по которым будем запускать задание
    const CTime stopTime = CTime::GetCurrentTime();
    const CTime startTime = stopTime - monitoringInterval;

    // список параметров заданий
    std::list<TaskParameters::Ptr> listTaskParams;
    for (const auto& channelParams : channelList)
    {
        listTaskParams.emplace_back(new TaskParameters(channelParams->channelName,
                                                       startTime, stopTime));
    }

    AddMonitoringTaskForChannels(channelList, listTaskParams, taskType);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::AddMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                                   const std::list<TaskParameters::Ptr>& taskParams,
                                                   const MonitoringTaskInfo::TaskType taskType)
{
    if (channelList.size() != taskParams.size())
    {
        EXT_ASSERT(!"Различается список каналов и заданий!");
        return;
    }

    if (taskParams.empty())
    {
        EXT_ASSERT(!"Передали пустой список заданий!");
        return;
    }

    if (taskType != MonitoringTaskInfo::TaskType::eEveryDayReport)
    {
        // для заданий запроса данных запоминаем время концов загружаемых интервалов
        auto channelsIt = channelList.begin(), channelsItEnd = channelList.end();
        auto taskIt = taskParams.cbegin(), taskItEnd = taskParams.cend();
        for (; taskIt != taskItEnd && channelsIt != channelsItEnd; ++taskIt, ++channelsIt)
        {
            // запоминаем интервал окончания загрузки
            (*channelsIt)->m_loadingParametersIntervalEnd = (*taskIt)->endTime;
        }
    }

    MonitoringTaskInfo taskInfo;
    taskInfo.taskType = taskType;
    taskInfo.channelParameters = channelList;

    // запускаем задание обновления данных
    m_monitoringTasksInfo.try_emplace(
        GetInterface<IMonitoringTasksService>()->AddTaskList(taskParams, IMonitoringTasksService::TaskPriority::eNormal),
        taskInfo);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::DelMonitoringTaskForChannel(const ChannelParameters::Ptr& channelParams)
{
    auto monitoring = GetInterface<IMonitoringTasksService>();

    // Проходим по всем заданиям
    for (auto monitoringTaskIt = m_monitoringTasksInfo.begin(), end = m_monitoringTasksInfo.end();
         monitoringTaskIt != end;)
    {
        switch (monitoringTaskIt->second.taskType)
        {
        case MonitoringTaskInfo::TaskType::eIntervalInfo:
        case MonitoringTaskInfo::TaskType::eUpdatingInfo:
            {
                // список каналов по которым запущено задание
                auto& taskChannels = monitoringTaskIt->second.channelParameters;

                // ищем в списке каналов наш канал
                auto it = std::find(taskChannels.begin(), taskChannels.end(), channelParams);
                if (it != taskChannels.end())
                {
                    // зануляем параметры чтобы не получить результат
                    *it = nullptr;

                    // проверяем что у задания остались не пустые каналы
                    if (std::all_of(taskChannels.begin(), taskChannels.end(), [](const auto& el) { return el == nullptr; }))
                    {
                        // не пустых каналов не осталось - будем удалять задание

                        monitoring->RemoveTask(monitoringTaskIt->first);
                        monitoringTaskIt = m_monitoringTasksInfo.erase(monitoringTaskIt);

                        break;
                    }
                }

                ++monitoringTaskIt;
            }
            break;
        default:
            ++monitoringTaskIt;
            break;
        }
    }
}
