#include "pch.h"

#include <ctime>

#include <ext/thread/invoker.h>

#include "include/IDirService.h"
#include "MonitoringTaskService/ChannelDataGetter/ChannelDataGetter.h"
#include "TrendMonitoring.h"

#include <chrono>

#include "Utils.h"

// time at which the report will be sent every day hours + minutes (20:00)
const std::pair<int, int> kReportDataTime = std::make_pair(20, 00);

// Реализация сервиса для мониторинга каналов
TrendMonitoring::TrendMonitoring(ext::ServiceProvider::Ptr&& serviceProvider,
                                 std::shared_ptr<IMonitoringTasksService>&& monitoringTasksService)
    : m_monitoringTasksService(std::move(monitoringTasksService))
    , m_appConfig(ext::CreateObject<ApplicationConfiguration>(serviceProvider))
{
    // load configuration from file
    LoadConfiguration();

    // initialize the bot after loading the configuration
    m_telegramBot = ext::GetInterface<telegram::bot::ITelegramBot>(serviceProvider);

    // launch tasks for monitoring, make them separate tasks because they can
    // long time to load data for intervals
    for (auto& channel : m_appConfig->m_chanelParameters)
        AddMonitoringTaskForChannel(channel, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // connect the data update timer
    TickSubscriber::SubscribeInvokedTimer(UpdateDataInterval, TimerType::eUpdatingData);

    // connect the report timer
    {
        // calculate next report time
        time_t nextReportTime_t = time(NULL);

        tm nextReportTm;
        localtime_s(&nextReportTm, &nextReportTime_t);

        // check that this day is not yet the time for the report
        bool needReportToday = false;
        if (nextReportTm.tm_hour < kReportDataTime.first ||
            (nextReportTm.tm_hour == kReportDataTime.first &&
            nextReportTm.tm_min < kReportDataTime.second))
            needReportToday = true;

        // calculate the next report time
        nextReportTm.tm_hour = kReportDataTime.first;
        nextReportTm.tm_min = kReportDataTime.second;
        nextReportTm.tm_sec = 0;

        // if not today, then the next report is needed tomorrow
        if (!needReportToday)
            ++nextReportTm.tm_mday;

        // convert to std::chrono
        std::chrono::system_clock::time_point nextReportTime =
            std::chrono::system_clock::from_time_t(mktime(&nextReportTm));

        // Connect the timer with an interval until the countdown trace
        EXT_UNUSED(m_everyDayReportScheduler.SubscribeTaskAtTime([&]()
        {
            GenerateEveryDayReport();
        }, nextReportTime));
    }
}

std::list<std::wstring> TrendMonitoring::GetNamesOfAllChannels() const
{
    EXT_TRACE_SCOPE() << EXT_TRACE_FUNCTION;

    // fill in the sorted list of channels
    std::list<CString> allChannelsNames;
    // we are looking exactly in the directory with compressed signals because there is less nesting and the search is faster
    ChannelDataGetter::FillChannelList(allChannelsNames, false);

    std::list<std::wstring> result;
    std::transform(allChannelsNames.begin(), allChannelsNames.end(), std::back_inserter(result), [](const auto& name) { return name.GetString(); });
    return result;
}

std::list<std::wstring> TrendMonitoring::GetNamesOfMonitoringChannels() const
{
    // fill in the sorted list of channels
    std::list<std::wstring> allChannelsNames;
    for (const auto& channel :  m_appConfig->m_chanelParameters)
        allChannelsNames.emplace_back(channel->channelName);

    return allChannelsNames;
}

void TrendMonitoring::UpdateDataForAllChannels()
{
    // clear data for all channels and add monitoring tasks
    for (auto& channel : m_appConfig->m_chanelParameters)
    {
        // delete monitoring tasks for the channel
        DelMonitoringTaskForChannel(channel);
        // clear data
        channel->ResetChannelData();
        // add a new task, do one at a time to be able to interrupt a specific one
        AddMonitoringTaskForChannel(channel, MonitoringTaskInfo::TaskType::eIntervalInfo);
    }

    // report a change in the channel list
    OnMonitoringChannelsListChanged();
}

size_t TrendMonitoring::GetNumberOfMonitoringChannels() const
{
    return m_appConfig->m_chanelParameters.size();
}

const MonitoringChannelData& TrendMonitoring::GetMonitoringChannelData(const size_t channelIndex)  const
{
    EXT_ASSERT(channelIndex < m_appConfig->m_chanelParameters.size()) << L"Количество каналов меньше чем индекс канала";
    return (*std::next(m_appConfig->m_chanelParameters.begin(), channelIndex))->GetMonitoringData();
}

size_t TrendMonitoring::AddMonitoringChannel()
{
    EXT_TRACE_SCOPE() << EXT_TRACE_FUNCTION;

    const auto& channelsList = GetNamesOfAllChannels();

    if (channelsList.empty())
    {
        ::MessageBox(NULL, L"Каналы для мониторинга не найдены", L"Невозможно добавить канал для мониторинга", MB_OK | MB_ICONERROR);
        return 0;
    }

    m_appConfig->m_chanelParameters.push_back(std::make_shared<ChannelParameters>(*channelsList.begin()));

    // start loading data
    AddMonitoringTaskForChannel(m_appConfig->m_chanelParameters.back(),
        MonitoringTaskInfo::TaskType::eIntervalInfo);

    // we report a change in the channel list synchronously so that a new one has time to appear in the channel list
    OnMonitoringChannelsListChanged(false);

    return m_appConfig->m_chanelParameters.size() - 1;
}

size_t TrendMonitoring::RemoveMonitoringChannelByIndex(const size_t channelIndex)
{
    auto& channelsList = m_appConfig->m_chanelParameters;

    if (channelIndex >= channelsList.size())
    {
        EXT_ASSERT(false) << L"Количество каналов меньше чем индекс удаляемого канала";
        return channelIndex;
    }

    // get an iterator per channel
    ChannelIt channelIt = std::next(channelsList.begin(), channelIndex);

    // interrupt the monitoring task for the channel
    DelMonitoringTaskForChannel(*channelIt);

    // remove from channel list
    channelIt = channelsList.erase(channelIt);
    if (channelIt == channelsList.end() && !channelsList.empty())
        --channelIt;

    size_t result = std::distance(channelsList.begin(), channelIt);

    // report a change in the channel list
    OnMonitoringChannelsListChanged();

    return result;
}

void TrendMonitoring::ChangeMonitoringChannelNotify(const size_t channelIndex,
                                                    const bool newNotifyState)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить нотификацию канала", MB_OK | MB_ICONERROR);
        return;
    }

    // get the parameters of the channel on which we change the name
    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(),
                                                      channelIndex);
    if (!channelParams->ChangeNotification(newNotifyState))
        return;

    // сообщаем об изменении в списке каналов
    OnMonitoringChannelsListChanged(true);
}

