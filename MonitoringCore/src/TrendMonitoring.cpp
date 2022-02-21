#include "pch.h"

#include <ctime>

#include <myInclude/ChannelDataCorrector/ChannelDataGetter.h>

#include <DirsService.h>

#include "Serialization/SerializatorFabric.h"
#include "TrendMonitoring.h"
#include "Telegram/TelegramBot.h"
#include "Utils.h"

// ����� � ������� ����� ���������� ����� ������ ���� ���� + ������ (20:00)
const std::pair<int, int> kReportDataTime = std::make_pair(20, 00);

////////////////////////////////////////////////////////////////////////////////
ITrendMonitoring* get_monitoring_service()
{
    return &get_service<TrendMonitoring>();
}

////////////////////////////////////////////////////////////////////////////////
// ���������� ������� ��� ����������� �������
TrendMonitoring::TrendMonitoring()
{
    // ������������� �� ������� � ���������� �����������
    EventRecipientImpl::subscribe(onCompletingMonitoringTask);
    // ������������� �� ������� ��������� � ������ ������������� ����������
    EventRecipientImpl::subscribe(telegram::users::onUsersListChangedEvent);

    // ��������� ������������ �� �����
    loadConfiguration();

    // �������������� ���� ����� �������� ������������
    m_telegramBot = std::make_unique<telegram::bot::CTelegramBot>(m_appConfig->getTelegramUsers());
    // �������� ���� ��������� �� ��������
    m_telegramBot->setBotSettings(getBotSettings());

    // �������� ������� ��� �����������, ������ ���������� ��������� ��� �����
    // ����� ��������� ������ ��� ����������
    for (auto& channel : m_appConfig->m_chanelParameters)
        addMonitoringTaskForChannel(channel, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // ���������� ������ ���������� ������
    CTickHandlerImpl::subscribeTimer(getUpdateDataInterval(), TimerType::eUpdatingData);

    // ���������� ������ ������
    {
        // ������������ ����� ��������� ������
        time_t nextReportTime_t = time(NULL);

        tm nextReportTm;
        localtime_s(&nextReportTm, &nextReportTime_t);

        // ��������� ��� � ���� ���� ��� �� ������� ����� ��� ������
        bool needReportToday = false;
        if (nextReportTm.tm_hour < kReportDataTime.first ||
            (nextReportTm.tm_hour == kReportDataTime.first &&
             nextReportTm.tm_min < kReportDataTime.second))
            needReportToday = true;

        // ������������ ����� ���� ������
        nextReportTm.tm_hour = kReportDataTime.first;
        nextReportTm.tm_min  = kReportDataTime.second;
        nextReportTm.tm_sec  = 0;

        // ���� �� ������� - ������ ���� ����� ����� ��� ������
        if (!needReportToday)
            ++nextReportTm.tm_mday;

        // ������������ � std::chrono
        std::chrono::system_clock::time_point nextReportTime =
            std::chrono::system_clock::from_time_t(mktime(&nextReportTm));

        // ���������� ������ � ���������� �� ���� ������
        CTickHandlerImpl::subscribeTimer(nextReportTime - std::chrono::system_clock::now(),
                                         TimerType::eEveryDayReporting);
    }
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
std::list<CString> TrendMonitoring::getNamesOfAllChannels() const
{
    OUTPUT_LOG_FUNC_ENTER;

    // ��������� ������������� ������ �������
    std::list<CString> allChannelsNames;
    // ���� ������ � ���������� � ������� ��������� ��� ��� ������ ����������� � ����� ��� �������
    ChannelDataGetter::FillChannelList(allChannelsNames, false);

    return allChannelsNames;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
std::list<CString> TrendMonitoring::getNamesOfMonitoringChannels() const
{
    // ��������� ������������� ������ �������
    std::list<CString> allChannelsNames;
    for (const auto& channel :  m_appConfig->m_chanelParameters)
        allChannelsNames.emplace_back(channel->channelName);

    return allChannelsNames;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::updateDataForAllChannels()
{
    // ������� ������ ��� ���� ������� � ��������� ������� �� ����������
    for (auto& channel : m_appConfig->m_chanelParameters)
    {
        // ������� ������� ����������� ��� ������
        delMonitoringTaskForChannel(channel);
        // ������� ������
        channel->resetChannelData();
        // ��������� ����� �������, ������ �� ������ ����� ���� �������� ����������
        addMonitoringTaskForChannel(channel, MonitoringTaskInfo::TaskType::eIntervalInfo);
    }

    // �������� �� ��������� � ������ �������
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::getNumberOfMonitoringChannels() const
{
    return m_appConfig->m_chanelParameters.size();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
const MonitoringChannelData& TrendMonitoring::getMonitoringChannelData(const size_t channelIndex)  const
{
    assert(channelIndex < m_appConfig->m_chanelParameters.size() && "���������� ������� ������ ��� ������ ������");
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
        ::MessageBox(NULL, L"������ ��� ����������� �� �������", L"���������� �������� ����� ��� �����������", MB_OK | MB_ICONERROR);
        return 0;
    }

    m_appConfig->m_chanelParameters.push_back(ChannelParameters::make(*channelsList.begin()));

    // �������� ������� ������
    addMonitoringTaskForChannel(m_appConfig->m_chanelParameters.back(),
                                MonitoringTaskInfo::TaskType::eIntervalInfo);

    // �������� �� ��������� � ������ ������� ���������, ����� � ������ ������� ����� ��������� �����
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
        assert(!"���������� ������� ������ ��� ������ ���������� ������");
        return channelIndex;
    }

    // �������� �������� �� �����
    ChannelIt channelIt = std::next(channelsList.begin(), channelIndex);

    // ���������� ������� ����������� ��� ������
    delMonitoringTaskForChannel(*channelIt);

    // ������� �� ����� �������
    channelIt = channelsList.erase(channelIt);
    if (channelIt == channelsList.end() && !channelsList.empty())
        --channelIt;

    size_t result = std::distance(channelsList.begin(), channelIt);

    // �������� �� ��������� � ������ �������
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
        ::MessageBox(NULL, L"������ ��� � ������", L"���������� �������� ����������� ������", MB_OK | MB_ICONERROR);
        return;
    }

    // �������� ��������� ������ �� �������� ������ ���
    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(),
                                                      channelIndex);
    if (!channelParams->changeNotification(newNotifyState))
        return;

    // �������� �� ��������� � ������ �������
    onMonitoringChannelsListChanged(true);
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoringChannelName(const size_t channelIndex,
                                                  const CString& newChannelName)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"������ ��� � ������", L"���������� �������� ��� ������", MB_OK | MB_ICONERROR);
        return;
    }

    // �������� ��������� ������ �� �������� ������ ���
    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->changeName(newChannelName))
        return;

    // ���� ��� ���������� ������� ��������� ��������� ������� �� ������
    delMonitoringTaskForChannel(channelParams);
    // ��������� ����� ������� ��� �����������
    addMonitoringTaskForChannel(channelParams, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // �������� �� ��������� � ������ �������
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoringChannelInterval(const size_t channelIndex,
                                                     const MonitoringInterval newInterval)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"������ ��� � ������", L"���������� �������� �������� ����������", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->changeInterval(newInterval))
        return;

    // ���� ��� ���������� ������� ��������� ��������� ������� �� ������
    delMonitoringTaskForChannel(channelParams);
    // ��������� ����� ������� ��� �����������
    addMonitoringTaskForChannel(channelParams, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // �������� �� ��������� � ������ �������
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoringChannelAlarmingValue(const size_t channelIndex,
                                                           const float newValue)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"������ ��� � ������", L"���������� �������� �������� ����������", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->changeAlarmingValue(newValue))
        return;

    // �������� �� ��������� � ������ �������
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

    // ������ ������� ������ ����� �����������
    size_t resultIndex;

    if (movingIt == channelsList.begin())
    {
        // ������� � �����
        channelsList.splice(channelsList.end(), channelsList, movingIt);
        resultIndex = channelsList.size() - 1;
    }
    else
    {
        // ������ � ���������� �������
        std::iter_swap(movingIt, std::prev(movingIt));
        resultIndex = channelIndex - 1;
    }

    // �������� �� ��������� � ������ �������
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

    // ������ ������� ������ ����� �����������
    size_t resultIndex;

    if (movingIt == --channelsList.end())
    {
        // ������� � ������
        channelsList.splice(channelsList.begin(), channelsList, movingIt);
        resultIndex = 0;
    }
    else
    {
        // ������ �� ��������� �������
        std::iter_swap(movingIt, std::next(movingIt));
        resultIndex = channelIndex + 1;
    }

    // �������� �� ��������� � ������ �������
    onMonitoringChannelsListChanged();

    return resultIndex;
}

