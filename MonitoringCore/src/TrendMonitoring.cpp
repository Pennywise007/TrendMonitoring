#include "pch.h"

#include <ctime>

#include <myInclude/ChannelDataCorrector/ChannelDataGetter.h>
#include <ZetDirs.h>

#include "Serialization/SerializatorFabric.h"
#include "TrendMonitoring.h"

// �������� ���������� ������
const std::chrono::minutes kUpdateDataInterval(5);

// ����� � ������� ����� ���������� ����� ������ ���� (20:00)
const std::pair<int, int> kReportDataTime = std::make_pair(20, 00);

// ��� ����������������� ����� � �����������
const wchar_t kConfigFileName[] = L"AppConfig.xml";

////////////////////////////////////////////////////////////////////////////////
ITrendMonitoring* get_monitoing_service()
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
    EventRecipientImpl::subscribe(onUsersListChangedEvent);

    // ��������� ������������ �� �����
    loadConfiguration();

    // �������������� ���� ����� �������� ������������
    m_telegramBot.initBot(m_appConfig->getTelegramUsers());
    // �������� ���� ��������� �� ��������
    m_telegramBot.setBotSettings(getBotSettings());

    // �������� ������� ��� �����������
    for (auto& channel : m_appConfig->m_chanelParameters)
        addMonitoringTaskForChannel(channel, TaskInfo::TaskType::eIntervalInfo);

    // ���������� ������ ���������� ������
    CTickHandlerImpl::subscribeTimer(kUpdateDataInterval, TimerType::eUpdatingData);

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
        nextReportTm.tm_min = kReportDataTime.second;
        nextReportTm.tm_sec = 0;

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
std::set<CString> TrendMonitoring::getNamesOfAllChannels()
{
    // �������� ��� ������ �� �������
    std::list<std::pair<CString, CString>> channelsWithConversion;
    CChannelDataGetter::FillChannelList(get_service<ZetDirsService>().getCompressedDir(), channelsWithConversion);

    // ��������� ������������� ������ �������
    std::set<CString> allChannelsNames;
    for (const auto& channelInfo : channelsWithConversion)
        allChannelsNames.emplace(channelInfo.first);

    return allChannelsNames;
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
std::list<CString> TrendMonitoring::getNamesOfMonitoringChannels()
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
        addMonitoringTaskForChannel(channel, TaskInfo::TaskType::eIntervalInfo);
    }

    // �������� �� ��������� � ������ �������
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
    assert(channelIndex < m_appConfig->m_chanelParameters.size() && "���������� ������� ������ ��� ������ ������");
    return (*std::next(m_appConfig->m_chanelParameters.begin(), channelIndex))->getMonitoringData();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::addMonitoingChannel()
{
    const auto& channelsList = getNamesOfAllChannels();

    if (channelsList.empty())
    {
        ::MessageBox(NULL, L"������ ��� ����������� �� �������", L"���������� �������� ����� ��� �����������", MB_OK | MB_ICONERROR);
        return 0;
    }

    m_appConfig->m_chanelParameters.push_back(ChannelParameters::make(*channelsList.begin()));

    // �������� ������� ������
    addMonitoringTaskForChannel(m_appConfig->m_chanelParameters.back(),
                                TaskInfo::TaskType::eIntervalInfo);

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
void TrendMonitoring::changeMonitoingChannelName(const size_t channelIndex,
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
    addMonitoringTaskForChannel(channelParams, TaskInfo::TaskType::eIntervalInfo);

    // �������� �� ��������� � ������ �������
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoingChannelInterval(const size_t channelIndex,
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
    addMonitoringTaskForChannel(channelParams, TaskInfo::TaskType::eIntervalInfo);

    // �������� �� ��������� � ������ �������
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
void TrendMonitoring::changeMonitoingChannelAllarmingValue(const size_t channelIndex,
                                                           const float newValue)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"������ ��� � ������", L"���������� �������� �������� ����������", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->changeAllarmingValue(newValue))
        return;

    // �������� �� ��������� � ������ �������
    onMonitoringChannelsListChanged();
}

//----------------------------------------------------------------------------//
// ITrendMonitoring
size_t TrendMonitoring::moveUpMonitoingChannelByIndex(const size_t channelIndex)
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
size_t TrendMonitoring::moveDownMonitoingChannelByIndex(const size_t channelIndex)
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
const TelegramBotSettings& TrendMonitoring::getBotSettings()
{
    return m_appConfig->getTelegramSettings();
}

//----------------------------------------------------------------------------//
void TrendMonitoring::setBotSettings(const TelegramBotSettings& newSettings)
{
    // ��������� ���������
    m_appConfig->setTelegramSettings(newSettings);

    // ��������� ���������
    saveConfiguration();

    // �������������� ���� ����� �������
    m_telegramBot.setBotSettings(getBotSettings());
}

//----------------------------------------------------------------------------//
void TrendMonitoring::handleIntervalInfoResult(const MonitoringResult::ResultData& monitoringResult,
                                               ChannelParameters::Ptr& channelParams)
{
    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // ������ ������� ��������
        {
            // ����������� ����� ������ �� �������
            channelParams->setTrendChannelData(monitoringResult);

            channelParams->channelState[MonitoringChannelData::eDataLoaded] = true;
        }
        break;
    case MonitoringResult::Result::eNoData:     // � ���������� ��������� ��� ������
    case MonitoringResult::Result::eErrorText:  // �������� ������
        {
            // ��������� � ��������� ������
            if (monitoringResult.resultType == MonitoringResult::Result::eErrorText &&
                !monitoringResult.errorText.IsEmpty())
                send_message_to_log(LogMessageData::MessageType::eError, monitoringResult.errorText);
            else
                send_message_to_log(LogMessageData::MessageType::eError, L"��� ������ � ����������� ���������");

            // ��������� ����� ��� ������
            channelParams->trendData.emptyDataTime = monitoringResult.emptyDataTime;

            // �������� ��� �������� ������ ��������
            channelParams->channelState[MonitoringChannelData::eLoadingError] = true;
        }
        break;
    default:
        assert(!"�� ��������� ��� ����������");
        break;
    }

    // ��������� �� ��������� � ������ ��� �����������
    get_service<CMassages>().postMessage(onMonitoringListChanged);
}