void TrendMonitoring::ChangeMonitoringChannelName(const size_t channelIndex,
                                                  const std::wstring& newChannelName)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить имя канала", MB_OK | MB_ICONERROR);
        return;
    }

    // get the parameters of the channel on which we change the name
    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->ChangeName(newChannelName))
        return;

    // if the name has changed successfully, abort a possible job on the channel
    DelMonitoringTaskForChannel(channelParams);
    // add a new task for monitoring
    AddMonitoringTaskForChannel(channelParams, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // report a change in the channel list
    OnMonitoringChannelsListChanged();
}

void TrendMonitoring::ChangeMonitoringChannelInterval(const size_t channelIndex,
                                                     const MonitoringInterval newInterval)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить интервал наблюдения", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->ChangeInterval(newInterval))
        return;

    // if the name has changed successfully, abort a possible job on the channel
    DelMonitoringTaskForChannel(channelParams);
    // add a new task for monitoring
    AddMonitoringTaskForChannel(channelParams, MonitoringTaskInfo::TaskType::eIntervalInfo);

    // report a change in the channel list
    OnMonitoringChannelsListChanged();
}

void TrendMonitoring::ChangeMonitoringChannelAlarmingValue(const size_t channelIndex,
                                                           const float newValue)
{
    if (channelIndex >= m_appConfig->m_chanelParameters.size())
    {
        ::MessageBox(NULL, L"Канала нет в списке", L"Невозможно изменить значение оповещения", MB_OK | MB_ICONERROR);
        return;
    }

    ChannelParameters::Ptr channelParams = *std::next(m_appConfig->m_chanelParameters.begin(), channelIndex);
    if (!channelParams->ChangeAlarmingValue(newValue))
        return;

    // report a change in the channel list
    OnMonitoringChannelsListChanged();
}

size_t TrendMonitoring::MoveUpMonitoringChannelByIndex(const size_t channelIndex)
{
    auto& channelsList = m_appConfig->m_chanelParameters;

    if (channelsList.size() < 2)
        return channelIndex;

    ChannelIt movingIt = std::next(channelsList.begin(), channelIndex);

    // index of the channel position after moving
    size_t resultIndex;

    if (movingIt == channelsList.begin())
    {
        // move to the end
        channelsList.splice(channelsList.end(), channelsList, movingIt);
        resultIndex = channelsList.size() - 1;
    }
    else
    {
        // swap with previous places
        std::iter_swap(movingIt, std::prev(movingIt));
        resultIndex = channelIndex - 1;
    }

    // report a change in the channel list
    OnMonitoringChannelsListChanged();

    return resultIndex;
}

