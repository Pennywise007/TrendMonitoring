#include "pch.h"

#include <ctime>

#include <myInclude/ChannelDataCorrector/ChannelDataGetter.h>
#include <DirsService.h>
#include "Serialization/SerializatorFabric.h"
#include "TrendMonitoring.h"
#include "Utils.h"

// интервал обновления данных
const std::chrono::minutes kUpdateDataInterval(5);

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
    EventRecipientImpl::subscribe(onUsersListChangedEvent);

    // загружаем конфигурацию из файла
    loadConfiguration();

    // инициализируем бота после загрузки конфигурации
    m_telegramBot.initBot(m_appConfig->getTelegramUsers());
    // передаем боту настройки из конфигов
    m_telegramBot.setBotSettings(getBotSettings());

    // запускам задания для мониторинга, делаем отдельными заданиями ибо могут
    // долго грузиться данные для интервалов
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
std::set<CString> TrendMonitoring::getNamesOfAllChannels()
{
    OUTPUT_LOG_FUNC_ENTER;

    // получаем все данные по каналам
    std::map<CString, CString> channelsWithConversion;
    // ищем именно в директории с сжатыми сигналами ибо там меньше вложенность и поиск идёт быстрее
    CChannelDataGetter::FillChannelList(get_service<DirsService>().getZetCompressedDir(), channelsWithConversion);

    // заполняем сортированный список каналов
    std::set<CString> allChannelsNames;
    for (const auto& channelInfo : channelsWithConversion)
        allChannelsNames.emplace(channelInfo.first);

    return std::move(allChannelsNames);
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
    addMonitoringTaskForChannel(channelParams, TaskInfo::TaskType::eIntervalInfo);

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
    addMonitoringTaskForChannel(channelParams, TaskInfo::TaskType::eIntervalInfo);

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
bool TrendMonitoring::handleIntervalInfoResult(const MonitoringResult::ResultData& monitoringResult,
                                               ChannelParameters* channelParameters,
                                               CString& alertText)
{
    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // данные успешно получены
        {
            // проставляем новые данные из задания
            channelParameters->setTrendChannelData(monitoringResult);

            channelParameters->channelState[MonitoringChannelData::eDataLoaded] = true;
        }
        break;
    case MonitoringResult::Result::eNoData:     // в переданном интервале нет данных
    case MonitoringResult::Result::eErrorText:  // возникла ошибка
        {
            // оповещаем о возникшей ошибке
            if (monitoringResult.resultType == MonitoringResult::Result::eErrorText &&
                !monitoringResult.errorText.IsEmpty())
                alertText = monitoringResult.errorText;
            else
                alertText = L"Нет данных в запрошенном интервале.";

            // обновляем время без данных
            channelParameters->trendData.emptyDataTime = monitoringResult.emptyDataTime;
            // сообщаем что возникла ошибка загрузки
            channelParameters->channelState[MonitoringChannelData::eLoadingError] = true;
        }
        break;
    default:
        assert(!"Не известный тип результата");
        break;
    }

    // всегда есть что изменилось
    return true;
}

