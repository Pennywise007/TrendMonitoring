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
// ���������� ������� ��� ����������� �������
// ����������� ���������� ������� ����������� �������, ����������� ������� ��� �����������
class TrendMonitoring
    : public ITrendMonitoring
    , public EventRecipientImpl
    , public CTickHandlerImpl
{
    friend class CSingleton<TrendMonitoring>;

public:
    TrendMonitoring();

    // �������� ���������� ������
    static std::chrono::minutes getUpdateDataInterval() { return std::chrono::minutes(5); }

// ITrendMonitoring
public:
    /// <summary>�������� ������ ���� ���� �������� �������.</summary>
    [[nodiscard]] std::set<CString> getNamesOfAllChannels() const override;
    /// <summary>�������� ������ ���� ������� �� ������� ���������� ����������.</summary>
    [[nodiscard]] std::list<CString> getNamesOfMonitoringChannels() const override;

    /// <summary>�������� ������ ��� ���� �������.</summary>
    void updateDataForAllChannels() override;

    /// <summary>�������� ���������� ����������� �������.</summary>
    size_t getNumberOfMonitoringChannels() const override;
    /// <summary>�������� ������ ��� ������������ ������ �� �������.</summary>
    /// <param name="channelIndex">������ � ������ �������.</param>
    /// <returns>������ ��� ������.</returns>
    [[nodiscard]] const MonitoringChannelData& getMonitoringChannelData(const size_t channelIndex) const override;

    /// <summary>�������� ����� ��� �����������.</summary>
    /// <returns>������ ������������ ������ � ������.</returns>
    size_t addMonitoringChannel() override;
    /// <summary>������� ����������� ����� �� ������ �� �������.</summary>
    /// <param name="channelIndex">������ ������ � ������ �������.</param>
    /// <returns>������ ��������� ����� ��������.</returns>
    size_t removeMonitoringChannelByIndex(const size_t channelIndex) override;

    /// <summary>�������� ���� ���������� � ������ �� ������.</summary>
    /// <param name="channelIndex">������ ������ � ������ �������.</param>
    /// <param name="newNotifyState">����� ��������� ����������.</param>
    void changeMonitoringChannelNotify(const size_t channelIndex,
                                       const bool newNotifyState) override;
    /// <summary>�������� ��� ������������ ������.</summary>
    /// <param name="channelIndex">������ ������ � ������ �������.</param>
    /// <param name="newChannelName">����� ��� ������.</param>
    void changeMonitoringChannelName(const size_t channelIndex,
                                     const CString& newChannelName) override;
    /// <summary>�������� �������� ����������� ������ ��� ������������ ������.</summary>
    /// <param name="channelIndex">������ ������ � ������ �������.</param>
    /// <param name="newInterval">����� �������� �����������.</param>
    void changeMonitoringChannelInterval(const size_t channelIndex,
                                         const MonitoringInterval newInterval) override;
    /// <summary>�������� �������� �� ���������� �������� ����� ����������� ����������.</summary>
    /// <param name="channelIndex">������ ������ � ������ �������.</param>
    /// <param name="newValue">����� �������� � �����������.</param>
    void changeMonitoringChannelAlarmingValue(const size_t channelIndex,
                                              const float newValue) override;

    /// <summary>����������� ����� �� ������ ����� �� �������.</summary>
    /// <param name="channelIndex">������ ������ � ������ �������.</param>
    /// <returns>����� ������ ������.</returns>
    size_t moveUpMonitoringChannelByIndex(const size_t channelIndex) override;
    /// <summary>����������� ���� �� ������ ����� �� �������.</summary>
    /// <param name="channelIndex">������ ������ � ������ �������.</param>
    /// <returns>����� ������ ������.</returns>
    size_t moveDownMonitoringChannelByIndex(const size_t channelIndex) override;

    // �������� ��������� ���� ���������
    [[nodiscard]] const telegram::bot::TelegramBotSettings& getBotSettings() const override;
    // ���������� ��������� ���� ���������
    void setBotSettings(const telegram::bot::TelegramBotSettings& newSettings) override;

// IEventRecipient
public:
    // ���������� � ������������ �������
    void onEvent(const EventId& code, float eventValue,
                 const std::shared_ptr<IEventData>& eventData) override;

// ITickHandler
public:
    // ���� �������
    enum TimerType
    {
        eUpdatingData = 0,      // ������ ���������� ������� ������
        eEveryDayReporting      // ������ � ���������� �������
    };

    /// <summary>���������� ��� ���� �������.</summary>
    /// <param name="tickParam">�������� ���������� �������.</param>
    /// <returns>true ���� ��� ��, false ���� ���� ���������� ���� ������.</returns>
    bool onTick(TickParam tickParam) override;

// tests
public:
    void installTelegramBot(const std::shared_ptr<telegram::bot::ITelegramBot>& telegramBot);

    [[nodiscard]] ApplicationConfiguration& getConfiguration() const { return *m_appConfig; }

// ������ �� ������� ������� � �����������
private:
    // ���������� ������� �������� ���������
    void saveConfiguration();
    // �������� �������� ���������
    void loadConfiguration();
    // �������� ���� � ����� XML � ����������� ����������
    [[nodiscard]] CString getConfigurationXMLFilePath() const;
    // ���������� ��� ��������� � ������ ����������� �������
    // bAsynchNotify - ���� ��� ���� ���������� �� ��������� � ������ ������� ����������
    void onMonitoringChannelsListChanged(bool bAsynchNotify = true);

// ������ � ��������� �����������
private:
    // �������� ������� ����������� ��� ������
    // @param channelParams - ��������� ������ �� �������� ���� ��������� ������� �����������
    // @param taskType - ��� ������������ �������
    // @param monitoringInterval - �������� ������� � ���������� ������� �� ������ �����������
    // ���� -1 - ������������ channelParams->monitoringInterval
    void addMonitoringTaskForChannel(ChannelParameters::Ptr& channelParams,
                                     const MonitoringTaskInfo::TaskType taskType,
                                     CTimeSpan monitoringInterval = -1);
    // �������� ������� ����������� ��� ������ �������
    void addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                      const MonitoringTaskInfo::TaskType taskType,
                                      CTimeSpan monitoringInterval);
    // �������� ������� �����������, ������� ����� ������������� �����
    void addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                      const std::list<TaskParameters::Ptr>& taskParams,
                                      const MonitoringTaskInfo::TaskType taskType);

    // ������� ������� ������� ����� ������ ����������� ��� ���������� ������
    void delMonitoringTaskForChannel(const ChannelParameters::Ptr& channelParams);

private:
    // ���� � ������������� �������������� ������� � ����������� �������
    std::map<TaskId, MonitoringTaskInfo, TaskComparer> m_monitoringTasksInfo;

private:
    // ��� ��� ������ � ����������
    std::shared_ptr<telegram::bot::ITelegramBot> m_telegramBot;

private:
    // ������ ����������
    ApplicationConfiguration::Ptr m_appConfig = ApplicationConfiguration::create();
};