size_t TrendMonitoring::MoveDownMonitoringChannelByIndex(const size_t channelIndex)
{
    auto& channelsList = m_appConfig->m_chanelParameters;

    if (channelsList.size() < 2)
        return channelIndex;

    ChannelIt movingIt = std::next(channelsList.begin(), channelIndex);

    // index of the channel position after moving
    size_t resultIndex;

    if (movingIt == --channelsList.end())
    {
        // move to the beginning
        channelsList.splice(channelsList.begin(), channelsList, movingIt);
        resultIndex = 0;
    }
    else
    {
        // swap with next places
        std::iter_swap(movingIt, std::next(movingIt));
        resultIndex = channelIndex + 1;
    }

    // report a change in the channel list
    OnMonitoringChannelsListChanged();

    return resultIndex;
}

void TrendMonitoring::OnChanged()
{
    ext::InvokeMethodAsync([&]()
        {
            // save changes to config
            SaveConfiguration();
        });
}

void TrendMonitoring::OnBotSettingsChanged(const bool, const std::wstring&)
{
    ext::InvokeMethodAsync([&]()
        {
            // save changes to config
            SaveConfiguration();
        });
}

void TrendMonitoring::OnCompleteTask(const TaskId& taskId, ResultsPtrList monitoringResult)
{
    auto it = m_monitoringTasksInfo.find(taskId);
    if (it == m_monitoringTasksInfo.end())
        // completed not our task
        return;

    ext::InvokeMethod([&, taskId, monitoringResult, it]()
    {
        EXT_ASSERT(!monitoringResult.empty() && !it->second.channelParameters.empty());

        // iterators by channel parameters
        ChannelIt channelIt = it->second.channelParameters.begin();
        const ChannelIt channelEnd = it->second.channelParameters.end();

        // iterators by job results
        auto resultIt = monitoringResult.begin();
        const auto resultEnd = monitoringResult.end();

        // flag that there were changes in the monitoring list
        bool bMonitoringListChanged = false;

        std::vector<std::wstring> listOfProblemChannels;
        listOfProblemChannels.reserve(monitoringResult.size());

        std::wstring reportTextForAllChannels;
        // for each channel, analyze its task result
        for (; channelIt != channelEnd && resultIt != resultEnd; ++channelIt, ++resultIt)
        {
            EXT_ASSERT((*channelIt)->channelName == (*resultIt)->taskParameters->channelName) << "Wrong channel name data received!";

            std::wstring channelError;
            bMonitoringListChanged |= MonitoringTaskResultHandler::HandleIntervalInfoResult(it->second.taskType, *resultIt,
                channelIt->get(), channelError);

            // if an error occurred while receiving data and you can notify about it
            if (!channelError.empty() && (*channelIt)->bNotify)
            {
                reportTextForAllChannels += std::string_swprintf(L"Канал \"%s\": %s\n",
                    (*channelIt)->channelName.c_str(),
                    channelError.c_str());

                listOfProblemChannels.emplace_back((*channelIt)->channelName);
            }
        }

        std::string_trim_all(reportTextForAllChannels);

        // if errors occur, we process them differently for each task type
        if (it->second.taskType == MonitoringTaskInfo::TaskType::eEveryDayReport)
        {
            // if there is nothing to report, we say that everything is OK
            if (reportTextForAllChannels.empty())
                reportTextForAllChannels = L"Данные в порядке.";

            const std::wstring reportDelimer(25, L'*');

            // create a report message
            std::wstring reportMessage;
            reportMessage = std::string_swprintf(L"%s\n\nЕжедневный отчёт за %s\n\n%s\n%s",
                reportDelimer.c_str(),
                CTime::GetCurrentTime().Format(L"%d.%m.%Y").GetString(),
                reportTextForAllChannels.c_str(),
                reportDelimer.c_str());

            // notify about the finished report
            ext::send_event_async(&IReportEvents::OnReportDone, reportMessage);

            // inform telegram users
            m_telegramBot->SendMessageToAdmins(reportMessage);
        }
        else if (!reportTextForAllChannels.empty())
        {
            EXT_ASSERT(it->second.taskType == MonitoringTaskInfo::TaskType::eIntervalInfo ||
                it->second.taskType == MonitoringTaskInfo::TaskType::eUpdatingInfo);

            // report to the log that there are problems
            send_message_to_log(ILogEvents::LogMessageData::MessageType::eError, reportTextForAllChannels);

            // notify about the error
            auto errorMessage = std::make_shared<IMonitoringErrorEvents::EventData>();
            errorMessage->errorTextForAllChannels = std::move(reportTextForAllChannels);
            errorMessage->problemChannelNames = std::move(listOfProblemChannels);

            // generate an error ID
            EXT_DUMP_IF(FAILED(CoCreateGuid(&errorMessage->errorGUID)));
            ext::send_event(&IMonitoringErrorEvents::OnError, errorMessage);
        }

        if (bMonitoringListChanged)
            ext::send_event_async(&IMonitoringListEvents::OnChanged);

        // remove the task from the list
        m_monitoringTasksInfo.erase(it);
    });
}

