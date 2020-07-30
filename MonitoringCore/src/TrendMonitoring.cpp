#include "pch.h"

#include <ctime>

#include <myInclude/ChannelDataCorrector/ChannelDataGetter.h>
#include <ZetDirs.h>

#include "Serialization/SerializatorFabric.h"
#include "TrendMonitoring.h"

// интервал обновления данных
const std::chrono::minutes kUpdateDataInterval(5);

// время в которое будет отсылаться отчёт каждый день (20:00)
const std::pair<int, int> kReportDataTime = std::make_pair(20, 00);

// имя конфигурационного файла с настройками
const wchar_t kConfigFileName[] = L"AppConfig.xml";

////////////////////////////////////////////////////////////////////////////////
ITrendMonitoring* get_monitoing_service()
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
    EventRecipientImpl::subscribe(onUsersListChangedEvent);

    // загружаем конфигурацию из файла
    loadConfiguration();

    // инициализируем бота после загрузки конфигурации
    m_telegramBot.initBot(m_appConfig->getTelegramUsers());
    // передаем боту настройки из конфигов
    m_telegramBot.setBotSettings(getBotSettings());

    // запускам задания для мониторинга
    for (auto& channel : m_appConfig->m_chanelParameters)
        addMonitoringTaskForChannel(channel, TaskInfo::TaskType::eIntervalInfo);

    // подключаем таймер обновления данных
    CTickHandlerImpl::subscribeTimer(kUpdateDataInterval, TimerType::eUpdatingData);

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
        nextReportTm.tm_min = kReportDataTime.second;
        nextReportTm.tm_sec = 0;

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
std::set<CString> TrendMonitoring::getNamesOfAllChannels()
{
    // получаем все данные по каналам
    std::list<std::pair<CString, CString>> channelsWithConversion;
    CChannelDataGetter::FillChannelList(get_service<ZetDirsService>().getCompressedDir(), channelsWithConversion);

    // заполняем сортированный список каналов
    std::set<CString> allChannelsNames;
    for (const auto& channelInfo : channelsWithConversion)
        allChannelsNames.emplace(channelInfo.first);

    return allChannelsNames;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
std::list<CString> TrendMonitoring::getNamesOfMonitoringChannels()
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
        addMonitoringTaskForChannel(channel, TaskInfo::TaskType::eIntervalInfo);
    }

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::getNumberOfMonitoringChannels()
{
    return m_appConfig->m_chanelParameters.size();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
const MonitoringChannelData& TrendMonitoring::getMonitoringChannelData(const size_t channelIndex)
{
    assert(channelIndex < m_appConfig->m_chanelParameters.size() && "Количество каналов меньше чем индекс канала");
    return (*std::next(m_appConfig->m_chanelParameters.begin(), channelIndex))->getMonitoringData();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::addMonitoingChannel()
{
    const auto& channelsList = getNamesOfAllChannels();

    if (channelsList.empty())
    {
        ::MessageBox(NULL, L"Каналы для мониторинга не найдены", L"Невозможно добавить канал для мониторинга", MB_OK | MB_ICONERROR);
        return 0;
    }

    m_appConfig->m_chanelParameters.push_back(ChannelParameters::make(*channelsList.begin()));

    // начинаем грузить данные
    addMonitoringTaskForChannel(m_appConfig->m_chanelParameters.back(),
                                TaskInfo::TaskType::eIntervalInfo);

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
void TrendMonitoring::changeMonitoingChannelName(const size_t channelIndex,
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
    addMonitoringTaskForChannel(channelParams, TaskInfo::TaskType::eIntervalInfo);

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoingChannelInterval(const size_t channelIndex,
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
    addMonitoringTaskForChannel(channelParams, TaskInfo::TaskType::eIntervalInfo);

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoingChannelAllarmingValue(const size_t channelIndex,
                                                           const float newValue)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить значение оповещения", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->changeAllarmingValue(newValue))
        return;

    // сообщаем об изменении в списке каналов
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::moveUpMonitoingChannelByIndex(const size_t channelIndex)
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
size_t TrendMonitoring::moveDownMonitoingChannelByIndex(const size_t channelIndex)
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
const TelegramBotSettings& TrendMonitoring::getBotSettings()
{
    return m_appConfig->getTelegramSettings();
}

//----------------------------------------------------------------------------//
void TrendMonitoring::setBotSettings(const TelegramBotSettings& newSettings)
{
    // применяем настройки
    m_appConfig->setTelegramSettings(newSettings);

    // сохраняем настройки
    saveConfiguration();

    // инициализируем бота новым токеном
    m_telegramBot.setBotSettings(getBotSettings());
}

//----------------------------------------------------------------------------//
void TrendMonitoring::handleIntervalInfoResult(const MonitoringResult::ResultData& monitoringResult,
                                               ChannelParameters::Ptr& channelParams)
{
    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // данные успешно получены
        {
            // проставляем новые данные из задания
            channelParams->setTrendChannelData(monitoringResult);

            channelParams->channelState[MonitoringChannelData::eDataLoaded] = true;
        }
        break;
    case MonitoringResult::Result::eNoData:     // в переданном интервале нет данных
    case MonitoringResult::Result::eErrorText:  // возникла ошибка
        {
            // оповещаем о возникшей ошибке
            if (monitoringResult.resultType == MonitoringResult::Result::eErrorText &&
                !monitoringResult.errorText.IsEmpty())
                send_message_to_log(LogMessageData::MessageType::eError, monitoringResult.errorText);
            else
                send_message_to_log(LogMessageData::MessageType::eError, L"Нет данных в запрошенном интервале");

            // обновляем время без данных
            channelParams->trendData.emptyDataTime = monitoringResult.emptyDataTime;

            // сообщаем что возникла ошибка загрузки
            channelParams->channelState[MonitoringChannelData::eLoadingError] = true;
        }
        break;
    default:
        assert(!"Не известный тип результата");
        break;
    }

    // оповещаем об изменении в списке для мониторинга
    get_service<CMassages>().postMessage(onMonitoringListChanged);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::handleUpdatingResult(const MonitoringResult::ResultData& monitoringResult,
                                           ChannelParameters::Ptr& channelParams)
{
    // текст ошибки если она произошла
    CString errorText;

    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // данные успешно получены
        {
            // если данные ещё не были получены
            if (!channelParams->channelState[MonitoringChannelData::eDataLoaded])
                // проставляем новые данные из задания
                channelParams->setTrendChannelData(monitoringResult);
            else
            {
                // мержим старые и новые данные
                {
                    channelParams->trendData.currentValue = monitoringResult.currentValue;

                    if (channelParams->trendData.maxValue < monitoringResult.maxValue)
                        channelParams->trendData.maxValue = monitoringResult.maxValue;
                    if (channelParams->trendData.minValue > monitoringResult.minValue)
                        channelParams->trendData.minValue = monitoringResult.minValue;

                    channelParams->trendData.emptyDataTime += monitoringResult.emptyDataTime;
                    channelParams->trendData.lastDataExistTime = monitoringResult.lastDataExistTime;
                }
            }

            // анализируем новые данные
            {
                // если установлено оповещение и за интервал не было значения меньше
                if (_finite(channelParams->allarmingValue) != 0)
                {
                    // если за интервал все значения вышли за допустимое
                    if ((channelParams->allarmingValue >= 0 &&
                         monitoringResult.minValue >= channelParams->allarmingValue) ||
                         (channelParams->allarmingValue < 0 &&
                          monitoringResult.maxValue <= channelParams->allarmingValue))
                    {
                        errorText.AppendFormat(L"Превышение допустимых значений у канала %s. ",
                                               channelParams->channelName.GetString());

                        channelParams->channelState[MonitoringChannelData::eReportedExcessOfValue] = true;
                    }
                }

                // если данные были. но пропусков очень
                if (monitoringResult.emptyDataTime.GetTotalSeconds() >
                    std::chrono::duration_cast<std::chrono::seconds>(kUpdateDataInterval).count() / 2)
                {
                    errorText.AppendFormat(L"Много пропусков данных у канала %s. ",
                                           channelParams->channelName.GetString());

                    channelParams->channelState[MonitoringChannelData::eReportedALotOfEmptyData] = true;
                }
            }

            channelParams->channelState[MonitoringChannelData::eDataLoaded] = true;
            if (channelParams->channelState[MonitoringChannelData::eLoadingError])
            {
                // если была ошибка при загрузке данных убираем флаг ошибки и сообщаем что данные загружены
                channelParams->channelState[MonitoringChannelData::eLoadingError] = false;

                send_message_to_log(LogMessageData::MessageType::eOrdinary,
                                    L"Данные канала %s получены",
                                    channelParams->channelName.GetString());
            }

            // оповещаем об изменении в списке для мониторинга
            get_service<CMassages>().postMessage(onMonitoringListChanged);
        }
        break;
    case MonitoringResult::Result::eNoData:     // в переданном интервале нет данных
    case MonitoringResult::Result::eErrorText:  // возникла ошибка
        {
            // обновляем время без данных
            channelParams->trendData.emptyDataTime += monitoringResult.emptyDataTime;

            // сообщаем что возникла ошибка загрузки
            if (!channelParams->channelState[MonitoringChannelData::eLoadingError])
            {
                channelParams->channelState[MonitoringChannelData::eLoadingError] = true;

                errorText.AppendFormat(L"Не удалось обновить данные у канала %s. ",
                                       channelParams->channelName.GetString());
            }

            // произошли изменения только если уже были загружены данные
            if (channelParams->channelState[MonitoringChannelData::eDataLoaded])
                // оповещаем об изменении в списке для мониторинга
                get_service<CMassages>().postMessage(onMonitoringListChanged);

            // Если по каналу были загружены данные, а сейчас загрузить не получилось
            // Значит произошло отключение - проверяем было ли оповещено об этом
            if (channelParams->channelState[MonitoringChannelData::eDataLoaded] &&
                !channelParams->channelState[MonitoringChannelData::eReportedFallenOff])
            {
                // если ещё нет таймера на проверку отвалившихся датчиков запускаем его
                if (!CTickHandlerImpl::isTimerExist(TimerType::eFallenOffReporting))
                    // запускаем таймер на время двух апдейтов - ждем что датчики вернутся в строй за это время
                    // или что отвалятся другие датчики
                    CTickHandlerImpl::subscribeTimer(3 * kUpdateDataInterval, TimerType::eFallenOffReporting);
            }
        }
        break;
    default:
        assert(!"Не известный тип результата");
        break;
    }

    if (!errorText.IsEmpty())
    {
        errorText = errorText.Trim();

        // пишем в лог
        send_message_to_log(LogMessageData::MessageType::eError, errorText);

        // оповещаем о возникшей ошибке
        auto errorMessage = std::make_shared<MessageTextData>();
        errorMessage->messageText = std::move(errorText);
        get_service<CMassages>().postMessage(onMonitoringErrorEvent, 0,
                                             std::static_pointer_cast<IEventData>(errorMessage));
    }
}

//----------------------------------------------------------------------------//
void TrendMonitoring::handleEveryDayReportResult(const MonitoringResult::ResultsList& monitoringResults,
                                                 const ChannelParametersList& channelParameters)
{
    assert(!monitoringResults.empty() && !channelParameters.empty());

    // итераторы по параметрам каналов
    ChannelParametersList::const_iterator channelIt = channelParameters.begin(),
                                          channelEnd = channelParameters.end();

    // итераторы по результатам задания
    MonitoringResult::ResultIt resultIt = monitoringResults.begin(),
                               resultEnd = monitoringResults.end();

    // текст с данными мониторинга каналов
    CString channelsReportResult;

    for (; channelIt != channelEnd && resultIt != resultEnd; ++channelIt, ++resultIt)
    {
        // данные мониторинга
        const MonitoringChannelData& monitoringData = (*channelIt)->getMonitoringData();

        CString channelInfo;
        channelInfo.Format(L"Канал \"%s\": ", monitoringData.channelName.GetString());

        switch (resultIt->resultType)
        {
        case MonitoringResult::Result::eSucceeded:  // данные успешно получены
            {
                bool bError = false;

                // если установлено оповещение при превышении значения
                if (_finite(monitoringData.allarmingValue) != 0)
                {
                    // если за интервал одно из значений вышло за допустимые
                    if ((monitoringData.allarmingValue >= 0 &&
                         resultIt->maxValue >= monitoringData.allarmingValue) ||
                         (monitoringData.allarmingValue < 0 &&
                          resultIt->minValue <= monitoringData.allarmingValue))
                    {
                        channelInfo.AppendFormat(L"допустимый уровень был превышен. Допустимое значение %.02f, значения за день [%.02f..%.02f].",
                                                 monitoringData.allarmingValue,
                                                 resultIt->minValue, resultIt->maxValue);

                        bError = true;
                    }
                }

                // если много пропусков данных
                if (resultIt->emptyDataTime.GetTotalHours() > 2)
                {
                    channelInfo.AppendFormat(L"много пропусков данных (%lld ч).",
                                             resultIt->emptyDataTime.GetTotalHours());

                    bError = true;
                }

                // сообщаем только если есть проблемы у датчика
                if (!bError)
                    continue;
            }
            break;
        case MonitoringResult::Result::eNoData:     // в переданном интервале нет данных
        case MonitoringResult::Result::eErrorText:  // возникла ошибка
            {
                // сообщаем что данных нет
                channelInfo += L"нет данных.";
            }
            break;
        default:
            assert(!"Не известный тип результата");
            break;
        }

        channelsReportResult += channelInfo + L"\n";
    }

    // если не о чем сообщать говорим что все ок
    if (channelsReportResult.IsEmpty())
        channelsReportResult = L"Данные в порядке.";

    CString reportDelimer(L'*', 25);

    // создаем сообщение об отчёте
    auto reportMessage = std::make_shared<MessageTextData>();
    reportMessage->messageText.Format(L"%s\n\nЕжедневный отчёт за %s\n\n%s\n%s",
                                      reportDelimer.GetString(),
                                      CTime::GetCurrentTime().Format(L"%d.%m.%Y").GetString(),
                                      channelsReportResult.GetString(),
                                      reportDelimer.GetString());

    // оповещаем о готовом отчёте
    get_service<CMassages>().postMessage(onReportPreparedEvent, 0,
                                         std::static_pointer_cast<IEventData>(reportMessage));

    // сообщаем пользователям телеграма
    m_telegramBot.sendMessageToAdmins(reportMessage->messageText);
}

//----------------------------------------------------------------------------//
// IEventRecipient
void TrendMonitoring::onEvent(const EventId& code, float eventValue,
                              std::shared_ptr<IEventData> eventData)
{
    if (code == onCompletingMonitoringTask)
    {
        std::shared_ptr<MonitoringResult> monitoringResult =
            std::static_pointer_cast<MonitoringResult>(eventData);

        auto it = m_monitoringTasksInfo.find(monitoringResult->m_taskId);
        if (it == m_monitoringTasksInfo.end())
            return;

        switch (it->second.taskType)
        {
        case TaskInfo::TaskType::eIntervalInfo:
        case TaskInfo::TaskType::eUpdatingInfo:
            {
                // итераторы по параметрам каналов
                ChannelIt channelIt = it->second.channelParameters.begin();
                ChannelIt channelEnd = it->second.channelParameters.end();

                // итераторы по результатам задания
                MonitoringResult::ResultIt resultIt = monitoringResult->m_taskResults.begin();
                MonitoringResult::ResultIt resultEnd = monitoringResult->m_taskResults.end();

                // получаем результат и параметры канала по которому запущено задание
                for (; channelIt != channelEnd && resultIt != resultEnd; ++channelIt, ++resultIt)
                {
                    // если прервали запрос данных по каналу но осталось задание у канала будет нулевой указатель
                    if (!*channelIt)
                        continue;

                    switch (it->second.taskType)
                    {
                    case TaskInfo::TaskType::eIntervalInfo:
                        handleIntervalInfoResult(*resultIt, *channelIt);
                        break;
                    case TaskInfo::TaskType::eUpdatingInfo:
                        handleUpdatingResult(*resultIt, *channelIt);
                        break;
                    default:
                        assert(!"Не известный тип задания!");
                        break;
                    }
                }
            }
            break;
        case TaskInfo::TaskType::eEveryDayReport:
            handleEveryDayReportResult(monitoringResult->m_taskResults, it->second.channelParameters);
            break;
        }

        // удаляем задание из списка
        m_monitoringTasksInfo.erase(it);
    }
    else if (code == onUsersListChangedEvent)
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
            CTime currentTime = CTime::GetCurrentTime();

            // проходим по всем каналам и смотрим по каким надо обновить данные
            for (auto& channelParameters : m_appConfig->m_chanelParameters)
            {
                // данные ещё не загружены
                if (!channelParameters->channelState[MonitoringChannelData::eDataLoaded] &&
                    !channelParameters->channelState[MonitoringChannelData::eLoadingError])
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
                    kUpdateDataInterval.count() - 1)
                    continue;

                // запускаем задание обновление интервала c времени последнего обновления до текущего времени
                addMonitoringTaskForChannel(
                    channelParameters, TaskInfo::TaskType::eUpdatingInfo,
                    currentTime - *channelParameters->m_loadingParametersIntervalEnd);
            }
        }
        break;
    case TimerType::eFallenOffReporting:
        {
            // список отвалившихся каналов
            ChannelParametersList fallenOffChannels;

            CTime currentTime = CTime::GetCurrentTime();
            // проходим по всем каналам и смотрим по каким надо сообщить об отваливании
            for (auto& channelParameters : m_appConfig->m_chanelParameters)
            {
                if (!channelParameters->channelState[MonitoringChannelData::eDataLoaded])
                    // данные ещё не загружены
                    continue;
                if (channelParameters->channelState[MonitoringChannelData::eReportedFallenOff])
                    // уже сообщали об отваливании
                    continue;

                // если прошло два обновления данных с момента последних данных по каналу
                if ((currentTime - channelParameters->trendData.lastDataExistTime).GetTotalMinutes() >=
                    2 * kUpdateDataInterval.count())
                    fallenOffChannels.push_back(channelParameters);
            }

            // если есть отвалившиеся каналы
            if (!fallenOffChannels.empty())
            {
                // Собираем текст ошибки
                CString reportText = L"Пропали данные по каналам:";
                for (auto& channelParameters : fallenOffChannels)
                {
                    reportText += L" " + channelParameters->channelName + L";";

                    channelParameters->channelState[MonitoringChannelData::eReportedFallenOff] = true;
                }

                // пишем ошибку в лог
                send_message_to_log(LogMessageData::MessageType::eError, reportText);

                // оповещаем о возникшей ошибке
                auto errorMessage = std::make_shared<MessageTextData>();
                errorMessage->messageText = std::move(reportText);
                get_service<CMassages>().postMessage(onMonitoringErrorEvent, 0,
                                                     std::static_pointer_cast<IEventData>(errorMessage));
            }
        }
        // исполняемый один раз таймер
        return false;
    case TimerType::eEveryDayReporting:
        {
            // Подключаем таймер с интервалом до след отчёта
            CTickHandlerImpl::subscribeTimer(std::chrono::hours(24),
                                             TimerType::eEveryDayReporting);

            if (!m_appConfig->m_chanelParameters.empty())
            {
                // копия текущих каналов по которым запускаем мониторинг
                ChannelParametersList channelsCopy;
                for (const auto& currentChannel : m_appConfig->m_chanelParameters)
                {
                    channelsCopy.push_back(ChannelParameters::make(currentChannel->channelName));
                    channelsCopy.back()->allarmingValue = currentChannel->allarmingValue;
                }

                // запускаем задание формирования отчёта за последний день
                addMonitoringTaskForChannels(channelsCopy,
                                             TaskInfo::TaskType::eEveryDayReport,
                                             CTimeSpan(1, 0, 0, 0));
            }
        }
        // исполняемый один раз таймер
        return false;
    default:
        assert(!"Неизвестный таймер!");
        break;
    }

    return true;
}

//----------------------------------------------------------------------------//
void TrendMonitoring::saveConfiguration()
{
    ISerializator::Ptr serializator =
        SerializationFabric::createXMLSerializator(getConfigurationXMLFilePath());

    SerializationExecutor serializationExecutor;
    serializationExecutor.serializeObject(serializator, m_appConfig);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::loadConfiguration()
{
    IDeserializator::Ptr deserializator =
        SerializationFabric::createXMLDeserializator(getConfigurationXMLFilePath());

    SerializationExecutor serializationExecutor;
    serializationExecutor.deserializeObject(deserializator, m_appConfig);
}

//----------------------------------------------------------------------------//
CString TrendMonitoring::getConfigurationXMLFilePath()
{
    return get_service<ZetDirsService>().getCurrentDir() + kConfigFileName;
}

//----------------------------------------------------------------------------//
void TrendMonitoring::onMonitoringChannelsListChanged(bool bAsynchNotify /*= true*/)
{
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
                                                  const TaskInfo::TaskType taskType,
                                                  CTimeSpan monitoringInterval /* = -1*/)
{
    if (monitoringInterval == -1)
        monitoringInterval =
            monitoring_interval_to_timespan(channelParams->monitoringInterval);

    addMonitoringTaskForChannels({ channelParams }, taskType, monitoringInterval);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                                   const TaskInfo::TaskType taskType,
                                                   CTimeSpan monitoringInterval)
{
    if (channelList.empty())
    {
        assert(!"Передали пустой список каналов для задания!");
        return;
    }

    // формируем интервалы по которым будем запускать задание
    CTime stopTime = CTime::GetCurrentTime();
    CTime startTime = stopTime - monitoringInterval;

    // список параметров заданий
    std::list<TaskParameters::Ptr> listTaskParams;

    // запоминаем конец интервала по которому были загружены данные
    for (auto& channelParams : channelList)
    {
        channelParams->m_loadingParametersIntervalEnd = stopTime;

        listTaskParams.emplace_back(new TaskParameters(channelParams->channelName,
                                                       startTime, stopTime));
    }

    TaskInfo taskInfo;
    taskInfo.taskType = taskType;
    taskInfo.channelParameters = channelList;

    m_monitoringTasksInfo.try_emplace(
        get_monitoing_tasks_service()->addTaskList(listTaskParams,
                                                   IMonitoringTasksService::eNormal),
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
        case TaskInfo::TaskType::eIntervalInfo:
        case TaskInfo::TaskType::eUpdatingInfo:
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
                    if (std::all_of(taskChannels.begin(), taskChannels.end(),
                                    [](const auto& el)
                                    {
                                        return el == nullptr;
                                    }))
                    {
                        // не пустых каналов не осталось - будем удалять задание

                        get_monitoing_tasks_service()->removeTask(monitoringTaskIt->first);
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
