#include "pch.h"

#include <memory>

#include "TelegamBotMock.h"
#include "TestTrendMonitoring.h"

#include "gmock/gmock.h"

#include "src/TrendMonitoring.h"

using namespace ::testing;

namespace {

void imitate_data_loaded_and_prepared_for_next_loading(TrendMonitoring* trendMonitoring)
{
    EXPECT_TRUE(trendMonitoring);

    auto& config = trendMonitoring->getConfiguration();
    const auto lastLoadingTime = CTime::GetCurrentTime() - CTimeSpan(0, 0, TrendMonitoring::getUpdateDataInterval().count(), 0);
    for (auto& channelParam : config.m_chanelParameters)
    {
        channelParam->m_loadingParametersIntervalEnd = lastLoadingTime;
        channelParam->channelState.dataLoaded = true;
    }
}

void add_monitoring_result(MonitoringResult::Ptr& monitoringResult, const MonitoringChannelData& channelData,
                           MonitoringResult::Result result = MonitoringResult::Result::eSucceeded,
                           CString errorText = L"")
{
    ASSERT_TRUE(monitoringResult);

    const CTime curTime = CTime::GetCurrentTime();
    const CTime taskStartTime = curTime - monitoring_interval_to_timespan(channelData.monitoringInterval);

    TaskParameters::Ptr taskParams = std::make_shared<TaskParameters>(channelData.channelName, taskStartTime, curTime);

    MonitoringResult::ResultData& resultData = monitoringResult->m_taskResults.emplace_back(taskParams);
    resultData.resultType = result;
    resultData.errorText = std::move(errorText);

    static_cast<TrendChannelData&>(resultData) = channelData.trendData;
}

void compare_channel_data(const MonitoringChannelData& data1, const MonitoringChannelData& data2)
{
    EXPECT_EQ(data1.channelName, data2.channelName) << data1.channelName << " - " << data2.channelName;
    EXPECT_EQ(data1.monitoringInterval, data2.monitoringInterval) <<
        (int)data1.monitoringInterval << " - " << (int)data2.monitoringInterval;
    EXPECT_EQ(data1.channelState.dataLoaded, data2.channelState.dataLoaded);
    EXPECT_EQ(data1.channelState.loadingDataError, data2.channelState.loadingDataError);
    EXPECT_EQ(data1.channelState, data2.channelState);

    EXPECT_FLOAT_EQ(data1.trendData.startValue, data2.trendData.startValue);
    EXPECT_FLOAT_EQ(data1.trendData.currentValue, data2.trendData.currentValue);
    EXPECT_FLOAT_EQ(data1.trendData.maxValue, data2.trendData.maxValue);
    EXPECT_FLOAT_EQ(data1.trendData.minValue, data2.trendData.minValue);
    EXPECT_EQ(data1.trendData.emptyDataTime, data2.trendData.emptyDataTime);
    EXPECT_EQ(data1.trendData.lastDataExistTime, data2.trendData.lastDataExistTime);
}

TaskId prepare_timer_task(ITrendMonitoring* monitoringService, MonitoringTaskServiceMock* monitoringServiceMock,
                          TrendMonitoring::TimerType timerType,
                          const std::list<std::pair<TaskId, MonitoringChannelData>>& channelsData)
{
    TaskId taskId;
    if (!SUCCEEDED(CoCreateGuid(&taskId)))
        assert(!"�� ������� ������� ����!");

    EXPECT_CALL(*monitoringServiceMock, addTaskList(_, Matcher(IMonitoringTasksService::eNormal))).
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

    ITickHandler* tickHandler = dynamic_cast<ITickHandler*>(monitoringService);
    EXPECT_TRUE(tickHandler);
    EXPECT_TRUE(tickHandler->onTick(timerType));

    return taskId;
}

}// namespace

void MonitoringTestClass::ExpectChangesOnCompletingMonitoringTask(const MonitoringResult::Ptr& monitoringResult,
                                                                  const bool expectListChanged,
                                                                  const bool expectErrorReport,
                                                                  const bool expectLogMessage)
{
    EXPECT_FALSE(m_listChanged) << "Called twice!";
    EXPECT_FALSE(m_errorReport) << "Called twice!";
    EXPECT_FALSE(m_logMessage) << "Called twice!";

    get_service<CMassages>().sendMessage(onCompletingMonitoringTask, 0, std::static_pointer_cast<IEventData>(monitoringResult));

    EXPECT_EQ(m_listChanged, expectListChanged) << "Called twice!";
    EXPECT_EQ(!!m_errorReport, expectErrorReport) << "Called twice!";
    EXPECT_EQ(!!m_logMessage, expectLogMessage) << "Called twice!";
}

TEST_F(MonitoringTestClass, ReportIntervalInfo_Success)
{
    testAddChannels();

    auto&& [taskId, channelData] = m_channelsData.front();

    MonitoringResult::Ptr monitoringResult = std::make_shared<MonitoringResult>(taskId);
    add_monitoring_result(monitoringResult, channelData);

    ExpectChangesOnCompletingMonitoringTask(monitoringResult, true, false, false);

    channelData.channelState.dataLoaded = true;
    channelData.channelState.loadingDataError = false;

    const MonitoringChannelData& currentChannelData = m_monitoringService->getMonitoringChannelData(0);
    compare_channel_data(currentChannelData, channelData);
}

