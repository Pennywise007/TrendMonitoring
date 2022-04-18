#include "pch.h"

#include <memory>

#include "mocks/TelegamBotMock.h"

#include "TestTrendMonitoring.h"

#include "gmock/gmock.h"

#include "src/TrendMonitoring.h"

using namespace ::testing;

void imitate_data_loaded_and_prepared_for_next_loading(TrendMonitoring* trendMonitoring)
{
    EXPECT_TRUE(trendMonitoring);

    auto& config = *trendMonitoring->m_appConfig.get();
    const auto lastLoadingTime = CTime::GetCurrentTime() - CTimeSpan(0, 0, TrendMonitoring::UpdateDataInterval.count(), 0);
    for (auto& channelParam : config.m_chanelParameters)
    {
        channelParam->m_loadingParametersIntervalEnd = lastLoadingTime;
        channelParam->channelState.dataLoaded = true;
        channelParam->trendData.lastDataExistTime = lastLoadingTime;
    }
}

namespace {

void add_monitoring_result(IMonitoringTaskEvent::ResultsPtrList& monitoringResult, const MonitoringChannelData& channelData,
                           IMonitoringTaskEvent::Result result = IMonitoringTaskEvent::Result::eSucceeded,
                           std::wstring errorText = L"")
{
    const CTime curTime = CTime::GetCurrentTime();
    const CTime taskStartTime = curTime - monitoring_interval_to_timespan(channelData.monitoringInterval);

    TaskParameters::Ptr taskParams = std::make_shared<TaskParameters>(channelData.channelName, taskStartTime, curTime);

    IMonitoringTaskEvent::ResultDataPtr& resultData = monitoringResult.emplace_back(std::make_shared<IMonitoringTaskEvent::ResultData>(taskParams));
    resultData->resultType = result;
    resultData->errorText = std::move(errorText);

    static_cast<TrendChannelData&>(*resultData.get()) = channelData.trendData;
}

void compare_channel_data(const MonitoringChannelData& data1, const MonitoringChannelData& data2)
{
    EXPECT_EQ(data1.channelName, data2.channelName) << data1.channelName << " - " << data2.channelName;
    EXPECT_EQ(data1.monitoringInterval, data2.monitoringInterval) <<
        (int)data1.monitoringInterval << " - " << (int)data2.monitoringInterval;
    EXPECT_EQ(data1.channelState.dataLoaded, data2.channelState.dataLoaded);
    EXPECT_EQ(data1.channelState.loadingDataError, data2.channelState.loadingDataError);

    EXPECT_FLOAT_EQ(data1.trendData.startValue, data2.trendData.startValue);
    EXPECT_FLOAT_EQ(data1.trendData.currentValue, data2.trendData.currentValue);
    EXPECT_FLOAT_EQ(data1.trendData.maxValue, data2.trendData.maxValue);
    EXPECT_FLOAT_EQ(data1.trendData.minValue, data2.trendData.minValue);
}

TaskId prepare_timer_task(ITrendMonitoring* monitoringService, MonitoringTaskServiceMock* monitoringServiceMock,
                          TrendMonitoring::TimerType timerType,
                          const std::list<std::pair<TaskId, MonitoringChannelData>>& channelsData)
{
    TaskId taskId;
    if (!SUCCEEDED(CoCreateGuid(&taskId)))
        EXT_ASSERT(false) << "Не удалось создать гуид!";

    EXPECT_CALL(*monitoringServiceMock, AddTaskList(_, Matcher(IMonitoringTasksService::TaskPriority::eNormal))).
        WillOnce(Invoke([&](const std::list<TaskParameters::Ptr>& listTaskParams,
                            const IMonitoringTasksService::TaskPriority /*priority*/)
    {
        EXPECT_EQ(listTaskParams.size(), channelsData.size());

        auto taskParamIt = listTaskParams.begin();
        for (auto&& [guid, chanInfo] : channelsData)
        {
            const TaskParameters* taskParams = taskParamIt->get();

            EXPECT_EQ(chanInfo.channelName, taskParams->channelName);

            switch (timerType)
            {
            case TrendMonitoring::eUpdatingData:
                break;
            case TrendMonitoring::eEveryDayReporting:
                EXPECT_EQ(taskParams->endTime - taskParams->startTime, CTimeSpan(1, 0, 0, 0));
                break;
            default:
                EXPECT_TRUE(false);
            }

            ++taskParamIt;
        }

        return taskId;
    }));

    ext::tick::ITickHandler* tickHandler = dynamic_cast<ext::tick::ITickHandler*>(monitoringService);
    EXPECT_TRUE(tickHandler);
    tickHandler->OnTick(timerType);

    return taskId;
}

}// namespace

