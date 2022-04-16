#pragma once

#include <chrono>

#include <ext/core/dependency_injection.h>
#include <ext/thread/scheduler.h>
#include <ext/thread/tick.h>

#include <include/IMonitoringTasksService.h>
#include <include/ITrendMonitoring.h>
#include <include/ITelegramBot.h>
#include <include/ITelegramUsersList.h>

#include "ApplicationConfiguration.h"

#include "MonitoringTaskService/MonitoringTaskResultHandler.h"

// Implementation of a service for channel monitoring
// Manages the list of monitored channels, performs tasks for monitoring
class TrendMonitoring
    : public ITrendMonitoring
    , ext::events::ScopeSubscription<telegram::users::ITelegramUserChangedEvent,
                                     telegram::bot::ISettingsChangedEvent,
                                     IMonitoringTaskEvent>
    , public ext::tick::TickSubscriber
{
public:
    TrendMonitoring(ext::ServiceProvider::Ptr&& serviceProvider,
                    std::shared_ptr<IMonitoringTasksService>&& monitoringTasksService);

    // update interval
    inline const static std::chrono::minutes UpdateDataInterval = std::chrono::minutes(5);

// ITrendMonitoring
public:
#pragma region General functions above the channel list
    /// <summary>Get a list of names of all channels available for monitoring.</summary>
    EXT_NODISCARD std::list<std::wstring> GetNamesOfAllChannels() const override;
    /// <summary>Get a list of channel names that are being monitored.</summary>
    EXT_NODISCARD std::list<std::wstring> GetNamesOfMonitoringChannels() const override;

    /// <summary>Update data for all channels.</summary>
    void UpdateDataForAllChannels() override;

    /// <summary>Get the number of observed channels.</summary>
    EXT_NODISCARD size_t GetNumberOfMonitoringChannels() const override;
    /// <summary>Get data for the monitored channel by index.</summary>
    /// <param name="channelIndex">Index in the list of channels.</param>
    /// <returns>Data for the channel.</returns>
    EXT_NODISCARD const MonitoringChannelData& GetMonitoringChannelData(const size_t channelIndex) const override;
#pragma endregion General functions above the channel list

#pragma region Channel list management
    /// <summary>Add a channel for monitoring.</summary>
    /// <returns>Index of the added channel in the list.</returns>
    size_t AddMonitoringChannel() override;
    /// <summary>Remove the observed channel from the list by index.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <returns>Index of selection after deletion.</returns>
    size_t RemoveMonitoringChannelByIndex(const size_t channelIndex) override;

    /// <summary>Change the notification flag of the channel by number.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <param name="newNotifyState">New notification state.</param>
    void ChangeMonitoringChannelNotify(const size_t channelIndex,
                                       const bool newNotifyState) override;
    /// <summary>Change the name of the monitored channel.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <param name="newChannelName">New channel name.</param>
    void ChangeMonitoringChannelName(const size_t channelIndex,
                                     const std::wstring& newChannelName) override;
    /// <summary>Change the data monitoring interval for the observed channel.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <param name="newInterval">New monitoring interval.</param>
    void ChangeMonitoringChannelInterval(const size_t channelIndex,
                                         const MonitoringInterval newInterval) override;
    /// <summary>Change the value upon reaching which a notification will be generated.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <param name="newValue">New value with notification.</param>
    void ChangeMonitoringChannelAlarmingValue(const size_t channelIndex,
                                              const float newValue) override;

    /// <summary>Move the channel up the list by index.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <returns>New channel index.</returns>
    size_t MoveUpMonitoringChannelByIndex(const size_t channelIndex) override;
    /// <summary>Move the channel down the list by index.</summary>
    /// <param name="channelIndex">Channel index in the channel list.</param>
    /// <returns>New channel index.</returns>
    size_t MoveDownMonitoringChannelByIndex(const size_t channelIndex) override;
#pragma endregion Channel list management

// ITelegramUserChangedEvent
private:
    void OnChanged() override;

// ISettingsChangedEvent
private:
    void OnBotSettingsChanged(const bool newEnableValue, const std::wstring& newBotToken) override;

// IMonitoringTaskEvent
public:
    // Event about finishing monitoring task
    void OnCompleteTask(const TaskId& taskId, ResultsPtrList monitoringResult) override;

// ITickHandler
public:
    enum TimerType
    {
        eUpdatingData = 0,          // current data update timer
        eEveryDayReporting = 123    // timer with daily report
    };

    /// <summary>Called when the timer ticks</summary>
    /// <param name="tickParam">Parameter passed during subscription.</param>
    void OnTick(ext::tick::TickParam tickParam) EXT_NOEXCEPT override;

    // work with the list of channels and settings
private:
    // saving the current program settings
    void SaveConfiguration() EXT_NOEXCEPT;
    // load program settings
    void LoadConfiguration() EXT_NOEXCEPT;
    // Get the path to the XML file with application settings
    EXT_NODISCARD std::wstring GetConfigurationXMLFilePath() const;
    // called when there is a change in the list of observed channels
    // bAsynchNotify - a flag to notify about a change in the list of channels asynchronously
    void OnMonitoringChannelsListChanged(bool bAsynchNotify = true);
    // generate every day report
    void GenerateEveryDayReport();

    // work with monitoring tasks
private:
    // Add a monitoring task for the channel
    // @param channelParams - parameters of the channel on which the monitoring task should be launched
    // @param taskType - the type of task being executed
    // @param monitoringInterval - time interval from now until monitoring starts
    // if -1 - use channelParams->monitoringInterval
    void AddMonitoringTaskForChannel(ChannelParameters::Ptr& channelParams,
                                     const MonitoringTaskInfo::TaskType taskType,
                                     CTimeSpan monitoringInterval = -1);
    // Add a monitoring task for the channel list
    void AddMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                      const MonitoringTaskInfo::TaskType taskType,
                                      CTimeSpan monitoringInterval);
    // Add monitoring tasks, each task has a channel
    void AddMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                      const std::list<TaskParameters::Ptr>& taskParams,
                                      const MonitoringTaskInfo::TaskType taskType);

    // Delete jobs to request new monitoring data for the specified channel
    void DelMonitoringTaskForChannel(const ChannelParameters::Ptr& channelParams);

private:
    // map with matching job ID and job parameters
    std::map<TaskId, MonitoringTaskInfo, TaskIdHelper> m_monitoringTasksInfo;

private:
    // monitoring tasks manager
    std::shared_ptr<IMonitoringTasksService> m_monitoringTasksService;
    // bot for working with telegram
    std::shared_ptr<telegram::bot::ITelegramBot> m_telegramBot;

private:
    ext::Scheduler m_everyDayReportScheduler;

    // application data
    ApplicationConfiguration::Ptr m_appConfig;

    // testers
    friend void imitate_data_loaded_and_prepared_for_next_loading(TrendMonitoring* trendMonitoring);
};