//----------------------------------------------------------------------------//
bool TrendMonitoring::handleUpdatingResult(const MonitoringResult::ResultData& monitoringResult,
                                           ChannelParameters* channelParameters,
                                           CString& alertText)
{
    bool bDataChanged = false;
    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // данные успешно получены
        {
            // если данные ещё не были получены
            if (!channelParameters->channelState[MonitoringChannelData::eDataLoaded])
                // проставляем новые данные из задания
                channelParameters->setTrendChannelData(monitoringResult);
            else
            {
                // мержим старые и новые данные
                {
                    channelParameters->trendData.currentValue = monitoringResult.currentValue;

                    if (channelParameters->trendData.maxValue < monitoringResult.maxValue)
                        channelParameters->trendData.maxValue = monitoringResult.maxValue;
                    if (channelParameters->trendData.minValue > monitoringResult.minValue)
                        channelParameters->trendData.minValue = monitoringResult.minValue;

                    channelParameters->trendData.emptyDataTime += monitoringResult.emptyDataTime;
                    channelParameters->trendData.lastDataExistTime = monitoringResult.lastDataExistTime;
                }
            }

            // анализируем новые данные
            {
                // если установлено оповещение и за интервал не было значения меньше
                if (_finite(channelParameters->alarmingValue) != 0)
                {
                    // если за интервал все значения вышли за допустимое
                    if ((channelParameters->alarmingValue >= 0 &&
                         monitoringResult.minValue >= channelParameters->alarmingValue) ||
                         (channelParameters->alarmingValue < 0 &&
                          monitoringResult.maxValue <= channelParameters->alarmingValue))
                    {
                        alertText.AppendFormat(L"Превышение допустимых значений. Допустимое значение %.02f, значения [%.02f..%.02f].",
                                                channelParameters->alarmingValue,
                                                monitoringResult.minValue, monitoringResult.maxValue);
                        channelParameters->channelState[MonitoringChannelData::eReportedExcessOfValue] = true;
                    }
                }

                // проверяем количество пропусков, оповещаем если секунд без данных больше чем половина времени обновления интервала
                if (auto emptySeconds = monitoringResult.emptyDataTime.GetTotalMinutes();
                    emptySeconds > kUpdateDataInterval.count() / 2)
                {
                    alertText.Append(CString(alertText.IsEmpty() ? L"" : L" ") + L"Много пропусков данных.");

                    channelParameters->channelState[MonitoringChannelData::eReportedALotOfEmptyData] = true;
                }
            }

            channelParameters->channelState[MonitoringChannelData::eDataLoaded] = true;
            if (channelParameters->channelState[MonitoringChannelData::eLoadingError])
            {
                if (channelParameters->channelState[MonitoringChannelData::eReportedFallenOff])
                {
                    // если пользователю сказали что данных нет и их получили - сообщаем радостную новость.
                    alertText.Append(CString(alertText.IsEmpty() ? L"" : L" ") + L"Данные получены.");
                    channelParameters->channelState[MonitoringChannelData::eReportedFallenOff] = false;
                }
                else
                    // т.к. писали о том что данные не удалось обновить - напишем что данные получены
                    send_message_to_log(LogMessageData::MessageType::eOrdinary,
                                        L"Канал \"%s\": Данные получены.",
                                        channelParameters->channelName.GetString());

                // если была ошибка при загрузке данных убираем флаг ошибки и сообщаем что данные загружены
                channelParameters->channelState[MonitoringChannelData::eLoadingError] = false;
            }

            // оповещаем об изменении в списке для мониторинга
            bDataChanged = true;
        }
        break;
    case MonitoringResult::Result::eNoData:     // в переданном интервале нет данных
    case MonitoringResult::Result::eErrorText:  // возникла ошибка
        {
            // обновляем время без данных
            channelParameters->trendData.emptyDataTime += monitoringResult.emptyDataTime;

            // сообщаем что возникла ошибка загрузки
            if (!channelParameters->channelState[MonitoringChannelData::eLoadingError])
            {
                send_message_to_log(LogMessageData::MessageType::eOrdinary,
                                    L"Канал \"%s\": не удалось обновить данные.",
                                    channelParameters->channelName.GetString());

                channelParameters->channelState[MonitoringChannelData::eLoadingError] = true;
            }

            // произошли изменения только если уже были загружены данные
            if (channelParameters->channelState[MonitoringChannelData::eDataLoaded])
                // оповещаем об изменении в списке для мониторинга
                bDataChanged = true;

            // Если по каналу были загружены данные, а сейчас загрузить не получилось
            // Значит произошло отключение - проверяем было ли оповещено об этом
            if (channelParameters->channelState[MonitoringChannelData::eDataLoaded] &&
                !channelParameters->channelState[MonitoringChannelData::eReportedFallenOff])
            {
                // если прошло 3 обновления данных с момента последних данных по каналу, а данных все ещё нет - датчик отвалился
                if ((CTime::GetCurrentTime() - channelParameters->trendData.lastDataExistTime).GetTotalMinutes() >=
                    3 * kUpdateDataInterval.count())
                {
                    alertText = L"Пропали данные по каналу.";
                    channelParameters->channelState[MonitoringChannelData::eReportedFallenOff] = true;
                }
            }
        }
        break;
    default:
        assert(!"Не известный тип результата");
        break;
    }

    return bDataChanged;
}

