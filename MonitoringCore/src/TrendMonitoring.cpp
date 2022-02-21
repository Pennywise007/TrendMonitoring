#include "pch.h"

#include <ctime>

#include <myInclude/ChannelDataCorrector/ChannelDataGetter.h>

#include <DirsService.h>

#include "Serialization/SerializatorFabric.h"
#include "TrendMonitoring.h"
#include "Telegram/TelegramBot.h"
#include "Utils.h"

// время в которое будет отсылаться отчёт каждый день часы + минуты (20:00)
const std::pair<int, int> kReportDataTime = std::make_pair(20, 00);

////////////////////////////////////////////////////////////////////////////////
ITrendMonitoring* get_monitoring_service()
{
    return &get_service<TrendMonitoring>();
}

////////////////////////////////////////////////////////////////////////////////
// Реализация сервиса для мониторинга каналов
TrendMonitoring::TrendMonitoring()
{
    // подписываемся на события о завершении мониторинга
    EventRecipientImpl::subscribe(onCompletingMonitoringTask);
    // подписываемся на событие изменения в списке пользователей телеграмма
    EventRecipientImpl::subscribe(telegram::users::onUsersListChangedEvent);

    // загружаем конфигурацию из файла
    loadConfiguration();

    // инициализируем бота после загрузки конфигурации
    m_telegramBot = std::make_unique<telegram::bot::CTelegramBot>(m_appConfig->getTelegramUsers());
    // передаем боту настройки из конфигов
    m_telegramBot->setBotSettings(getBotSettings());

    // запускам задания для мониторинга, делаем отдельными заданиями ибо могут
    // долго грузиться данные для интервалов
    for (auto& channel : m_appConfig->m_chanelParameters)
        addMonitoringTaskForChannel(channel, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // подключаем таймер обновления данных
    CTickHandlerImpl::subscribeTimer(getUpdateDataInterval(), TimerType::eUpdatingData);

    // подключаем таймер отчета
    {
        // рассчитываем время следущего отчета
        time_t nextReportTime_t = time(NULL);

        tm nextReportTm;
        localtime_s(&nextReportTm, &nextReportTime_t);

        // проверяем что в этот день ещё не настало время для отчёта
        bool needReportToday = false;
        if (nextReportTm.tm_hour < kReportDataTime.first ||
            (nextReportTm.tm_hour == kReportDataTime.first &&
             nextReportTm.tm_min < kReportDataTime.second))
            needReportToday = true;

        // рассчитываем время след отчёта
        nextReportTm.tm_hour = kReportDataTime.first;
        nextReportTm.tm_min  = kReportDataTime.second;
        nextReportTm.tm_sec  = 0;

        // если не сегодня - значит след отчёт нужен уже завтра
        if (!needReportToday)
            ++nextReportTm.tm_mday;

        // конвертируем в std::chrono
        std::chrono::system_clock::time_point nextReportTime =
            std::chrono::system_clock::from_time_t(mktime(&nextReportTm));

        // Подключаем таймер с интервалом до след отчёта
        CTickHandlerImpl::subscribeTimer(nextReportTime - std::chrono::system_clock::now(),
                                         TimerType::eEveryDayReporting);
    }
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
std::list<CString> TrendMonitoring::getNamesOfAllChannels() const
{
    OUTPUT_LOG_FUNC_ENTER;

    // заполняем сортированный список каналов
    std::list<CString> allChannelsNames;
    // ищем именно в директории с сжатыми сигналами ибо там меньше вложенность и поиск идёт быстрее
    ChannelDataGetter::FillChannelList(allChannelsNames, false);

    return allChannelsNames;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
std::list<CString> TrendMonitoring::getNamesOfMonitoringChannels() const
{
    // заполняем сортированный список каналов
    std::list<CString> allChannelsNames;
    for (const auto& channel :  m_appConfig->m_chanelParameters)
        allChannelsNames.emplace_back(channel->channelName);

    return allChannelsNames;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::updateDataForAllChannels()
{
    // очищаем данные для всех каналов и добавляем задания на мониторинг
    for (auto& channel : m_appConfig->m_chanelParameters)
    {
        // удаляем задания мониторинга для канала
        delMonitoringTaskForChannel(channel);
        // очищаем данные
        channel->resetChannelData();
        // добавляем новое задание, делаем по одному чтобы мочь прервать конкретное
        addMonitoringTaskForChannel(channel, MonitoringTaskInfo::TaskType::eIntervalInfo);
    }

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::getNumberOfMonitoringChannels() const
{
    return m_appConfig->m_chanelParameters.size();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
const MonitoringChannelData& TrendMonitoring::getMonitoringChannelData(const size_t channelIndex)  const
{
    assert(channelIndex < m_appConfig->m_chanelParameters.size() && "Количество каналов меньше чем индекс канала");
    return (*std::next(m_appConfig->m_chanelParameters.begin(), channelIndex))->getMonitoringData();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::addMonitoringChannel()
{
    OUTPUT_LOG_FUNC_ENTER;

    const auto& channelsList = getNamesOfAllChannels();

    if (channelsList.empty())
    {
        ::MessageBox(NULL, L"Каналы для мониторинга не найдены", L"Невозможно добавить канал для мониторинга", MB_OK | MB_ICONERROR);
        return 0;
    }

    m_appConfig->m_chanelParameters.push_back(ChannelParameters::make(*channelsList.begin()));

    // начинаем грузить данные
    addMonitoringTaskForChannel(m_appConfig->m_chanelParameters.back(),
                                MonitoringTaskInfo::TaskType::eIntervalInfo);

    // сообщаем об изменении в списке каналов синхронно, чтобы в списке каналов успел появиться новый
    onMonitoringChannelsListChanged(false);

    return m_appConfig->m_chanelParameters.size() - 1;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::removeMonitoringChannelByIndex(const size_t channelIndex)
{
    auto& channelsList = m_appConfig->m_chanelParameters;

    if (channelIndex >= channelsList.size())
    {
        assert(!"Количество каналов меньше чем индекс удаляемого канала");
        return channelIndex;
    }

    // получаем итератор на канал
    ChannelIt channelIt = std::next(channelsList.begin(), channelIndex);

    // прерывааем задание мониторинга для канала
    delMonitoringTaskForChannel(*channelIt);

    // удаляем из спика каналов
    channelIt = channelsList.erase(channelIt);
    if (channelIt == channelsList.end() && !channelsList.empty())
        --channelIt;

    size_t result = std::distance(channelsList.begin(), channelIt);

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged();

    return result;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoringChannelNotify(const size_t channelIndex,
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
    if (!channelParams->changeNotification(newNotifyState))
        return;

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged(true);
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoringChannelName(const size_t channelIndex,
                                                  const CString& newChannelName)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить имя канала", MB_OK | MB_ICONERROR);
        return;
    }

    // получаем параметры канала по которому меняем имя
    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->changeName(newChannelName))
        return;

    // если имя изменилось успешно прерываем возможное задание по каналу
    delMonitoringTaskForChannel(channelParams);
    // добавляем новое задание для мониторинга
    addMonitoringTaskForChannel(channelParams, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoringChannelInterval(const size_t channelIndex,
                                                     const MonitoringInterval newInterval)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить интервал наблюдения", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->changeInterval(newInterval))
        return;

    // если имя изменилось успешно прерываем возможное задание по каналу
    delMonitoringTaskForChannel(channelParams);
    // добавляем новое задание для мониторинга
    addMonitoringTaskForChannel(channelParams, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoringChannelAlarmingValue(const size_t channelIndex,
                                                           const float newValue)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить значение оповещения", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->changeAlarmingValue(newValue))
        return;

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::moveUpMonitoringChannelByIndex(const size_t channelIndex)
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
    onMonitoringChannelsListChanged();

    return resultIndex;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::moveDownMonitoringChannelByIndex(const size_t channelIndex)
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
    onMonitoringChannelsListChanged();

    return resultIndex;
}

//----------------------------------------------------------------------------//
const telegram::bot::TelegramBotSettings& TrendMonitoring::getBotSettings() const
{
    return m_appConfig->getTelegramSettings();
}

//----------------------------------------------------------------------------//
void TrendMonitoring::setBotSettings(const telegram::bot::TelegramBotSettings& newSettings)
{
    // применяем настройки
    m_appConfig->setTelegramSettings(newSettings);

    // сохраняем настройки
    saveConfiguration();

    // инициализируем бота новым токеном
    m_telegramBot->setBotSettings(getBotSettings());
}

//----------------------------------------------------------------------------//
// IEventRecipient
void TrendMonitoring::onEvent(const EventId& code, float /*eventValue*/,
                              const std::shared_ptr<IEventData>& eventData)
{
    if (code == onCompletingMonitoringTask)
    {
        MonitoringResult::Ptr monitoringResult = std::static_pointer_cast<MonitoringResult>(eventData);

        auto it = m_monitoringTasksInfo.find(monitoringResult->m_taskId);
        if (it == m_monitoringTasksInfo.end())
            // выполнено не наше задание
            return;

        assert(!monitoringResult->m_taskResults.empty() && !it->second.channelParameters.empty());

        // итераторы по параметрам каналов
        ChannelIt channelIt  = it->second.channelParameters.begin();
        const ChannelIt channelEnd = it->second.channelParameters.end();

        // итераторы по результатам задания
        MonitoringResult::ResultIt resultIt  = monitoringResult->m_taskResults.begin();
        const MonitoringResult::ResultIt resultEnd = monitoringResult->m_taskResults.end();

        // флаг что в списке мониторинга были изменения
        bool bMonitoringListChanged = false;

        std::vector<CString> listOfProblemChannels;
        listOfProblemChannels.reserve(monitoringResult->m_taskResults.size());

        CString reportTextForAllChannels;
        // для каждого канала анализируем его результат задания
        for (; channelIt != channelEnd && resultIt != resultEnd; ++channelIt, ++resultIt)
        {
            assert((*channelIt)->channelName == resultIt->pTaskParameters->channelName &&
                   "Получены результаты для другого канала!");

            CString channelError;
            bMonitoringListChanged |= MonitoringTaskResultHandler::HandleIntervalInfoResult(it->second.taskType, *resultIt,
                                                                                            *channelIt, channelError);

            // если возникла ошибка при получении данных и можно о ней оповещать
            if (!channelError.IsEmpty() && (*channelIt)->bNotify)
            {
                reportTextForAllChannels.AppendFormat(L"Канал \"%s\": %s\n",
                                                      (*channelIt)->channelName.GetString(),
                                                      channelError.GetString());

                listOfProblemChannels.emplace_back((*channelIt)->channelName);
            }
        }

        reportTextForAllChannels = reportTextForAllChannels.Trim();

        // если возникли ошибки отрабатываем их по разному для каждого типа задания
        if (it->second.taskType == MonitoringTaskInfo::TaskType::eEveryDayReport)
        {
            // если не о чем сообщать говорим что все ок
            if (reportTextForAllChannels.IsEmpty())
                reportTextForAllChannels = L"Данные в порядке.";

            const CString reportDelimer(L'*', 25);

            // создаем сообщение об отчёте
            auto reportMessage = std::make_shared<MessageTextData>();
            reportMessage->messageText.Format(L"%s\n\nЕжедневный отчёт за %s\n\n%s\n%s",
                                              reportDelimer.GetString(),
                                              CTime::GetCurrentTime().Format(L"%d.%m.%Y").GetString(),
                                              reportTextForAllChannels.GetString(),
                                              reportDelimer.GetString());

            // оповещаем о готовом отчёте
            get_service<CMassages>().postMessage(onReportPreparedEvent, 0,
                                                 std::static_pointer_cast<IEventData>(reportMessage));

            // сообщаем пользователям телеграма
            m_telegramBot->sendMessageToAdmins(reportMessage->messageText);
        }
        else if (!reportTextForAllChannels.IsEmpty())
        {
            assert(it->second.taskType == MonitoringTaskInfo::TaskType::eIntervalInfo ||
                   it->second.taskType == MonitoringTaskInfo::TaskType::eUpdatingInfo);

            // сообщаем в лог что возникли проблемы
            send_message_to_log(LogMessageData::MessageType::eError, reportTextForAllChannels.GetString());

            // оповещаем о возникшей ошибке
            auto errorMessage = std::make_shared<MonitoringErrorEventData>();
            errorMessage->errorTextForAllChannels = std::move(reportTextForAllChannels);
            errorMessage->problemChannelNames = std::move(listOfProblemChannels);

            // генерим идентификатор ошибки
            if (!SUCCEEDED(CoCreateGuid(&errorMessage->errorGUID)))
                assert(!"Не удалось создать гуид!");
            get_service<CMassages>().postMessage(onMonitoringErrorEvent, 0,
                                                 std::static_pointer_cast<IEventData>(errorMessage));
        }

        if (bMonitoringListChanged)
            // оповещаем об изменении в списке для мониторинга
            get_service<CMassages>().postMessage(onMonitoringListChanged);

        // удаляем задание из списка
        m_monitoringTasksInfo.erase(it);
    }
    else if (code == telegram::users::onUsersListChangedEvent)
    {
        // сохраняем изменения в конфиг
        saveConfiguration();
    }
    else
        assert(!"Неизвестное событие");
}

//----------------------------------------------------------------------------//
bool TrendMonitoring::onTick(TickParam tickParam)
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
                        send_message_to_log(LogMessageData::MessageType::eError,
                                            L"Данные по каналу %s грузятся больше 10 минут",
                                            channelParameters->channelName.GetString());

                    continue;
                }

                // уже должны были грузить данные и заполнить
                assert(channelParameters->m_loadingParametersIntervalEnd.has_value());

                // если с момента загрузки прошло не достаточно времени(только поставили на загрузку)
                if (!channelParameters->m_loadingParametersIntervalEnd.has_value() ||
                    (currentTime - *channelParameters->m_loadingParametersIntervalEnd).GetTotalMinutes() <
                    getUpdateDataInterval().count() - 1)
                    continue;

                // добавляем канал в список обновляемых
                updatingDataChannels.emplace_back(channelParameters);
                listTaskParams.emplace_back(new TaskParameters(channelParameters->channelName,
                                                               *channelParameters->m_loadingParametersIntervalEnd,
                                                               currentTime));
            }

            if (!listTaskParams.empty())
                addMonitoringTaskForChannels(updatingDataChannels, listTaskParams,
                                             MonitoringTaskInfo::TaskType::eUpdatingInfo);
        }
        break;
    case TimerType::eEveryDayReporting:
        {
            // Подключаем таймер с интервалом до след отчёта
            CTickHandlerImpl::subscribeTimer(std::chrono::hours(24), TimerType::eEveryDayReporting);

            if (!m_appConfig->m_chanelParameters.empty())
            {
                // копия текущих каналов по которым запускаем мониторинг
                ChannelParametersList channelsCopy;
                for (const auto& currentChannel : m_appConfig->m_chanelParameters)
                {
                    channelsCopy.push_back(ChannelParameters::make(currentChannel->channelName));
                    channelsCopy.back()->alarmingValue = currentChannel->alarmingValue;
                }

                // запускаем задание формирования отчёта за последний день
                addMonitoringTaskForChannels(channelsCopy,
                                             MonitoringTaskInfo::TaskType::eEveryDayReport,
                                             CTimeSpan(1, 0, 0, 0));
            }
        }
        // исполняемый один раз таймер т.к. мы запустили новый
        return false;
    default:
        assert(!"Неизвестный таймер!");
        break;
    }

    return true;
}

//----------------------------------------------------------------------------//
void TrendMonitoring::installTelegramBot(const std::shared_ptr<telegram::bot::ITelegramBot>& telegramBot)
{
    if (telegramBot)
        m_telegramBot = telegramBot;
    else
        m_telegramBot = std::make_unique<telegram::bot::CTelegramBot>(m_appConfig->getTelegramUsers());;
}

//----------------------------------------------------------------------------//
void TrendMonitoring::saveConfiguration()
{
    SerializationExecutor::serializeObject(SerializationFabric::createXMLSerializator(getConfigurationXMLFilePath()),
                                           m_appConfig);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::loadConfiguration()
{
    SerializationExecutor::deserializeObject(SerializationFabric::createXMLDeserializator(getConfigurationXMLFilePath()),
                                             m_appConfig);
}

//----------------------------------------------------------------------------//
CString TrendMonitoring::getConfigurationXMLFilePath() const
{
    return get_service<DirsService>().getExeDir() + kConfigFileName;
}

//----------------------------------------------------------------------------//
void TrendMonitoring::onMonitoringChannelsListChanged(bool bAsynchNotify /*= true*/)
{
    OUTPUT_LOG_FUNC_ENTER;

    // сохраняем новый список мониторинга
    saveConfiguration();

    // оповещаем об изменении в списке для мониторинга
    if (bAsynchNotify)
        get_service<CMassages>().postMessage(onMonitoringListChanged);
    else
        get_service<CMassages>().sendMessage(onMonitoringListChanged);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::addMonitoringTaskForChannel(ChannelParameters::Ptr& channelParams,
                                                  const MonitoringTaskInfo::TaskType taskType,
                                                  CTimeSpan monitoringInterval /* = -1*/)
{
    OUTPUT_LOG_FUNC;
    OUTPUT_LOG_ADD_COMMENT("channel name = %s", channelParams->channelName.GetString());
    OUTPUT_LOG_DO;

    if (monitoringInterval == -1)
        monitoringInterval = monitoring_interval_to_timespan(channelParams->monitoringInterval);

    addMonitoringTaskForChannels({ channelParams }, taskType, monitoringInterval);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::addMonitoringTaskForChannels(const ChannelParametersList& channelList,
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

    addMonitoringTaskForChannels(channelList, listTaskParams, taskType);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                                   const std::list<TaskParameters::Ptr>& taskParams,
                                                   const MonitoringTaskInfo::TaskType taskType)
{
    if (channelList.size() != taskParams.size())
    {
        assert(!"Различается список каналов и заданий!");
        return;
    }

    if (taskParams.empty())
    {
        assert(!"Передали пустой список заданий!");
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
        get_monitoring_tasks_service()->addTaskList(taskParams, IMonitoringTasksService::eNormal),
        taskInfo);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::delMonitoringTaskForChannel(const ChannelParameters::Ptr& channelParams)
{
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

                        get_monitoring_tasks_service()->removeTask(monitoringTaskIt->first);
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