void TrendMonitoring::OnTick(ext::tick::TickParam tickParam) EXT_NOEXCEPT
{
    switch (TimerType(tickParam))
    {
    case TimerType::eUpdatingData:
        {
            const CTime currentTime = CTime::GetCurrentTime();

            // because channels could be added at different times, we will do for each channel
            // own task with a certain interval, form a list of task parameters
            std::list<TaskParameters::Ptr> listTaskParams;
            // list of updated channels
            ChannelParametersList updatingDataChannels;

            // go through all channels and look at which data needs to be updated
            for (auto& channelParameters : m_appConfig->m_chanelParameters)
            {
                // data not loaded yet
                if (!channelParameters->channelState.dataLoaded &&
                    !channelParameters->channelState.loadingDataError)
                {
                    // check how long they've been gone
                    if (channelParameters->m_loadingParametersIntervalEnd.has_value() &&
                        (currentTime - *channelParameters->m_loadingParametersIntervalEnd).GetTotalMinutes() > 10)
                        send_message_to_log(ILogEvents::LogMessageData::MessageType::eError,
                                            L"Данные по каналу %s грузятся больше 10 минут",
                                            channelParameters->channelName);

                    continue;
                }

                // should have already loaded the data and filled in
                EXT_ASSERT(channelParameters->m_loadingParametersIntervalEnd.has_value());

                // add the channel to the list of updated
                updatingDataChannels.emplace_back(channelParameters);
                listTaskParams.emplace_back(new TaskParameters(channelParameters->channelName,
                                                               *channelParameters->m_loadingParametersIntervalEnd,
                                                               currentTime));
            }

            if (!listTaskParams.empty())
                AddMonitoringTaskForChannels(updatingDataChannels, listTaskParams,
                                             MonitoringTaskInfo::TaskType::eUpdatingInfo);
        }
        break;
    default:
        EXT_UNREACHABLE(L"Неизвестный таймер!");
        break;
    }
}

void TrendMonitoring::SaveConfiguration() EXT_NOEXCEPT
{
    try
    {
        ext::serializable::serializer::Executor::SerializeObject(
            ext::serializable::serializer::Fabric::XMLSerializer(GetConfigurationXMLFilePath()),
            m_appConfig.get());
    }
    catch (...)
    { }
}

void TrendMonitoring::LoadConfiguration() EXT_NOEXCEPT
{
    try
    {
        ext::serializable::serializer::Executor::DeserializeObject(
            ext::serializable::serializer::Fabric::XMLDeserializer(GetConfigurationXMLFilePath()),
            m_appConfig.get());
    }
    catch (...)
    { }
}

std::wstring TrendMonitoring::GetConfigurationXMLFilePath() const
{
    return std::filesystem::get_exe_directory().append(kConfigFileName);
}

void TrendMonitoring::OnMonitoringChannelsListChanged(bool bAsynchNotify /*= true*/)
{
    EXT_TRACE_SCOPE() << EXT_TRACE_FUNCTION;

    // save the new monitoring list
    SaveConfiguration();

    // notify about a change in the monitoring list
    if (bAsynchNotify)
        ext::send_event_async(&IMonitoringListEvents::OnChanged);
    else
        ext::send_event(&IMonitoringListEvents::OnChanged);
}