//----------------------------------------------------------------------------//
bool TrendMonitoring::handleEveryDayReportResult(const MonitoringResult::ResultData& monitoringResult,
                                                 ChannelParameters* channelParameters,
                                                 CString& alertText)
{
    // данные мониторинга
    const MonitoringChannelData& monitoringData = channelParameters->getMonitoringData();

    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // данные успешно получены
        {
            // если установлено оповещение при превышении значения
            if (_finite(monitoringData.alarmingValue) != 0)
            {
                // если за интервал одно из значений вышло за допустимые
                if ((monitoringData.alarmingValue >= 0 &&
                     monitoringResult.maxValue >= monitoringData.alarmingValue) ||
                     (monitoringData.alarmingValue < 0 &&
                      monitoringResult.minValue <= monitoringData.alarmingValue))
                {
                    alertText.AppendFormat(L"Допустимый уровень был превышен. Допустимое значение %.02f, значения за день [%.02f..%.02f].",
                                            monitoringData.alarmingValue,
                                            monitoringResult.minValue, monitoringResult.maxValue);
                }
            }

            // если много пропусков данных
            if (monitoringResult.emptyDataTime.GetTotalHours() > 2)
                alertText.AppendFormat(L"много пропусков данных (%lld ч).",
                                        monitoringResult.emptyDataTime.GetTotalHours());
        }
        break;
    case MonitoringResult::Result::eNoData:     // в переданном интервале нет данных
    case MonitoringResult::Result::eErrorText:  // возникла ошибка
        // сообщаем что данных нет
        alertText = L"Нет данных.";
        break;
    default:
        assert(!"Не известный тип результата");
        break;
    }

    // данные обновляются при получении других результатов
    return false;
}

