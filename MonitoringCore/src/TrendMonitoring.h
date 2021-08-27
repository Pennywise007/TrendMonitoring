#pragma once

#include <chrono>

#include <Messages.h>
#include <Singleton.h>
#include <TickService.h>

#include <include/IMonitoringTasksService.h>
#include <include/ITrendMonitoring.h>
#include <include/ITelegramBot.h>

#include "ApplicationConfiguration.h"
#include "TrendMonitoringTask.h"

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

    // интервал обновления данных
    static std::chrono::minutes getUpdateDataInterval() { return std::chrono::minutes(5); }

// ITrendMonitoring
public:
    /// <summary>Получить список имен всех меющихся каналов.</summary>
    [[nodiscard]] std::set<CString> getNamesOfAllChannels() const override;
    /// <summary>Получить список имен каналов по которым происходит мониторинг.</summary>
    [[nodiscard]] std::list<CString> getNamesOfMonitoringChannels() const override;

    /// <summary>Обновить данные для всех каналов.</summary>
    void updateDataForAllChannels() override;

    /// <summary>Получить количества наблюдаемых каналов.</summary>
    size_t getNumberOfMonitoringChannels() const override;
    /// <summary>Получить данные для наблюдаемого канала по индексу.</summary>
    /// <param name="channelIndex">Индекс в списке каналов.</param>
    /// <returns>Данные для канала.</returns>
    [[nodiscard]] const MonitoringChannelData& getMonitoringChannelData(const size_t channelIndex) const override;

    /// <summary>Добавить канал для мониторинга.</summary>
    /// <returns>Индекс добавленного канала в списке.</returns>
    size_t addMonitoringChannel() override;
    /// <summary>Удалить наблюдаемый канал из списка по индексу.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <returns>Индекс выделения после удаления.</returns>
    size_t removeMonitoringChannelByIndex(const size_t channelIndex) override;

    /// <summary>Изменить флаг оповещения у канала по номеру.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newNotifyState">Новое состояние оповещения.</param>
    void changeMonitoringChannelNotify(const size_t channelIndex,
                                       const bool newNotifyState) override;
    /// <summary>Изменить имя наблюдаемого канала.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newChannelName">Новое имя канала.</param>
    void changeMonitoringChannelName(const size_t channelIndex,
                                     const CString& newChannelName) override;
    /// <summary>Изменить интервал мониторинга данных для наблюдаемого канала.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newInterval">Новый интервал мониторинга.</param>
    void changeMonitoringChannelInterval(const size_t channelIndex,
                                         const MonitoringInterval newInterval) override;
    /// <summary>Изменить значение по достижению которого будет произведено оповещение.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newValue">Новое значение с оповещением.</param>
    void changeMonitoringChannelAlarmingValue(const size_t channelIndex,
                                              const float newValue) override;

    /// <summary>Передвинуть вверх по списку канал по индексу.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <returns>Новый индекс канала.</returns>
    size_t moveUpMonitoringChannelByIndex(const size_t channelIndex) override;
    /// <summary>Передвинуть вниз по списку канал по индексу.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <returns>Новый индекс канала.</returns>
    size_t moveDownMonitoringChannelByIndex(const size_t channelIndex) override;

    // Получить настройки бота телеграма
    [[nodiscard]] const telegram::bot::TelegramBotSettings& getBotSettings() const override;
    // установить настройки бота телеграма
    void setBotSettings(const telegram::bot::TelegramBotSettings& newSettings) override;

// IEventRecipient
public:
    // оповещение о произошедшем событии
    void onEvent(const EventId& code, float eventValue,
                 const std::shared_ptr<IEventData>& eventData) override;

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

// tests
public:
    void installTelegramBot(const std::shared_ptr<telegram::bot::ITelegramBot>& telegramBot);

    [[nodiscard]] ApplicationConfiguration& getConfiguration() const { return *m_appConfig; }

// работа со списком каналов и настройками
private:
    // сохранение текущих настроек программы
    void saveConfiguration();
    // загрузка настроек программы
    void loadConfiguration();
    // Получить путь к файлу XML с настройками приложения
    [[nodiscard]] CString getConfigurationXMLFilePath() const;
    // вызывается при изменении в списке наблюдаемых каналов
    // bAsynchNotify - флаг что надо оповестить об изменении в списке каналов асинхронно
    void onMonitoringChannelsListChanged(bool bAsynchNotify = true);

// работа с заданиями мониторинга
private:
    // Добавить задание мониторинга для канала
    // @param channelParams - параметры канала по которому надо запустить задание мониторинга
    // @param taskType - тип выполняемого задания
    // @param monitoringInterval - интервал времени с настоящего момента до начала мониторинга
    // если -1 - используется channelParams->monitoringInterval
    void addMonitoringTaskForChannel(ChannelParameters::Ptr& channelParams,
                                     const MonitoringTaskInfo::TaskType taskType,
                                     CTimeSpan monitoringInterval = -1);
    // Добавить задание мониторинга для списка каналов
    void addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                      const MonitoringTaskInfo::TaskType taskType,
                                      CTimeSpan monitoringInterval);
    // Добавить задания мониторинга, каждому таску соответствует канал
    void addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                      const std::list<TaskParameters::Ptr>& taskParams,
                                      const MonitoringTaskInfo::TaskType taskType);

    // Удалить задания запроса новых данных мониторинга для указанного канала
    void delMonitoringTaskForChannel(const ChannelParameters::Ptr& channelParams);

private:
    // мапа с соответствием идентификатора задания и параметрами задания
    std::map<TaskId, MonitoringTaskInfo, TaskComparer> m_monitoringTasksInfo;

private:
    // бот для работы с телеграмом
    std::shared_ptr<telegram::bot::ITelegramBot> m_telegramBot;

private:
    // данные приложения
    ApplicationConfiguration::Ptr m_appConfig = ApplicationConfiguration::create();
};

