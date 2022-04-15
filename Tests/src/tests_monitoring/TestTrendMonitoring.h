#pragma once

#include <gtest/gtest.h>

#include <include/ITrendMonitoring.h>
#include <ext/thread/event.h>

#include "src/TrendMonitoring.h"

#include "mocks/MonitoringTaskServiceMock.h"
#include "mocks/TelegamBotMock.h"
#include "helpers/TestHelper.h"

////////////////////////////////////////////////////////////////////////////////
// класс работы с трендами данных, используетс¤ дл¤ тестировани¤ мониторинга
class MonitoringTestClass
    : public testing::Test
    // to send us messages immediately without waiting for another thread
    , public ext::events::ScopeSubscription<IMonitoringListEvents, IMonitoringErrorEvents, ILogEvents>
{
public:
    // Список зашитых в данных каналов мониторинга
    inline static size_t kMonitoringChannelsCount = 3;

protected:
    void SetUp() override
    {
        auto& serviceCollection = ext::get_service<ext::ServiceCollection>();
        serviceCollection.RegisterScoped<MonitoringTaskServiceMock, IMonitoringTasksService, MonitoringTaskServiceMock>();

        auto serviceProvider = serviceCollection.BuildServiceProvider();

        m_monitoringService = ext::GetInterface<ITrendMonitoring>(serviceProvider);
        m_monitoringServiceMock = ext::GetInterface<MonitoringTaskServiceMock>(serviceProvider);

        m_notifictationEvent.Create();
        m_logMessageEvent.Create();
        m_errorReportEvent.Create();

        m_errorReport.reset();
        m_logMessage.reset();
    }

    void TearDown() override
    {
        ext::get_service<ext::ServiceCollection>().UnregisterObject<MonitoringTaskServiceMock, IMonitoringTasksService, MonitoringTaskServiceMock>();
        ext::get_service<TestHelper>().ResetAll();

        m_notifictationEvent.Destroy();
        m_logMessageEvent.Destroy();
        m_errorReportEvent.Destroy();

        m_monitoringService = nullptr;
        m_monitoringServiceMock = nullptr;
    }

protected:
    void ExpectAddTask(const size_t channelIndex,
                       std::optional<std::wstring> currentChannelName = std::nullopt,
                       std::optional<MonitoringInterval> currentInterval = std::nullopt);
    void ExpectRemoveCurrentTask(const size_t channelIndex);

    template<typename ChangeDataFunction, typename ...Args>
    void ExpectNotificationAboutListChanges(ChangeDataFunction function, Args&&... args)
    {
        m_notifictationEvent.Reset();
        ((*m_monitoringService).*function)(std::forward<Args>(args)...);
        EXPECT_TRUE(m_notifictationEvent.Wait(std::chrono::milliseconds(100)));
    }

    template<typename ChangeDataFunction, typename ...Args>
    size_t ExpectNotificationAboutListChangesWithReturn(ChangeDataFunction function, Args&&... args)
    {
        m_notifictationEvent.Reset();
        const size_t res = ((*m_monitoringService).*function)(std::forward<Args>(args)...);
        EXPECT_TRUE(m_notifictationEvent.Wait(std::chrono::milliseconds(100)));
        return res;
    }

    void ExpectChangesOnCompletingMonitoringTask(const ext::task::TaskId& expectedTaskId,
                                                 IMonitoringTaskEvent::ResultsPtrList&& monitoringResult,
                                                 const bool expectListChanged,
                                                 const bool expectErrorReport,
                                                 const bool expectLogMessage);

// IMonitoringListEvents
private:
    void OnChanged() override
    {
        EXPECT_FALSE(m_notifictationEvent.Raised()) << "Called twice!";
        m_notifictationEvent.Set();
    }

// ILogEvents
private:
    void OnNewLogMessage(const std::shared_ptr<LogMessageData>& logMessage) override
    {
        EXPECT_FALSE(m_logMessage) << "Called twice!";
        EXPECT_FALSE(m_logMessageEvent.Raised()) << "Called twice!";

        m_logMessage = logMessage;
        EXPECT_TRUE(m_logMessage);
        m_logMessageEvent.Set();
    }

// IMonitoringErrorEvents
private:
    void OnError(const std::shared_ptr<EventData>& errorData) override
    {
        EXPECT_FALSE(m_errorReport) << "Called twice!";
        EXPECT_FALSE(m_errorReportEvent.Raised()) << "Called twice!";
        m_errorReport = errorData;
        EXPECT_TRUE(m_errorReport);
        m_errorReportEvent.Set();
    }

protected:  // “есты
    // ѕроверка добавлени¤ каналов дл¤ мониторинга
    void testAddChannels(size_t count = kMonitoringChannelsCount);
    // ѕроверка настройки параметров каналов
    void testSetParamsToChannels();
    // ѕроверка управлени¤ списком каналов
    void testChannelListManagement();
    // ѕроверка удалени¤ каналов из списка
    void testDelChannels();

    // ѕроверка что внутренний массив со списком каналов совпадает с заданным после выполнени¤ теста
    void checkModelAndRealChannelsData(const std::wstring& testDescr);

protected:
    // список данных текущих каналов, частично должен совпадать с тем что в сервисе
    std::list<std::pair<TaskId, MonitoringChannelData>> m_channelsData;
    // сервис мониторинга
    std::shared_ptr<ITrendMonitoring> m_monitoringService;

    std::shared_ptr<IMonitoringErrorEvents::EventData> m_errorReport;
    std::shared_ptr<ILogEvents::LogMessageData> m_logMessage;

    std::shared_ptr<MonitoringTaskServiceMock> m_monitoringServiceMock;

    ext::Event m_notifictationEvent;
    ext::Event m_logMessageEvent;
    ext::Event m_errorReportEvent;
};