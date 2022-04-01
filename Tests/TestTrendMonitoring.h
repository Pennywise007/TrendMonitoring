#pragma once

#include <gtest/gtest.h>

#include <include/ITrendMonitoring.h>
#include "src/TrendMonitoring.h"

#include "MonitoringTaskServiceMock.h"
#include "TelegamBotMock.h"
#include "TestHelper.h"

////////////////////////////////////////////////////////////////////////////////
// класс работы с трендами данных, используется для тестирования мониторинга
class MonitoringTestClass
    : public testing::Test
    // to send us messages immediately without waiting for another thread
    , ext::events::ScopeAsyncSubscription<IMonitoringListEvents, IMonitoringErrorEvents, ILogEvents>
{
protected:
    void SetUp() override
    {
        // сбрасывааем все настройки у сервиса которые уже могли появиться при более раннем использовании другими тестами
        get_service<TestHelper>().resetMonitoringService();

        set_monitoring_tasks_service_mock(m_monitoringServiceMock);

        TrendMonitoring* monitoring = dynamic_cast<TrendMonitoring*>(m_monitoringService);
        ASSERT_TRUE(!!monitoring);
        monitoring->installTelegramBot(m_botMock);

        m_listChanged = false;
        m_errorReport.reset();
        m_logMessage.reset();
    }

    void TearDown() override
    {
        set_monitoring_tasks_service_mock(nullptr);

        TrendMonitoring* monitoring = dynamic_cast<TrendMonitoring*>(m_monitoringService);
        ASSERT_TRUE(!!monitoring);
        monitoring->installTelegramBot(nullptr);
    }

protected:
    void ExpectAddTask(const size_t channelIndex,
                       std::optional<CString> currentChannelName = std::nullopt,
                       std::optional<MonitoringInterval> currentInterval = std::nullopt);
    void ExpectRemoveCurrentTask(const size_t channelIndex);

    template<typename ChangeDataFunction, typename ...Args>
    void ExpectNotificationAboutListChanges(ChangeDataFunction function, Args&&... args)
    {
        m_listChanged = false;
        (m_monitoringService->*function)(std::forward<Args>(args)...);
        EXPECT_TRUE(m_listChanged);
        m_listChanged = false;
    }

    template<typename ChangeDataFunction, typename ...Args>
    size_t ExpectNotificationAboutListChangesWithReturn(ChangeDataFunction function, Args&&... args)
    {
        m_listChanged = false;
        const size_t res = (m_monitoringService->*function)(std::forward<Args>(args)...);
        EXPECT_TRUE(m_listChanged);
        m_listChanged = false;
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
        EXPECT_FALSE(m_listChanged) << "Called twice!";
        m_listChanged = true;
    }

// IMonitoringListEvents
private:
    void OnChanged() override
    {
        EXPECT_FALSE(m_listChanged) << "Called twice!";
        m_listChanged = true;
    }

// ILogEvents
private:
    void OnNewLogMessage(const std::shared_ptr<LogMessageData>& logMessage) override
    {
        EXPECT_FALSE(m_logMessage) << "Called twice!";
        m_logMessage = logMessage;
        EXPECT_TRUE(m_logMessage);
    }

// IMonitoringErrorEvents
private:
    void OnError(const std::shared_ptr<EventData>& errorData) override
    {
        EXPECT_FALSE(m_errorReport) << "Called twice!";
        m_errorReport = errorData;
        EXPECT_TRUE(m_errorReport);
    }

protected:  // Тесты
    // Проверка добавления каналов для мониторинга
    void testAddChannels();
    // Проверка настройки параметров каналов
    void testSetParamsToChannels();
    // Проверка управления списком каналов
    void testChannelListManagement();
    // Проверка удаления каналов из списка
    void testDelChannels();

    // Проверка что внутренний массив со списком каналов совпадает с заданным после выполнения теста
    void checkModelAndRealChannelsData(const std::string& testDescr);

protected:
    // список данных текущих каналов, частично должен совпадать с тем что в сервисе
    std::list<std::pair<TaskId, MonitoringChannelData>> m_channelsData;
    // сервис мониторинга
    ITrendMonitoring* m_monitoringService = GetInterface<ITrendMonitoring>();

    bool m_listChanged = false;
    std::shared_ptr<IMonitoringErrorEvents::EventData> m_errorReport;
    std::shared_ptr<ILogEvents::LogMessageData> m_logMessage;

    std::shared_ptr<MonitoringTaskServiceMock> m_monitoringServiceMock = std::make_shared<MonitoringTaskServiceMock>();
    std::shared_ptr<telegram::bot::TelegramBotMock> m_botMock = std::make_shared<telegram::bot::TelegramBotMock>();
};