//----------------------------------------------------------------------------//
// IEventRecipient
void TrendMonitoring::onEvent(const EventId& code, float eventValue,
                              std::shared_ptr<IEventData> eventData)
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
        ChannelIt channelEnd = it->second.channelParameters.end();

        // итераторы по результатам задания
        MonitoringResult::ResultIt resultIt  = monitoringResult->m_taskResults.begin();
        MonitoringResult::ResultIt resultEnd = monitoringResult->m_taskResults.end();

        // флаг что в списке мониторинга были изменения
        bool bMonitoringListChanged = false;
        // текст ошибок по всем каналам
        CString reportText;

        // для каждого канала анализируем его результат задания
        for (; channelIt != channelEnd && resultIt != resultEnd; ++channelIt, ++resultIt)
        {
            assert((*channelIt)->channelName == resultIt->pTaskParameters->channelName &&
                   "Получены результаты для другого канала!");

            CString channelError;

            switch (it->second.taskType)
            {
            case TaskInfo::TaskType::eIntervalInfo:
                bMonitoringListChanged |= handleIntervalInfoResult(*resultIt, *channelIt, channelError);
                break;
            case TaskInfo::TaskType::eUpdatingInfo:
                bMonitoringListChanged |= handleUpdatingResult(*resultIt, *channelIt, channelError);
                break;
            case TaskInfo::TaskType::eEveryDayReport:
                bMonitoringListChanged |= handleEveryDayReportResult(*resultIt, *channelIt, channelError);
                break;
            }

            // если возникла ошибка при получении данных и можно о ней оповещать
            if (!channelError.IsEmpty() && (*channelIt)->bNotify)
            {
                reportText.AppendFormat(L"Канал \"%s\": %s\n",
                                        (*channelIt)->channelName.GetString(),
                                        channelError.GetString());
            }
        }

        reportText = reportText.Trim();

        // если возникли ошибки отрабатываем их по разному для каждого типа задания
        if (!reportText.IsEmpty() || it->second.taskType == TaskInfo::TaskType::eEveryDayReport)
        {
            switch (it->second.taskType)
            {
            case TaskInfo::TaskType::eIntervalInfo:
            case TaskInfo::TaskType::eUpdatingInfo:
                {
                    // сообщаем в лог что возникли проблемы
                    send_message_to_log(LogMessageData::MessageType::eError, reportText.GetString());

                    // оповещаем о возникшей ошибке
                    auto errorMessage = std::make_shared<MonitoringErrorEventData>();
                    errorMessage->errorText = std::move(reportText);
                    // генерим идентификатор ошибки
                    if (!SUCCEEDED(CoCreateGuid(&errorMessage->errorGUID)))
                        assert(!"Не удалось создать гуид!");
                    get_service<CMassages>().postMessage(onMonitoringErrorEvent, 0,
                                                         std::static_pointer_cast<IEventData>(errorMessage));
                }
                break;
            case TaskInfo::TaskType::eEveryDayReport:
                {
                    // если не о чем сообщать говорим что все ок
                    if (reportText.IsEmpty())
                        reportText = L"Данные в порядке.";

                    CString reportDelimer(L'*', 25);

                    // создаем сообщение об отчёте
                    auto reportMessage = std::make_shared<MessageTextData>();
                    reportMessage->messageText.Format(L"%s\n\nЕжедневный отчёт за %s\n\n%s\n%s",
                                                      reportDelimer.GetString(),
                                                      CTime::GetCurrentTime().Format(L"%d.%m.%Y").GetString(),
                                                      reportText.GetString(),
                                                      reportDelimer.GetString());

                    // оповещаем о готовом отчёте
                    get_service<CMassages>().postMessage(onReportPreparedEvent, 0,
                                                         std::static_pointer_cast<IEventData>(reportMessage));

                    // сообщаем пользователям телеграма
                    m_telegramBot.sendMessageToAdmins(reportMessage->messageText);
                }
                break;
            }
        }

        if (bMonitoringListChanged)
            // оповещаем об изменении в списке для мониторинга
            get_service<CMassages>().postMessage(onMonitoringListChanged);

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

            // т.к. каналы могли добавляться в разное время будем для каждого канала делать
            // свое задание с определенным интервалом, формиурем список параметров заданий
            std::list<TaskParameters::Ptr> listTaskParams;
            // список обновляемых каналов
            ChannelParametersList updatingDataChannels;

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

                // добавляем канал в список обновляемых
                updatingDataChannels.emplace_back(channelParameters);
                listTaskParams.emplace_back(new TaskParameters(channelParameters->channelName,
                                                               *channelParameters->m_loadingParametersIntervalEnd,
                                                               currentTime));
            }

            if (!listTaskParams.empty())
                addMonitoringTaskForChannels(updatingDataChannels, listTaskParams,
                                             TaskInfo::TaskType::eUpdatingInfo);
        }
        break;
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
                    channelsCopy.back()->alarmingValue = currentChannel->alarmingValue;
                }

                // запускаем задание формирования отчёта за последний день
                addMonitoringTaskForChannels(channelsCopy,
                                             TaskInfo::TaskType::eEveryDayReport,
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
                                                  const TaskInfo::TaskType taskType,
                                                  CTimeSpan monitoringInterval /* = -1*/)
{
    OUTPUT_LOG_FUNC;
    OUTPUT_LOG_ADD_COMMENT("channel name = %s", channelParams->channelName.GetString());
    OUTPUT_LOG_DO;

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
    // формируем интервалы по которым будем запускать задание
    CTime stopTime = CTime::GetCurrentTime();
    CTime startTime = stopTime - monitoringInterval;

    // список параметров заданий
    std::list<TaskParameters::Ptr> listTaskParams;
    for (auto& channelParams : channelList)
    {
        listTaskParams.emplace_back(new TaskParameters(channelParams->channelName,
                                                       startTime, stopTime));
    }

    addMonitoringTaskForChannels(channelList, listTaskParams, taskType);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                                   const std::list<TaskParameters::Ptr>& taskParams,
                                                   const TaskInfo::TaskType taskType)
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

    if (taskType != TaskInfo::TaskType::eEveryDayReport)
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

    TaskInfo taskInfo;
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