void MonitoringTestClass::ExpectChangesOnCompletingMonitoringTask(const ext::task::TaskId& expectedTaskId,
                                                                  IMonitoringTaskEvent::ResultsPtrList&& monitoringResult,
                                                                  const bool expectListChanged,
                                                                  const bool expectErrorReport,
                                                                  const bool expectLogMessage)
{
    EXPECT_FALSE(m_notifictationEvent.Raised()) << "Called twice!";
    EXPECT_FALSE(m_errorReport) << "Called twice!";
    EXPECT_FALSE(m_logMessage) << "Called twice!";

    ext::send_event(&IMonitoringTaskEvent::OnCompleteTask, expectedTaskId, monitoringResult);

    EXPECT_EQ(m_notifictationEvent.Wait(std::chrono::milliseconds(50)), expectListChanged);
    EXPECT_EQ(m_logMessageEvent.Wait(std::chrono::milliseconds(50)), expectLogMessage);
    EXPECT_EQ(m_errorReportEvent.Wait(std::chrono::milliseconds(50)), expectErrorReport);
}

TEST_F(MonitoringTestClass, ReportIntervalInfo_Success)
{
    testAddChannels();

    auto&& [taskId, channelData] = m_channelsData.front();

    IMonitoringTaskEvent::ResultsPtrList monitoringResult;
    add_monitoring_result(monitoringResult, channelData);

    ExpectChangesOnCompletingMonitoringTask(taskId, std::move(monitoringResult), true, false, false);

    channelData.channelState.dataLoaded = true;
    channelData.channelState.loadingDataError = false;

    const MonitoringChannelData& currentChannelData = m_monitoringService->GetMonitoringChannelData(0);
    compare_channel_data(currentChannelData, channelData);
}

TEST_F(MonitoringTestClass, ReportIntervalInfo_Error)
{
    testAddChannels();

    auto&& [taskId, channelData] = m_channelsData.front();
    channelData.trendData.emptyDataTime = CTimeSpan(1, 0, 0, 0);

    const std::wstring errorText = L"Big error";

    IMonitoringTaskEvent::ResultsPtrList monitoringResult;
    add_monitoring_result(monitoringResult, channelData, IMonitoringTaskEvent::Result::eErrorText, errorText);

    ExpectChangesOnCompletingMonitoringTask(taskId, std::move(monitoringResult), true, true, true);

    channelData.channelState.dataLoaded = false;
    channelData.channelState.loadingDataError = true;

    const MonitoringChannelData& currentChannelData = m_monitoringService->GetMonitoringChannelData(0);

    auto reportTextForAllChannels = std::string_swprintf(L"Канал \"%s\": %s",
                                                         currentChannelData.channelName.c_str(),
                                                         errorText.c_str());

    // We expect settings only emptyDataTime
    compare_channel_data(currentChannelData, channelData);

    ASSERT_TRUE(m_errorReport);
    EXPECT_EQ(m_errorReport->errorTextForAllChannels, reportTextForAllChannels);
    EXPECT_EQ(m_errorReport->problemChannelNames, decltype(m_errorReport->problemChannelNames)({ currentChannelData.channelName }));

    EXPECT_EQ(m_logMessage->messageType, ILogEvents::LogMessageData::MessageType::eError);
    EXPECT_EQ(m_logMessage->logMessage, reportTextForAllChannels);
}

TEST_F(MonitoringTestClass, ReportIntervalInfo_NoData)
{
    testAddChannels();

    auto&& [taskId, channelData] = m_channelsData.front();
    channelData.trendData.emptyDataTime = CTimeSpan(1, 0, 0, 0);

    const CString errorText = L"Нет данных в запрошенном интервале.";
    IMonitoringTaskEvent::ResultsPtrList monitoringResult;
    add_monitoring_result(monitoringResult, channelData, IMonitoringTaskEvent::Result::eNoData);

    ExpectChangesOnCompletingMonitoringTask(taskId, std::move(monitoringResult), true, true, true);

    channelData.channelState.loadingDataError = true;

    const MonitoringChannelData& currentChannelData = m_monitoringService->GetMonitoringChannelData(0);

    auto reportTextForAllChannels = std::string_swprintf(L"Канал \"%s\": %s",
                                                         currentChannelData.channelName.c_str(),
                                                         errorText.GetString());

    // We expect settings only emptyDataTime
    compare_channel_data(currentChannelData, channelData);

    ASSERT_TRUE(m_errorReport);
    EXPECT_STREQ(m_errorReport->errorTextForAllChannels.c_str(), reportTextForAllChannels.c_str());
    EXPECT_EQ(m_errorReport->problemChannelNames, decltype(m_errorReport->problemChannelNames)({ currentChannelData.channelName }));

    EXPECT_EQ(m_logMessage->messageType, ILogEvents::LogMessageData::MessageType::eError);
    EXPECT_STREQ(m_logMessage->logMessage.c_str(), reportTextForAllChannels.c_str());
}