//----------------------------------------------------------------------------//
const telegram::bot::TelegramBotSettings& TrendMonitoring::getBotSettings() const
{
    return m_appConfig->getTelegramSettings();
}

//----------------------------------------------------------------------------//
void TrendMonitoring::setBotSettings(const telegram::bot::TelegramBotSettings& newSettings)
{
    // ��������� ���������
    m_appConfig->setTelegramSettings(newSettings);

    // ��������� ���������
    saveConfiguration();

    // �������������� ���� ����� �������
    m_telegramBot->setBotSettings(getBotSettings());
}

//----------------------------------------------------------------------------//
// IEventRecipient
void TrendMonitoring::onEvent(const EventId& code, float /*eventValue*/,
                              const std::shared_ptr<IEventData>& eventData)
{
    if (code == onCompletingMonitoringTask)
    {
        MonitoringResult::Ptr monitoringResult = std::static_pointer_cast<MonitoringResult>(eventData);

        auto it = m_monitoringTasksInfo.find(monitoringResult->m_taskId);
        if (it == m_monitoringTasksInfo.end())
            // ��������� �� ���� �������
            return;

        assert(!monitoringResult->m_taskResults.empty() && !it->second.channelParameters.empty());

        // ��������� �� ���������� �������
        ChannelIt channelIt  = it->second.channelParameters.begin();
        const ChannelIt channelEnd = it->second.channelParameters.end();

        // ��������� �� ����������� �������
        MonitoringResult::ResultIt resultIt  = monitoringResult->m_taskResults.begin();
        const MonitoringResult::ResultIt resultEnd = monitoringResult->m_taskResults.end();

        // ���� ��� � ������ ����������� ���� ���������
        bool bMonitoringListChanged = false;

        std::vector<CString> listOfProblemChannels;
        listOfProblemChannels.reserve(monitoringResult->m_taskResults.size());

        CString reportTextForAllChannels;
        // ��� ������� ������ ����������� ��� ��������� �������
        for (; channelIt != channelEnd && resultIt != resultEnd; ++channelIt, ++resultIt)
        {
            assert((*channelIt)->channelName == resultIt->pTaskParameters->channelName &&
                   "�������� ���������� ��� ������� ������!");

            CString channelError;
            bMonitoringListChanged |= MonitoringTaskResultHandler::HandleIntervalInfoResult(it->second.taskType, *resultIt,
                                                                                            *channelIt, channelError);

            // ���� �������� ������ ��� ��������� ������ � ����� � ��� ���������
            if (!channelError.IsEmpty() && (*channelIt)->bNotify)
            {
                reportTextForAllChannels.AppendFormat(L"����� \"%s\": %s\n",
                                                      (*channelIt)->channelName.GetString(),
                                                      channelError.GetString());

                listOfProblemChannels.emplace_back((*channelIt)->channelName);
            }
        }

        reportTextForAllChannels = reportTextForAllChannels.Trim();

        // ���� �������� ������ ������������ �� �� ������� ��� ������� ���� �������
        if (it->second.taskType == MonitoringTaskInfo::TaskType::eEveryDayReport)
        {
            // ���� �� � ��� �������� ������� ��� ��� ��
            if (reportTextForAllChannels.IsEmpty())
                reportTextForAllChannels = L"������ � �������.";

            const CString reportDelimer(L'*', 25);

            // ������� ��������� �� ������
            auto reportMessage = std::make_shared<MessageTextData>();
            reportMessage->messageText.Format(L"%s\n\n���������� ����� �� %s\n\n%s\n%s",
                                              reportDelimer.GetString(),
                                              CTime::GetCurrentTime().Format(L"%d.%m.%Y").GetString(),
                                              reportTextForAllChannels.GetString(),
                                              reportDelimer.GetString());

            // ��������� � ������� ������
            get_service<CMassages>().postMessage(onReportPreparedEvent, 0,
                                                 std::static_pointer_cast<IEventData>(reportMessage));

            // �������� ������������� ���������
            m_telegramBot->sendMessageToAdmins(reportMessage->messageText);
        }
        else if (!reportTextForAllChannels.IsEmpty())
        {
            assert(it->second.taskType == MonitoringTaskInfo::TaskType::eIntervalInfo ||
                   it->second.taskType == MonitoringTaskInfo::TaskType::eUpdatingInfo);

            // �������� � ��� ��� �������� ��������
            send_message_to_log(LogMessageData::MessageType::eError, reportTextForAllChannels.GetString());

            // ��������� � ��������� ������
            auto errorMessage = std::make_shared<MonitoringErrorEventData>();
            errorMessage->errorTextForAllChannels = std::move(reportTextForAllChannels);
            errorMessage->problemChannelNames = std::move(listOfProblemChannels);

            // ������� ������������� ������
            if (!SUCCEEDED(CoCreateGuid(&errorMessage->errorGUID)))
                assert(!"�� ������� ������� ����!");
            get_service<CMassages>().postMessage(onMonitoringErrorEvent, 0,
                                                 std::static_pointer_cast<IEventData>(errorMessage));
        }

        if (bMonitoringListChanged)
            // ��������� �� ��������� � ������ ��� �����������
            get_service<CMassages>().postMessage(onMonitoringListChanged);

        // ������� ������� �� ������
        m_monitoringTasksInfo.erase(it);
    }
    else if (code == telegram::users::onUsersListChangedEvent)
    {
        // ��������� ��������� � ������
        saveConfiguration();
    }
    else
        assert(!"����������� �������");
}