//----------------------------------------------------------------------------//
void TrendMonitoring::handleUpdatingResult(const MonitoringResult::ResultData& monitoringResult,
                                           ChannelParameters::Ptr& channelParams)
{
    // ����� ������ ���� ��� ���������
    CString errorText;

    switch (monitoringResult.resultType)
    {
    case MonitoringResult::Result::eSucceeded:  // ������ ������� ��������
        {
            // ���� ������ ��� �� ���� ��������
            if (!channelParams->channelState[MonitoringChannelData::eDataLoaded])
                // ����������� ����� ������ �� �������
                channelParams->setTrendChannelData(monitoringResult);
            else
            {
                // ������ ������ � ����� ������
                {
                    channelParams->trendData.currentValue = monitoringResult.currentValue;

                    if (channelParams->trendData.maxValue < monitoringResult.maxValue)
                        channelParams->trendData.maxValue = monitoringResult.maxValue;
                    if (channelParams->trendData.minValue > monitoringResult.minValue)
                        channelParams->trendData.minValue = monitoringResult.minValue;

                    channelParams->trendData.emptyDataTime += monitoringResult.emptyDataTime;
                    channelParams->trendData.lastDataExistTime = monitoringResult.lastDataExistTime;
                }
            }

            // ����������� ����� ������
            {
                // ���� ����������� ���������� � �� �������� �� ���� �������� ������
                if (_finite(channelParams->allarmingValue) != 0)
                {
                    // ���� �� �������� ��� �������� ����� �� ����������
                    if ((channelParams->allarmingValue >= 0 &&
                         monitoringResult.minValue >= channelParams->allarmingValue) ||
                         (channelParams->allarmingValue < 0 &&
                          monitoringResult.maxValue <= channelParams->allarmingValue))
                    {
                        errorText.AppendFormat(L"���������� ���������� �������� � ������ %s. ",
                                               channelParams->channelName.GetString());

                        channelParams->channelState[MonitoringChannelData::eReportedExcessOfValue] = true;
                    }
                }

                // ���� ������ ����. �� ��������� �����
                if (monitoringResult.emptyDataTime.GetTotalSeconds() >
                    std::chrono::duration_cast<std::chrono::seconds>(kUpdateDataInterval).count() / 2)
                {
                    errorText.AppendFormat(L"����� ��������� ������ � ������ %s. ",
                                           channelParams->channelName.GetString());

                    channelParams->channelState[MonitoringChannelData::eReportedALotOfEmptyData] = true;
                }
            }

            channelParams->channelState[MonitoringChannelData::eDataLoaded] = true;
            if (channelParams->channelState[MonitoringChannelData::eLoadingError])
            {
                // ���� ���� ������ ��� �������� ������ ������� ���� ������ � �������� ��� ������ ���������
                channelParams->channelState[MonitoringChannelData::eLoadingError] = false;

                send_message_to_log(LogMessageData::MessageType::eOrdinary,
                                    L"������ ������ %s ��������",
                                    channelParams->channelName.GetString());
            }

            // ��������� �� ��������� � ������ ��� �����������
            get_service<CMassages>().postMessage(onMonitoringListChanged);
        }
        break;
    case MonitoringResult::Result::eNoData:     // � ���������� ��������� ��� ������
    case MonitoringResult::Result::eErrorText:  // �������� ������
        {
            // ��������� ����� ��� ������
            channelParams->trendData.emptyDataTime += monitoringResult.emptyDataTime;

            // �������� ��� �������� ������ ��������
            if (!channelParams->channelState[MonitoringChannelData::eLoadingError])
            {
                channelParams->channelState[MonitoringChannelData::eLoadingError] = true;

                errorText.AppendFormat(L"�� ������� �������� ������ � ������ %s. ",
                                       channelParams->channelName.GetString());
            }

            // ��������� ��������� ������ ���� ��� ���� ��������� ������
            if (channelParams->channelState[MonitoringChannelData::eDataLoaded])
                // ��������� �� ��������� � ������ ��� �����������
                get_service<CMassages>().postMessage(onMonitoringListChanged);

            // ���� �� ������ ���� ��������� ������, � ������ ��������� �� ����������
            // ������ ��������� ���������� - ��������� ���� �� ��������� �� ����
            if (channelParams->channelState[MonitoringChannelData::eDataLoaded] &&
                !channelParams->channelState[MonitoringChannelData::eReportedFallenOff])
            {
                // ���� ��� ��� ������� �� �������� ������������ �������� ��������� ���
                if (!CTickHandlerImpl::isTimerExist(TimerType::eFallenOffReporting))
                    // ��������� ������ �� ����� ���� �������� - ���� ��� ������� �������� � ����� �� ��� �����
                    // ��� ��� ��������� ������ �������
                    CTickHandlerImpl::subscribeTimer(3 * kUpdateDataInterval, TimerType::eFallenOffReporting);
            }
        }
        break;
    default:
        assert(!"�� ��������� ��� ����������");
        break;
    }

    if (!errorText.IsEmpty())
    {
        errorText = errorText.Trim();

        // ����� � ���
        send_message_to_log(LogMessageData::MessageType::eError, errorText);

        // ��������� � ��������� ������
        auto errorMessage = std::make_shared<MessageTextData>();
        errorMessage->messageText = std::move(errorText);
        get_service<CMassages>().postMessage(onMonitoringErrorEvent, 0,
                                             std::static_pointer_cast<IEventData>(errorMessage));
    }
}