TEST_F(MonitoringTestClass, ReportUpdating_FastTimerTick)
{
    testAddChannels();

    // We should not start load data for recently added channels
    auto tickHandler = std::dynamic_pointer_cast<ext::tick::ITickHandler>(m_monitoringService);
    ASSERT_TRUE(!!tickHandler);
    tickHandler->OnTick(TrendMonitoring::TimerType::eUpdatingData);
}

TEST_F(MonitoringTestClass, ReportUpdating_Success)
{
    testAddChannels();

    imitate_data_loaded_and_prepared_for_next_loading(dynamic_cast<TrendMonitoring*>(m_monitoringService.get()));

    {
        const auto taskId = prepare_timer_task(m_monitoringService.get(), m_monitoringServiceMock.get(),
                                               TrendMonitoring::TimerType::eUpdatingData, m_channelsData);

        IMonitoringTaskEvent::ResultsPtrList monitoringResult;

        float channelIndex = 0;
        for (auto&&[taskId, channelData] : m_channelsData)
        {
            channelData.channelState.dataLoaded = true;
            auto& trendData = channelData.trendData;
            trendData.currentValue = channelIndex * 10.f + 2.f;
            trendData.maxValue = channelIndex * 10.f + 3.f;
            trendData.minValue = -trendData.maxValue;
            trendData.emptyDataTime = CTimeSpan(0, 0, 0, (int)channelIndex++ + 1);
            trendData.lastDataExistTime = CTime::GetCurrentTime();

            add_monitoring_result(monitoringResult, channelData);
        }

        ExpectChangesOnCompletingMonitoringTask(taskId, std::move(monitoringResult), true, false, false);
    }

    size_t channelIndex = 0;
    for (auto&&[taskId, channelData] : m_channelsData)
    {
        compare_channel_data(m_monitoringService->GetMonitoringChannelData(channelIndex++), channelData);
    }
}

TEST_F(MonitoringTestClass, ReportUpdating_Failed)
{
    testAddChannels(1);

    imitate_data_loaded_and_prepared_for_next_loading(dynamic_cast<TrendMonitoring*>(m_monitoringService.get()));

    IMonitoringTaskEvent::ResultsPtrList monitoringResult;
    for (auto&& [taskId, channelData] : m_channelsData)
    {
        auto& trendData = channelData.trendData;
        trendData.emptyDataTime = CTimeSpan(0, 0, TrendMonitoring::UpdateDataInterval.count(), 0);
        trendData.lastDataExistTime = CTime::GetCurrentTime();

        std::wstring alertText;
        channelData.channelState.OnAddChannelErrorReport(ChannelStateManager::eFallenOff, alertText, L"Пропали данные по каналу.");
        channelData.channelState.loadingDataError = true;
        channelData.channelState.dataLoaded = true;

        add_monitoring_result(monitoringResult, channelData, IMonitoringTaskEvent::Result::eNoData, alertText);
    }

    for (int i = 0; i < 6; ++i)
    {
        const auto taskId = prepare_timer_task(m_monitoringService.get(), m_monitoringServiceMock.get(),
                                               TrendMonitoring::TimerType::eUpdatingData, m_channelsData);


        bool expectError = i == 3;
        ExpectChangesOnCompletingMonitoringTask(taskId, std::move(monitoringResult), true, expectError, i == 0 || expectError);

        m_notifictationEvent.Reset();
        m_logMessageEvent.Reset();
        m_errorReportEvent.Reset();
        m_errorReport.reset();
        m_logMessage.reset();
    }

    {
        EXPECT_EQ(m_channelsData.size(), m_monitoringService->GetNumberOfMonitoringChannels());
        size_t channelIndex = 0;
        for (auto&&[taskId, channelData] : m_channelsData)
        {
            compare_channel_data(m_monitoringService->GetMonitoringChannelData(channelIndex++), channelData);
        }
    }
}