TEST_F(MonitoringTestClass, ReportIntervalInfo_Error)
{
    testAddChannels();

    auto&& [taskId, channelData] = m_channelsData.front();
    channelData.trendData.emptyDataTime = CTimeSpan(1, 0, 0, 0);

    const CString errorText = "Big error";
    MonitoringResult::Ptr monitoringResult = std::make_shared<MonitoringResult>(taskId);
    add_monitoring_result(monitoringResult, channelData, MonitoringResult::Result::eErrorText, errorText);

    ExpectChangesOnCompletingMonitoringTask(monitoringResult, true, true, true);

    channelData.channelState.dataLoaded = false;
    channelData.channelState.loadingDataError = true;

    const MonitoringChannelData& currentChannelData = m_monitoringService->getMonitoringChannelData(0);

    CString reportTextForAllChannels;
    reportTextForAllChannels.AppendFormat(L"����� \"%s\": %s",
                                          currentChannelData.channelName.GetString(),
                                          errorText.GetString());

    // We expect settings only emptyDataTime
    compare_channel_data(currentChannelData, channelData);

    EXPECT_EQ(m_errorReport->errorTextForAllChannels, reportTextForAllChannels);
    EXPECT_EQ(m_errorReport->problemChannelNames, decltype(m_errorReport->problemChannelNames)({ currentChannelData.channelName }));

    EXPECT_EQ(m_logMessage->messageType, LogMessageData::MessageType::eError);
    EXPECT_EQ(m_logMessage->logMessage, reportTextForAllChannels);
}

TEST_F(MonitoringTestClass, ReportIntervalInfo_NoData)
{
    testAddChannels();

    auto&& [taskId, channelData] = m_channelsData.front();
    channelData.trendData.emptyDataTime = CTimeSpan(1, 0, 0, 0);

    const CString errorText = L"��� ������ � ����������� ���������.";
    MonitoringResult::Ptr monitoringResult = std::make_shared<MonitoringResult>(taskId);
    add_monitoring_result(monitoringResult, channelData, MonitoringResult::Result::eNoData);

    ExpectChangesOnCompletingMonitoringTask(monitoringResult, true, true, true);

    channelData.channelState.loadingDataError = true;

    const MonitoringChannelData& currentChannelData = m_monitoringService->getMonitoringChannelData(0);

    CString reportTextForAllChannels;
    reportTextForAllChannels.AppendFormat(L"����� \"%s\": %s",
                                          currentChannelData.channelName.GetString(),
                                          errorText.GetString());

    // We expect settings only emptyDataTime
    compare_channel_data(currentChannelData, channelData);

    EXPECT_EQ(m_errorReport->errorTextForAllChannels, reportTextForAllChannels);
    EXPECT_EQ(m_errorReport->problemChannelNames, decltype(m_errorReport->problemChannelNames)({ currentChannelData.channelName }));

    EXPECT_EQ(m_logMessage->messageType, LogMessageData::MessageType::eError);
    EXPECT_EQ(m_logMessage->logMessage, reportTextForAllChannels);
}

TEST_F(MonitoringTestClass, ReportUpdating_FastTimerTick)
{
    testAddChannels();

    // We should not start load data for recently added channels
    auto* tickHandler = dynamic_cast<ITickHandler*>(m_monitoringService);
    EXPECT_TRUE(tickHandler);
    EXPECT_TRUE(tickHandler->onTick(TrendMonitoring::TimerType::eUpdatingData));
}

TEST_F(MonitoringTestClass, ReportUpdating_Success)
{
    testAddChannels();

    imitate_data_loaded_and_prepared_for_next_loading(dynamic_cast<TrendMonitoring*>(m_monitoringService));

    {
        const auto taskId = prepare_timer_task(m_monitoringService, m_monitoringServiceMock.get(),
                                               TrendMonitoring::TimerType::eUpdatingData, m_channelsData);

        MonitoringResult::Ptr monitoringResult = std::make_shared<MonitoringResult>(taskId);

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

        ExpectChangesOnCompletingMonitoringTask(monitoringResult, true, false, false);
    }

    size_t channelIndex = 0;
    for (auto&&[taskId, channelData] : m_channelsData)
    {
        compare_channel_data(m_monitoringService->getMonitoringChannelData(channelIndex++), channelData);
    }
}

TEST_F(MonitoringTestClass, DISABLED_ReportUpdating_Failed)
{
    // TODO
    testAddChannels();

    imitate_data_loaded_and_prepared_for_next_loading(dynamic_cast<TrendMonitoring*>(m_monitoringService));


    {
        const auto taskId = prepare_timer_task(m_monitoringService, m_monitoringServiceMock.get(),
                                               TrendMonitoring::TimerType::eUpdatingData, m_channelsData);

        MonitoringResult::Ptr monitoringResult = std::make_shared<MonitoringResult>(taskId);
        for (auto&&[taskId, channelData] : m_channelsData)
        {
            auto& trendData = channelData.trendData;
            trendData.emptyDataTime = CTimeSpan(0, 0, TrendMonitoring::getUpdateDataInterval().count(), 0);
            trendData.lastDataExistTime = CTime::GetCurrentTime();

            add_monitoring_result(monitoringResult, channelData);
        }

        ExpectChangesOnCompletingMonitoringTask(monitoringResult, true, false, false);
    }

    {
        EXPECT_EQ(m_channelsData.size(), m_monitoringService->getNumberOfMonitoringChannels());
        size_t channelIndex = 0;
        for (auto&&[taskId, channelData] : m_channelsData)
        {
            compare_channel_data(m_monitoringService->getMonitoringChannelData(channelIndex++), channelData);
        }
    }
}