//----------------------------------------------------------------------------//
void TrendMonitoring::handleEveryDayReportResult(const MonitoringResult::ResultsList& monitoringResults,
                                                 const ChannelParametersList& channelParameters)
{
    assert(!monitoringResults.empty() && !channelParameters.empty());

    // ��������� �� ���������� �������
    ChannelParametersList::const_iterator channelIt = channelParameters.begin(),
                                          channelEnd = channelParameters.end();

    // ��������� �� ����������� �������
    MonitoringResult::ResultIt resultIt = monitoringResults.begin(),
                               resultEnd = monitoringResults.end();

    // ����� � ������� ����������� �������
    CString channelsReportResult;

    for (; channelIt != channelEnd && resultIt != resultEnd; ++channelIt, ++resultIt)
    {
        // ������ �����������
        const MonitoringChannelData& monitoringData = (*channelIt)->getMonitoringData();

        CString channelInfo;
        channelInfo.Format(L"����� \"%s\": ", monitoringData.channelName.GetString());

        switch (resultIt->resultType)
        {
        case MonitoringResult::Result::eSucceeded:  // ������ ������� ��������
            {
                bool bError = false;

                // ���� ����������� ���������� ��� ���������� ��������
                if (_finite(monitoringData.allarmingValue) != 0)
                {
                    // ���� �� �������� ���� �� �������� ����� �� ����������
                    if ((monitoringData.allarmingValue >= 0 &&
                         resultIt->maxValue >= monitoringData.allarmingValue) ||
                         (monitoringData.allarmingValue < 0 &&
                          resultIt->minValue <= monitoringData.allarmingValue))
                    {
                        channelInfo.AppendFormat(L"���������� ������� ��� ��������. ���������� �������� %.02f, �������� �� ���� [%.02f..%.02f].",
                                                 monitoringData.allarmingValue,
                                                 resultIt->minValue, resultIt->maxValue);

                        bError = true;
                    }
                }

                // ���� ����� ��������� ������
                if (resultIt->emptyDataTime.GetTotalHours() > 2)
                {
                    channelInfo.AppendFormat(L"����� ��������� ������ (%lld �).",
                                             resultIt->emptyDataTime.GetTotalHours());

                    bError = true;
                }

                // �������� ������ ���� ���� �������� � �������
                if (!bError)
                    continue;
            }
            break;
        case MonitoringResult::Result::eNoData:     // � ���������� ��������� ��� ������
        case MonitoringResult::Result::eErrorText:  // �������� ������
            {
                // �������� ��� ������ ���
                channelInfo += L"��� ������.";
            }
            break;
        default:
            assert(!"�� ��������� ��� ����������");
            break;
        }

        channelsReportResult += channelInfo + L"\n";
    }

    // ���� �� � ��� �������� ������� ��� ��� ��
    if (channelsReportResult.IsEmpty())
        channelsReportResult = L"������ � �������.";

    CString reportDelimer(L'*', 25);

    // ������� ��������� �� ������
    auto reportMessage = std::make_shared<MessageTextData>();
    reportMessage->messageText.Format(L"%s\n\n���������� ����� �� %s\n\n%s\n%s",
                                      reportDelimer.GetString(),
                                      CTime::GetCurrentTime().Format(L"%d.%m.%Y").GetString(),
                                      channelsReportResult.GetString(),
                                      reportDelimer.GetString());

    // ��������� � ������� ������
    get_service<CMassages>().postMessage(onReportPreparedEvent, 0,
                                         std::static_pointer_cast<IEventData>(reportMessage));

    // �������� ������������� ���������
    m_telegramBot.sendMessageToAdmins(reportMessage->messageText);
}