//----------------------------------------------------------------------------//
bool TrendMonitoring::onTick(TickParam tickParam)
{
    switch (TimerType(tickParam))
    {
    case TimerType::eUpdatingData:
        {
            const CTime currentTime = CTime::GetCurrentTime();

            // �.�. ������ ����� ����������� � ������ ����� ����� ��� ������� ������ ������
            // ���� ������� � ������������ ����������, ��������� ������ ���������� �������
            std::list<TaskParameters::Ptr> listTaskParams;
            // ������ ����������� �������
            ChannelParametersList updatingDataChannels;

            // �������� �� ���� ������� � ������� �� ����� ���� �������� ������
            for (auto& channelParameters : m_appConfig->m_chanelParameters)
            {
                // ������ ��� �� ���������
                if (!channelParameters->channelState.dataLoaded &&
                    !channelParameters->channelState.loadingDataError)
                {
                    // ��������� ��� ����� �� ���
                    if (channelParameters->m_loadingParametersIntervalEnd.has_value() &&
                        (currentTime - *channelParameters->m_loadingParametersIntervalEnd).GetTotalMinutes() > 10)
                        send_message_to_log(LogMessageData::MessageType::eError,
                                            L"������ �� ������ %s �������� ������ 10 �����",
                                            channelParameters->channelName.GetString());

                    continue;
                }

                // ��� ������ ���� ������� ������ � ���������
                assert(channelParameters->m_loadingParametersIntervalEnd.has_value());

                // ���� � ������� �������� ������ �� ���������� �������(������ ��������� �� ��������)
                if (!channelParameters->m_loadingParametersIntervalEnd.has_value() ||
                    (currentTime - *channelParameters->m_loadingParametersIntervalEnd).GetTotalMinutes() <
                    getUpdateDataInterval().count() - 1)
                    continue;

                // ��������� ����� � ������ �����������
                updatingDataChannels.emplace_back(channelParameters);
                listTaskParams.emplace_back(new TaskParameters(channelParameters->channelName,
                                                               *channelParameters->m_loadingParametersIntervalEnd,
                                                               currentTime));
            }

            if (!listTaskParams.empty())
                addMonitoringTaskForChannels(updatingDataChannels, listTaskParams,
                                             MonitoringTaskInfo::TaskType::eUpdatingInfo);
        }
        break;
    case TimerType::eEveryDayReporting:
        {
            // ���������� ������ � ���������� �� ���� ������
            CTickHandlerImpl::subscribeTimer(std::chrono::hours(24), TimerType::eEveryDayReporting);

            if (!m_appConfig->m_chanelParameters.empty())
            {
                // ����� ������� ������� �� ������� ��������� ����������
                ChannelParametersList channelsCopy;
                for (const auto& currentChannel : m_appConfig->m_chanelParameters)
                {
                    channelsCopy.push_back(ChannelParameters::make(currentChannel->channelName));
                    channelsCopy.back()->alarmingValue = currentChannel->alarmingValue;
                }

                // ��������� ������� ������������ ������ �� ��������� ����
                addMonitoringTaskForChannels(channelsCopy,
                                             MonitoringTaskInfo::TaskType::eEveryDayReport,
                                             CTimeSpan(1, 0, 0, 0));
            }
        }
        // ����������� ���� ��� ������ �.�. �� ��������� �����
        return false;
    default:
        assert(!"����������� ������!");
        break;
    }

    return true;
}