void TrendMonitoring::GenerateEveryDayReport()
{
    // Connect the timer with an interval until the next report
    if (!m_everyDayReportScheduler.IsTaskExists(TimerType::eEveryDayReporting))
    {
        EXT_EXPECT(m_everyDayReportScheduler.SubscribeTaskByPeriod([&]()
        {
            GenerateEveryDayReport();
        }, std::chrono::hours(24), TimerType::eEveryDayReporting) == TimerType::eEveryDayReporting);
    }

    ext::get_service<ext::invoke::MethodInvoker>().CallSync([&]()
    {
        if (!m_appConfig->m_chanelParameters.empty())
        {
            // copy of the current channels on which we start monitoring
            ChannelParametersList channelsCopy;
            for (const auto& currentChannel : m_appConfig->m_chanelParameters)
            {
                channelsCopy.push_back(std::make_shared<ChannelParameters>(currentChannel->channelName));
                channelsCopy.back()->alarmingValue = currentChannel->alarmingValue;
            }

            // start the task of generating a report for the last day
            AddMonitoringTaskForChannels(channelsCopy,
                                         MonitoringTaskInfo::TaskType::eEveryDayReport,
                                         CTimeSpan(1, 0, 0, 0));
        }
    });
}

void TrendMonitoring::AddMonitoringTaskForChannel(ChannelParameters::Ptr& channelParams,
                                                  const MonitoringTaskInfo::TaskType taskType,
                                                  CTimeSpan monitoringInterval /* = -1*/)
{
    EXT_TRACE() << EXT_TRACE_FUNCTION << std::string_sprintf("channel name = %s", channelParams->channelName.c_str()).c_str();

    if (monitoringInterval == -1)
        monitoringInterval = monitoring_interval_to_timespan(channelParams->monitoringInterval);

    AddMonitoringTaskForChannels({ channelParams }, taskType, monitoringInterval);
}

void TrendMonitoring::AddMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                                   const MonitoringTaskInfo::TaskType taskType,
                                                   CTimeSpan monitoringInterval)
{
    // form the intervals at which we will run the task
    const CTime stopTime = CTime::GetCurrentTime();
    const CTime startTime = stopTime - monitoringInterval;

    // list of job parameters
    std::list<TaskParameters::Ptr> listTaskParams;
    for (const auto& channelParams : channelList)
    {
        listTaskParams.emplace_back(new TaskParameters(channelParams->channelName,
                                                       startTime, stopTime));
    }

    AddMonitoringTaskForChannels(channelList, listTaskParams, taskType);
}

void TrendMonitoring::AddMonitoringTaskForChannels(const ChannelParametersList& channelList,
                                                   const std::list<TaskParameters::Ptr>& taskParams,
                                                   const MonitoringTaskInfo::TaskType taskType)
{
    if (channelList.size() != taskParams.size())
    {
        EXT_ASSERT(!"The list of channels and tasks differs!");
        return;
    }

    if (taskParams.empty())
    {
        EXT_ASSERT(!"An empty list of tasks was passed!");
        return;
    }

    if (taskType != MonitoringTaskInfo::TaskType::eEveryDayReport)
    {
        // for data request tasks, remember the time of the ends of the loaded intervals
        auto channelsIt = channelList.begin(), channelsItEnd = channelList.end();
        auto taskIt = taskParams.cbegin(), taskItEnd = taskParams.cend();
        for (; taskIt != taskItEnd && channelsIt != channelsItEnd; ++taskIt, ++channelsIt)
        {
            // remember the download end interval
            (*channelsIt)->m_loadingParametersIntervalEnd = (*taskIt)->endTime;
        }
    }

    MonitoringTaskInfo taskInfo;
    taskInfo.taskType = taskType;
    taskInfo.channelParameters = channelList;

    // run data update task
    m_monitoringTasksInfo.try_emplace(
        m_monitoringTasksService->AddTaskList(taskParams, IMonitoringTasksService::TaskPriority::eNormal),
        taskInfo);
}

void TrendMonitoring::DelMonitoringTaskForChannel(const ChannelParameters::Ptr& channelParams)
{
    // Loop through all tasks
    for (auto monitoringTaskIt = m_monitoringTasksInfo.begin(), end = m_monitoringTasksInfo.end();
        monitoringTaskIt != end;)
    {
        switch (monitoringTaskIt->second.taskType)
        {
        case MonitoringTaskInfo::TaskType::eIntervalInfo:
        case MonitoringTaskInfo::TaskType::eUpdatingInfo:
        {
            // list of channels on which the task is launched
            auto& taskChannels = monitoringTaskIt->second.channelParameters;

            // look for our channel in the channel list
            auto it = std::find(taskChannels.begin(), taskChannels.end(), channelParams);
            if (it != taskChannels.end())
            {
                // reset the parameters to not get the result
                *it = nullptr;

                // check that the task has non-empty channels
                if (std::all_of(taskChannels.begin(), taskChannels.end(), [](const auto& el) { return el == nullptr; }))
                {
                    // no empty channels left - we will delete the task
                    m_monitoringTasksService->RemoveTask(monitoringTaskIt->first);
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