//----------------------------------------------------------------------------//
// IEventRecipient
void TrendMonitoring::onEvent(const EventId& code, float eventValue,
                              std::shared_ptr<IEventData> eventData)
{
    if (code == onCompletingMonitoringTask)
    {
        std::shared_ptr<MonitoringResult> monitoringResult =
            std::static_pointer_cast<MonitoringResult>(eventData);

        auto it = m_monitoringTasksInfo.find(monitoringResult->m_taskId);
        if (it == m_monitoringTasksInfo.end())
            return;

        switch (it->second.taskType)
        {
        case TaskInfo::TaskType::eIntervalInfo:
        case TaskInfo::TaskType::eUpdatingInfo:
            {
                // ��������� �� ���������� �������
                ChannelIt channelIt = it->second.channelParameters.begin();
                ChannelIt channelEnd = it->second.channelParameters.end();

                // ��������� �� ����������� �������
                MonitoringResult::ResultIt resultIt = monitoringResult->m_taskResults.begin();
                MonitoringResult::ResultIt resultEnd = monitoringResult->m_taskResults.end();

                // �������� ��������� � ��������� ������ �� �������� �������� �������
                for (; channelIt != channelEnd && resultIt != resultEnd; ++channelIt, ++resultIt)
                {
                    // ���� �������� ������ ������ �� ������ �� �������� ������� � ������ ����� ������� ���������
                    if (!*channelIt)
                        continue;

                    switch (it->second.taskType)
                    {
                    case TaskInfo::TaskType::eIntervalInfo:
                        handleIntervalInfoResult(*resultIt, *channelIt);
                        break;
                    case TaskInfo::TaskType::eUpdatingInfo:
                        handleUpdatingResult(*resultIt, *channelIt);
                        break;
                    default:
                        assert(!"�� ��������� ��� �������!");
                        break;
                    }
                }
            }
            break;
        case TaskInfo::TaskType::eEveryDayReport:
            handleEveryDayReportResult(monitoringResult->m_taskResults, it->second.channelParameters);
            break;
        }

        // ������� ������� �� ������
        m_monitoringTasksInfo.erase(it);
    }
    else if (code == onUsersListChangedEvent)
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
            CTime currentTime = CTime::GetCurrentTime();

            // �������� �� ���� ������� � ������� �� ����� ���� �������� ������
            for (auto& channelParameters : m_appConfig->m_chanelParameters)
            {
                // ������ ��� �� ���������
                if (!channelParameters->channelState[MonitoringChannelData::eDataLoaded] &&
                    !channelParameters->channelState[MonitoringChannelData::eLoadingError])
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
                    kUpdateDataInterval.count() - 1)
                    continue;

                // ��������� ������� ���������� ��������� c ������� ���������� ���������� �� �������� �������
                addMonitoringTaskForChannel(
                    channelParameters, TaskInfo::TaskType::eUpdatingInfo,
                    currentTime - *channelParameters->m_loadingParametersIntervalEnd);
            }
        }
        break;
    case TimerType::eFallenOffReporting:
        {
            // ������ ������������ �������
            ChannelParametersList fallenOffChannels;

            CTime currentTime = CTime::GetCurrentTime();
            // �������� �� ���� ������� � ������� �� ����� ���� �������� �� �����������
            for (auto& channelParameters : m_appConfig->m_chanelParameters)
            {
                if (!channelParameters->channelState[MonitoringChannelData::eDataLoaded])
                    // ������ ��� �� ���������
                    continue;
                if (channelParameters->channelState[MonitoringChannelData::eReportedFallenOff])
                    // ��� �������� �� �����������
                    continue;

                // ���� ������ ��� ���������� ������ � ������� ��������� ������ �� ������
                if ((currentTime - channelParameters->trendData.lastDataExistTime).GetTotalMinutes() >=
                    2 * kUpdateDataInterval.count())
                    fallenOffChannels.push_back(channelParameters);
            }

            // ���� ���� ������������ ������
            if (!fallenOffChannels.empty())
            {
                // �������� ����� ������
                CString reportText = L"������� ������ �� �������:";
                for (auto& channelParameters : fallenOffChannels)
                {
                    reportText += L" " + channelParameters->channelName + L";";

                    channelParameters->channelState[MonitoringChannelData::eReportedFallenOff] = true;
                }

                // ����� ������ � ���
                send_message_to_log(LogMessageData::MessageType::eError, reportText);

                // ��������� � ��������� ������
                auto errorMessage = std::make_shared<MessageTextData>();
                errorMessage->messageText = std::move(reportText);
                get_service<CMassages>().postMessage(onMonitoringErrorEvent, 0,
                                                     std::static_pointer_cast<IEventData>(errorMessage));
            }
        }
        // ����������� ���� ��� ������
        return false;
    case TimerType::eEveryDayReporting:
        {
            // ���������� ������ � ���������� �� ���� ������
            CTickHandlerImpl::subscribeTimer(std::chrono::hours(24),
                                             TimerType::eEveryDayReporting);

            if (!m_appConfig->m_chanelParameters.empty())
            {
                // ����� ������� ������� �� ������� ��������� ����������
                ChannelParametersList channelsCopy;
                for (const auto& currentChannel : m_appConfig->m_chanelParameters)
                {
                    channelsCopy.push_back(ChannelParameters::make(currentChannel->channelName));
                    channelsCopy.back()->allarmingValue = currentChannel->allarmingValue;
                }

                // ��������� ������� ������������ ������ �� ��������� ����
                addMonitoringTaskForChannels(channelsCopy,
                                             TaskInfo::TaskType::eEveryDayReport,
                                             CTimeSpan(1, 0, 0, 0));
            }
        }
        // ����������� ���� ��� ������
        return false;
    default:
        assert(!"����������� ������!");
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
    return get_service<ZetDirsService>().getCurrentDir() + kConfigFileName;
}

