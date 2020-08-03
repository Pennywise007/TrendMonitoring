#pragma once

#include "Messages.h"
#include "Singleton.h"
#include "TickService.h"

#include "ApplicationConfiguration.h"
#include "IMonitoringTasksService.h"
#include "ITrendMonitoring.h"

#include "TelegramBot.h"

////////////////////////////////////////////////////////////////////////////////
// Реализация сервиса для мониторинга каналов
// Осуществяет управление списком наблюдаемых каналов, выполенение заданий для мониторинга
class TrendMonitoring
    : public ITrendMonitoring
    , public EventRecipientImpl
    , public CTickHandlerImpl
{
    friend class CSingleton<TrendMonitoring>;

public:
    TrendMonitoring();

// ITrendMonitoring
public:
    /// <summary>Получить список имен всех меющихся каналов.</summary>
    std::set<CString> getNamesOfAllChannels() override;
    /// <summary>Получить список имен каналов по которым происходит мониторинг.</summary>
    std::list<CString> getNamesOfMonitoringChannels() override;

    /// <summary>Обновить данные для всех каналов.</summary>
    void updateDataForAllChannels() override;

    /// <summary>Получить количества наблюдаемых каналов.</summary>
    size_t getNumberOfMonitoringChannels() override;
    /// <summary>Получить данные для наблюдаемого канала по индексу.</summary>
    /// <param name="channelIndex">Индекс в списке каналов.</param>
    /// <returns>Данные для канала.</returns>
    const MonitoringChannelData& getMonitoringChannelData(const size_t channelIndex) override;

    /// <summary>Добавить канал для мониторинга.</summary>
    /// <returns>Индекс добавленного канала в списке.</returns>
    size_t addMonitoingChannel() override;
    /// <summary>Удалить наблюдаемый канал из списка по индексу.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <returns>Индекс выделения после удаления.</returns>
    size_t removeMonitoringChannelByIndex(const size_t channelIndex) override;

    /// <summary>Изменить флаг оповещения у канала по номеру.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newNotifyState">Новое состояние оповещения.</param>
    void changeMonitoingChannelNotify(const size_t channelIndex,
                                      const bool newNotifyState) override;
    /// <summary>Изменить имя наблюдаемого канала.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newChannelName">Новое имя канала.</param>
    void changeMonitoingChannelName(const size_t channelIndex,
                                    const CString& newChannelName) override;
    /// <summary>Изменить интервал мониторинга данных для наблюдаемого канала.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newInterval">Новый интервал мониторинга.</param>
    void changeMonitoingChannelInterval(const size_t channelIndex,
                                        const MonitoringInterval newInterval) override;
    /// <summary>Изменить значение по достижению которого будет произведено оповещение.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newValue">Новое значение с оповещением.</param>
    void changeMonitoingChannelAllarmingValue(const size_t channelIndex,
                                              const float newValue) override;

    /// <summary>Передвинуть вверх по списку канал по индексу.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <returns>Новый индекс канала.</returns>
    size_t moveUpMonitoingChannelByIndex(const size_t channelIndex) override;
    /// <summary>Передвинуть вниз по списку канал по индексу.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <returns>Новый индекс канала.</returns>
    size_t moveDownMonitoingChannelByIndex(const size_t channelIndex) override;

    // Получить настройки бота телеграма
    const TelegramBotSettings& getBotSettings() override;
    // установить настройки бота телеграма
    void setBotSettings(const TelegramBotSettings& newSettings) override;

// IEventRecipient
public:
    // оповещение о произошедшем событии
    void onEvent(const EventId& code, float eventValue,
                 std::shared_ptr<IEventData> eventData) override;

// ITickHandler
public:
    // типы таймера
    enum TimerType
    {
        eUpdatingData = 0,      // таймер обновления текущих данных
        eEveryDayReporting      // таймер с ежедневным отчетом
    };

    /// <summary>Вызывается при тике таймера.</summary>
    /// <param name="tickParam">Параметр вызванного таймера.</param>
    /// <returns>true если все ок, false если надо прекратить этот таймер.</returns>
    bool onTick(TickParam tickParam) override;

// работа со списком каналов и настройками
private:
    // сохранение текущих настроек программы
    void saveConfiguration();
    // загрузка настроек программы
    void loadConfiguration();
    // Получить путь к файлу XML с настройками приложения
    CString getConfigurationXMLFilePath();
    // вызывается при изменении в списке наблюдаемых каналов
    // bAsynchNotify - флаг что надо оповестить об изменении в списке каналов асинхронно
    void onMonitoringChannelsListChanged(bool bAsynchNotify = true);

// работа с заданиями мониторинга
private:
    // структура с параметрами задания
    struct TaskInfo
    {
        // тип выполняемого задания
        enum class TaskType
        {
            eIntervalInfo = 0,      // Запрос данных за указанный интервал(с перезаписью)
            eUpdatingInfo,          // Запрос новых данных(обновление существующей информации)
                                    // Происходит по таймеру раз в 5 минут
            eEveryDayReport         // Запрос данных для ежедневного отчёта
        } taskType = TaskType::eIntervalInfo;

        // параметры каналов по которым выполеняется задание
        ChannelParametersList channelParameters;
    };

    // Добавить задание мониторинга для канала
    // @param channelParams - параметры канала по которому надо запустить задание мониторинга
    // @param taskType - тип выполняемого задания
    // @param monitoringInterval - интервал времени с настоящего момента до начала мониторинга
    // если -1 - используется channelParams->monitoringInterval
    void addMonitoringTaskForChannel(ChannelParameters::Ptr& channelParams,
                                     const TaskInfo::TaskType taskType,
                                     CTimeSpan monitoringInterval = -1);
    // Добавить задание мониторинга для списка каналов
    void addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                      const TaskInfo::TaskType taskType,
                                      CTimeSpan monitoringInterval);
    // Добавить задания мониторинга, каждому таску соответствует канал
    void addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                      const std::list<TaskParameters::Ptr>& taskParams,
                                      const TaskInfo::TaskType taskType);

    // Удалить задания запроса новых данных мониторинга для указанного канала
    void delMonitoringTaskForChannel(const ChannelParameters::Ptr& channelParams);


    // Обработка результатов мониторинга для канала
    // @param monitoringResult - результат мониторинга
    // @param channelParameters - параметры канала
    // @param allertText - текст о котором нужно сообщить
    // @return true - в случае необходимости обновить данные по каналам
    bool handleIntervalInfoResult(const MonitoringResult::ResultData& monitoringResult,
                                  ChannelParameters* channelParameters,
                                  CString& allertText);   // запрос данных за интервал
    bool handleUpdatingResult(const MonitoringResult::ResultData& monitoringResult,
                              ChannelParameters* channelParameters,
                              CString& allertText);       // обновление данных
    bool handleEveryDayReportResult(const MonitoringResult::ResultData& monitoringResult,
                                    ChannelParameters* channelParameters,
                                    CString& allertText); // ежедневный отчёт

private:
    // мапа с соответствием идентификатора задания и параметрами задания
    std::map<TaskId, TaskInfo, TaskComparer> m_monitoringTasksInfo;

private:
    // бот для работы с телеграмом
    CTelegramBot m_telegramBot;

private:
    // данные приложения
    ApplicationConfiguration::Ptr m_appConfig = ApplicationConfiguration::create();
};