//----------------------------------------------------------------------------//
void TrendMonitoring::installTelegramBot(const std::shared_ptr<telegram::bot::ITelegramBot>& telegramBot)
{
    if (telegramBot)
        m_telegramBot = telegramBot;
    else
        m_telegramBot = std::make_unique<telegram::bot::CTelegramBot>(m_appConfig->getTelegramUsers());;
}

//----------------------------------------------------------------------------//
void TrendMonitoring::saveConfiguration()
{
    SerializationExecutor::serializeObject(SerializationFabric::createXMLSerializator(getConfigurationXMLFilePath()),
                                           m_appConfig);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::loadConfiguration()
{
    SerializationExecutor::deserializeObject(SerializationFabric::createXMLDeserializator(getConfigurationXMLFilePath()),
                                             m_appConfig);
}

//----------------------------------------------------------------------------//
CString TrendMonitoring::getConfigurationXMLFilePath() const
{
    return get_service<DirsService>().getExeDir() + kConfigFileName;
}

//----------------------------------------------------------------------------//
void TrendMonitoring::onMonitoringChannelsListChanged(bool bAsynchNotify /*= true*/)
{
    OUTPUT_LOG_FUNC_ENTER;

    // ��������� ����� ������ �����������
    saveConfiguration();

    // ��������� �� ��������� � ������ ��� �����������
    if (bAsynchNotify)
        get_service<CMassages>().postMessage(onMonitoringListChanged);
    else
        get_service<CMassages>().sendMessage(onMonitoringListChanged);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::addMonitoringTaskForChannel(ChannelParameters::Ptr& channelParams,
                                                  const MonitoringTaskInfo::TaskType taskType,
                                                  CTimeSpan monitoringInterval /* = -1*/)
{
    OUTPUT_LOG_FUNC;
    OUTPUT_LOG_ADD_COMMENT("channel name = %s", channelParams->channelName.GetString());
    OUTPUT_LOG_DO;

    if (monitoringInterval == -1)
        monitoringInterval = monitoring_interval_to_timespan(channelParams->monitoringInterval);

    addMonitoringTaskForChannels({ channelParams }, taskType, monitoringInterval);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                                   const MonitoringTaskInfo::TaskType taskType,
                                                   CTimeSpan monitoringInterval)
{
    // ��������� ��������� �� ������� ����� ��������� �������
    const CTime stopTime = CTime::GetCurrentTime();
    const CTime startTime = stopTime - monitoringInterval;

    // ������ ���������� �������
    std::list<TaskParameters::Ptr> listTaskParams;
    for (const auto& channelParams : channelList)
    {
        listTaskParams.emplace_back(new TaskParameters(channelParams->channelName,
                                                       startTime, stopTime));
    }

    addMonitoringTaskForChannels(channelList, listTaskParams, taskType);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::addMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                                   const std::list<TaskParameters::Ptr>& taskParams,
                                                   const MonitoringTaskInfo::TaskType taskType)
{
    if (channelList.size() != taskParams.size())
    {
        assert(!"����������� ������ ������� � �������!");
        return;
    }

    if (taskParams.empty())
    {
        assert(!"�������� ������ ������ �������!");
        return;
    }

    if (taskType != MonitoringTaskInfo::TaskType::eEveryDayReport)
    {
        // ��� ������� ������� ������ ���������� ����� ������ ����������� ����������
        auto channelsIt = channelList.begin(), channelsItEnd = channelList.end();
        auto taskIt = taskParams.cbegin(), taskItEnd = taskParams.cend();
        for (; taskIt != taskItEnd && channelsIt != channelsItEnd; ++taskIt, ++channelsIt)
        {
            // ���������� �������� ��������� ��������
            (*channelsIt)->m_loadingParametersIntervalEnd = (*taskIt)->endTime;
        }
    }

    MonitoringTaskInfo taskInfo;
    taskInfo.taskType = taskType;
    taskInfo.channelParameters = channelList;

    // ��������� ������� ���������� ������
    m_monitoringTasksInfo.try_emplace(
        get_monitoring_tasks_service()->addTaskList(taskParams, IMonitoringTasksService::eNormal),
        taskInfo);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::delMonitoringTaskForChannel(const ChannelParameters::Ptr& channelParams)
{
    // �������� �� ���� ��������
    for (auto monitoringTaskIt = m_monitoringTasksInfo.begin(), end = m_monitoringTasksInfo.end();
         monitoringTaskIt != end;)
    {
        switch (monitoringTaskIt->second.taskType)
        {
        case MonitoringTaskInfo::TaskType::eIntervalInfo:
        case MonitoringTaskInfo::TaskType::eUpdatingInfo:
            {
                // ������ ������� �� ������� �������� �������
                auto& taskChannels = monitoringTaskIt->second.channelParameters;

                // ���� � ������ ������� ��� �����
                auto it = std::find(taskChannels.begin(), taskChannels.end(), channelParams);
                if (it != taskChannels.end())
                {
                    // �������� ��������� ����� �� �������� ���������
                    *it = nullptr;

                    // ��������� ��� � ������� �������� �� ������ ������
                    if (std::all_of(taskChannels.begin(), taskChannels.end(), [](const auto& el) { return el == nullptr; }))
                    {
                        // �� ������ ������� �� �������� - ����� ������� �������

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