//----------------------------------------------------------------------------//
void TrendMonitoring::onMonitoringChannelsListChanged(bool bAsynchNotify /*= true*/)
{
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
                                                  const TaskInfo::TaskType taskType,
                                                  CTimeSpan monitoringInterval /* = -1*/)
{
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
    if (channelList.empty())
    {
        assert(!"�������� ������ ������ ������� ��� �������!");
        return;
    }

    // ��������� ��������� �� ������� ����� ��������� �������
    CTime stopTime = CTime::GetCurrentTime();
    CTime startTime = stopTime - monitoringInterval;

    // ������ ���������� �������
    std::list<TaskParameters::Ptr> listTaskParams;

    // ���������� ����� ��������� �� �������� ���� ��������� ������
    for (auto& channelParams : channelList)
    {
        channelParams->m_loadingParametersIntervalEnd = stopTime;

        listTaskParams.emplace_back(new TaskParameters(channelParams->channelName,
                                                       startTime, stopTime));
    }

    TaskInfo taskInfo;
    taskInfo.taskType = taskType;
    taskInfo.channelParameters = channelList;

    m_monitoringTasksInfo.try_emplace(
        get_monitoing_tasks_service()->addTaskList(listTaskParams,
                                                   IMonitoringTasksService::eNormal),
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
        case TaskInfo::TaskType::eIntervalInfo:
        case TaskInfo::TaskType::eUpdatingInfo:
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
                    if (std::all_of(taskChannels.begin(), taskChannels.end(),
                                    [](const auto& el)
                                    {
                                        return el == nullptr;
                                    }))
                    {
                        // �� ������ ������� �� �������� - ����� ������� �������

                        get_monitoing_tasks_service()->removeTask(monitoringTaskIt->first);
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
