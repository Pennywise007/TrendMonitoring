#include "pch.h"

#include <regex>

#pragma warning( push )
#pragma warning( disable: 4996 )
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/std_pair.hpp> // for CallbackParser
#pragma warning( pop )

#include <include/IMonitoringTasksService.h>
#include <include/ITrendMonitoring.h>

#include <ext/std/string.h>
#include <ext/trace/tracer.h>

#include "CommandsInfoService.h"
#include "TelegramCallbacks.h"
#include "KeyboardCallback.h"

namespace telegram::callback {

using namespace command;
using namespace users;

// Maximum number of last errors stored by the program
constexpr size_t g_kMaxErrorInfoCount = 200;

// parsing callbacks
namespace CallbackParser
{
using namespace boost::spirit::x3;

namespace
{
template <typename T>
struct as_type
{
    template <typename Expr>
    auto operator[](Expr expr) const
    {
        return rule<struct _, T>{"as"} = expr;
    }
};

template <typename T>
static const as_type<T> as = {};
}
auto quoted = [](char q)
{
    return lexeme[q >> *(q >> char_(q) | '\\' >> char_ | char_ - q) >> q];
};

auto value = quoted('\'') | quoted('"');
auto key = lexeme[+alpha];
auto pair = key >> "={" >> value >> '}';
auto parser = skip(space)[*as<std::pair<TelegramCallbacks::CallBackParams::key_type, TelegramCallbacks::CallBackParams::mapped_type>>[pair]];
}

TelegramCallbacks::TelegramCallbacks(ITrendMonitoring::Ptr&& trendMonitoring,
                                     std::shared_ptr<IMonitoringTasksService>&& monitoringTaskService,
                                     std::shared_ptr<ITelegramThread>&& telegramThread,
                                     std::shared_ptr<users::ITelegramUsersList>&& userList)
    : m_trendMonitoring(std::move(trendMonitoring))
    , m_monitoringTaskService(std::move(monitoringTaskService))
    , m_telegramThread(std::move(telegramThread))
    , m_telegramUsers(std::move(userList))
{
    // TODO C++20 replace to template lambda
    auto addCommandCallback = [commandCallbacks = &m_commandCallbacks]
        (const std::string& keyWord, const CommandCallback& callback)
        {
            if (!commandCallbacks->try_emplace(keyWord, callback).second)
                EXT_ASSERT(!"Callback keywords must be different!");
        };
    addCommandCallback(report       ::kKeyWord, &TelegramCallbacks::ExecuteCallbackReport);
    addCommandCallback(restart      ::kKeyWord, &TelegramCallbacks::ExecuteCallbackRestart);
    addCommandCallback(resend       ::kKeyWord, &TelegramCallbacks::ExecuteCallbackResend);
    addCommandCallback(alertEnabling::kKeyWord, &TelegramCallbacks::ExecuteCallbackAlert);
    addCommandCallback(alarmingValue::kKeyWord, &TelegramCallbacks::ExecuteCallbackAlarmValue);
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::OnCallbackQuery(const TgBot::CallbackQuery::Ptr& query)
{
    m_telegramUsers->SetUserLastCommand(query->from, query->data);

    try
    {
        // parse the callback and check which parameters are missing
        CallBackParams callBackParams;

        for (auto& [keyWord, callback] : m_commandCallbacks)
        {
            if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                         keyWord >> CallbackParser::parser, callBackParams))
            {
                if (!ext::get_service<CommandsInfoService>().EnsureNeedAnswerOnCallback(*m_telegramUsers, keyWord, query))
                {
                    // пользователь отправивший сообщение
                    const TgBot::User::Ptr& pUser = query->message->from;

                    if (m_telegramUsers->GetUserStatus(pUser) == ITelegramUsersList::eNotAuthorized)
                        m_telegramThread->SendMessage(query->message->chat->id, L"Для работы бота вам необходимо авторизоваться.");
                    else
                        m_telegramThread->SendMessage(query->message->chat->id, L"Неизвестная или более не доступная команда.");
                }
                else
                    (this->*callback)(query->from, query->message, callBackParams, false);

                return;
            }
        }

        throw std::runtime_error(std::narrow(L"Ошибка разбора команды"));
    }
    catch (const std::exception& exc)
    {
        std::cerr << exc.what() << " Callback '" << getUNICODEString(query->data).c_str() << "'" << std::endl;
        EXT_ASSERT(false);

        // supplement the error with the request text
        std::wstring errorStr = std::string_swprintf(L"%s. Обратитесь к администратору системы, текст запроса \"%s\"",
                                                     std::widen(exc.what()).c_str(), getUNICODEString(query->data).c_str());

        // we answer at least something to the user
        m_telegramThread->SendMessage(query->message->chat->id, errorStr);
        // notify about the error
        send_message_to_log(ILogEvents::LogMessageData::MessageType::eError, errorStr);
    }
}

//----------------------------------------------------------------------------//
bool TelegramCallbacks::GotResponseToPreviousCallback(const TgBot::Message::Ptr& commandMessage)
{
    const std::string lastBotCommand = m_telegramUsers->GetUserLastCommand(commandMessage->from);

    try
    {
        // parse the callback and check which parameters are missing
        CallBackParams callBackParams;

        const std::initializer_list<std::string> callbacksWithAnswer = { alarmingValue::kKeyWord };
        for (const auto& callbackKeyWord : callbacksWithAnswer)
        {
            if (boost::spirit::x3::parse(lastBotCommand.begin(), lastBotCommand.end(),
                                         callbackKeyWord >> CallbackParser::parser, callBackParams))
            {
                if (const auto callbackIt = m_commandCallbacks.find(callbackKeyWord);
                    callbackIt != m_commandCallbacks.end())
                {
                    const CommandCallback& callback = callbackIt->second;
                    (this->*callback)(commandMessage->from, commandMessage, callBackParams, true);
                    return true;
                }
                else
                {
                    const std::wstring error = L"Отсутствует обработчик для команды " + std::widen(callbackKeyWord.c_str()) + L"!";
                    std::wcerr << error;
                    EXT_ASSERT(false);
                    throw std::runtime_error(std::narrow(error.c_str()).c_str());
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        EXT_TRACE() << EXT_TRACE_FUNCTION << std::string_swprintf(L"Неизвестное сообщение от пользователя: %s\nТекст сообщения: %s\nПоследняя команда пользователя: %s\nОшибка: %s",
                            getUNICODEString(commandMessage->from->username).c_str(),
                            getUNICODEString(commandMessage->text).c_str(),
                            getUNICODEString(lastBotCommand).c_str(),
                            std::widen(exception.what()).c_str()).c_str();
    }

    return false;
}

void TelegramCallbacks::OnCompleteTask(const TaskId& taskId, IMonitoringTaskEvent::ResultsPtrList monitoringResult)
{
    // check that this is our task
    const auto taskIt = m_monitoringTasksInfo.find(taskId);
    if (taskIt == m_monitoringTasksInfo.end())
        return;

    // admin receive detailed reports
    const bool bDetailedInfo = taskIt->second.userStatus == ITelegramUsersList::UserStatus::eAdmin;
    // memorise chat id
    const int64_t chatId = taskIt->second.chatId;

    // remove task from list
    m_monitoringTasksInfo.erase(taskIt);

    // generating report
    std::wstring reportText = std::string_swprintf(L"Отчёт за %s - %s\n\n",
                      monitoringResult.front()->taskParameters->startTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString(),
                      monitoringResult.front()->taskParameters->endTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString());

    // generating report of all channels
    for (auto& channelResData : monitoringResult)
    {
        reportText += std::string_swprintf(L"Канал \"%s\":\n", channelResData->taskParameters->channelName.c_str());

        switch (channelResData->resultType)
        {
        case IMonitoringTaskEvent::Result::eSucceeded:  // данные успешно получены
        {
            if (bDetailedInfo)
            {
                // value, upon reaching which it is necessary to notify. Do Not Notify - NAN
                float alarmingValue = NAN;

                // if the report is detailed - look for what alert value the channel has
                for (size_t i = 0, count = m_trendMonitoring->GetNumberOfMonitoringChannels();
                    i < count; ++i)
                {
                    const MonitoringChannelData& channelData = m_trendMonitoring->GetMonitoringChannelData(i);
                    if (channelData.channelName == channelResData->taskParameters->channelName)
                    {
                        alarmingValue = channelData.alarmingValue;
                        break;
                    }
                }

                // if we have found a value at which it is worth notifying, we check if this value is exceeded
                if (isfinite(alarmingValue))
                {
                    // if during the interval one of the values ??is out of range
                    if ((alarmingValue >= 0 && channelResData->maxValue >= alarmingValue) ||
                        (alarmingValue < 0 && channelResData->minValue <= alarmingValue))
                        reportText += std::string_swprintf(L" допустимый уровень %.02f был превышен. ", alarmingValue);
                }

                // allowed to have 10% data loses
                const CTimeSpan permissibleEmptyDataTime =
                    (channelResData->taskParameters->endTime - channelResData->taskParameters->startTime).GetTotalSeconds() / 30;

                // notify about big data loses
                if (channelResData->emptyDataTime > permissibleEmptyDataTime)
                    reportText += std::string_swprintf(L"Много пропусков данных (%s).\n",
                                                       time_span_to_string(channelResData->emptyDataTime).c_str());
            }

            reportText += std::string_swprintf(L"Значения за интервал [%.02f..%.02f], последнее показание - %.02f.\n",
                                               channelResData->minValue, channelResData->maxValue,
                                               channelResData->currentValue);
        }
        break;
        case IMonitoringTaskEvent::Result::eNoData:     // there is no data in the passed interval
        case IMonitoringTaskEvent::Result::eErrorText:  // an error occurred
        {
            // notify about the error
            if (!channelResData->errorText.empty())
                reportText += channelResData->errorText;
            else
                reportText += L"Нет данных в запрошенном интервале.";
        }
        break;
        default:
            EXT_ASSERT(false) << L"Не известный тип результата";
            break;
        }

        reportText += L"\n";
    }

    // send the text of the report as a response
    m_telegramThread->SendMessage(chatId, reportText);
}

void TelegramCallbacks::OnError(const std::shared_ptr<IMonitoringErrorEvents::EventData>& errorData)
{
    EXT_ASSERT(!errorData->errorTextForAllChannels.empty());
    // get a list of admin chats
    const auto adminsChats = m_telegramUsers->GetAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin);
    if (adminsChats.empty() || errorData->errorTextForAllChannels.empty())
        return;

    // add a new error to the list and remember its ID
    m_monitoringErrors.emplace_back(errorData.get());

    if (m_monitoringErrors.size() > g_kMaxErrorInfoCount)
        m_monitoringErrors.pop_front();

    // Add action buttons for this error
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // callback to restart monitoring
    KeyboardCallback callbackRestart(restart::kKeyWord);

    // callback to send this message to normal users
    KeyboardCallback callBackOrdinaryUsers(resend::kKeyWord);
    callBackOrdinaryUsers.AddCallbackParam(resend::kParamId, std::wstring(CComBSTR(errorData->errorGUID)));

    keyboard->inlineKeyboard.push_back({ create_keyboard_button(L"Перезапустить систему", callbackRestart),
                                       create_keyboard_button(L"Оповестить обычных пользователей", callBackOrdinaryUsers) });

    // TODO test
    if (!errorData->problemChannelNames.empty() && errorData->problemChannelNames.size() < 4)
    {
        for (auto& channelName : errorData->problemChannelNames)
        {
            KeyboardCallback turnOffNotifications(alertEnabling::kKeyWord);
            turnOffNotifications.AddCallbackParam(alertEnabling::kParamEnable, L"false");
            turnOffNotifications.AddCallbackParam(alertEnabling::kParamChan, channelName);

            KeyboardCallback changeAlarmValue(alarmingValue::kKeyWord);
            changeAlarmValue.AddCallbackParam(alarmingValue::kParamChan, channelName);

            KeyboardCallback channelReport(report::kKeyWord);
            channelReport.AddCallbackParam(report::kParamChan, channelName);

            keyboard->inlineKeyboard.push_back({ create_keyboard_button(channelName + L" выключить оповещения", turnOffNotifications),
                                               create_keyboard_button(channelName + L" изменить уровень", changeAlarmValue),
                                               create_keyboard_button(channelName + L" сформировать отчёт", channelReport) });
        }
    }

    // send error text and keyboard to all admins to solve problems
    m_telegramThread->SendMessage(adminsChats, errorData->errorTextForAllChannels, false, 0, keyboard);
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::ExecuteCallbackReport(const TgBot::User::Ptr& /*from*/, const TgBot::Message::Ptr& message,
                                              const CallBackParams& reportParams, bool /*gotAnswer*/)
{
    // The report callback should have certain parameters, the final callback should be of the form
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(OPTIONAL) kParamInterval={'1000000'}
    const auto channelIt = reportParams.find(report::kParamChan);
    const auto reportTypeIt = reportParams.find(report::kParamType);
    if (reportTypeIt == reportParams.end() && channelIt == reportParams.end())
        throw std::runtime_error(std::narrow(L"Не известный колбэк.").c_str());

    // get a list of channels
    std::list<std::wstring> monitoringChannels = m_trendMonitoring->GetNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error(std::narrow(L"Не удалось получить список каналов, попробуйте повторить попытку").c_str());

    report::ReportType reportType = report::ReportType::eSpecialChannel;
    if (reportTypeIt != reportParams.end())
        reportType = (report::ReportType)std::stoul(reportTypeIt->second);
    else
        EXT_ASSERT(channelIt != reportParams.end());

    // the first callback of the format "kKeyWord kParamType={'ReportType'}" and check if we need to ask on which channel the report is needed
    switch(reportType)
    {
    default:
        EXT_ASSERT("Unknown report type.");
        [[fallthrough]];
    case report::ReportType::eSpecialChannel:
    {
        // if the channel is not specified, you need to request it
        if (channelIt == reportParams.end())
        {
            // form a callback
            KeyboardCallback defCallBack(report::kKeyWord);
            defCallBack.AddCallbackParam(reportTypeIt->first, reportTypeIt->second);

            // show the user the buttons in the channel selection for which the report is needed
            TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            keyboard->inlineKeyboard.reserve(monitoringChannels.size());

            for (const auto& channel : monitoringChannels)
            {
                const auto channelButton =
                    create_keyboard_button(channel, KeyboardCallback(defCallBack).AddCallbackParam(report::kParamChan, channel));

                keyboard->inlineKeyboard.push_back({ channelButton });
            }

            m_telegramThread->SendMessage(message->chat->id, L"Выберите канал", false, 0, keyboard);
            return;
        }
    }
    break;
    case report::ReportType::eAllChannels:
        break;
    }

    const auto timeIntervalIt = reportParams.find(report::kParamInterval);
    // check that kParamInterval is set
    if (timeIntervalIt == reportParams.end())
    {
        // form a callback
        KeyboardCallback defCallBack(report::kKeyWord);
        defCallBack.AddCallbackParam(reportTypeIt->first, reportTypeIt->second);

        // if a name is specified, add it
        if (channelIt != reportParams.end())
            defCallBack.AddCallbackParam(channelIt->first, channelIt->second);

        // ask the user to set the interval
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        keyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);

        // add buttons with all intervals
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            const auto intervalButton = create_keyboard_button(
                monitoring_interval_to_string(MonitoringInterval(i)),
                KeyboardCallback(defCallBack).AddCallbackParam(report::kParamInterval, std::to_wstring(i)));

            keyboard->inlineKeyboard[i] = { intervalButton };
        }

        m_telegramThread->SendMessage(message->chat->id,
                                      L"Выберите интервал времени за который нужно показать отчёт",
                                      false, 0, keyboard);
    }
    else
    {
        // received all the necessary parameters, start the monitoring task
        // list of channels to monitor
        std::list<std::wstring> channelsForTask;

        switch(reportType)
        {
        default:
            EXT_ASSERT("Unknown report type.");
            [[fallthrough]];
        case report::ReportType::eAllChannels:
            channelsForTask = std::move(monitoringChannels);
            break;

        case report::ReportType::eSpecialChannel:
            if (channelIt == reportParams.end())
                throw std::runtime_error(std::narrow(L"Не удалось распознать имя канала, попробуйте повторить попытку").c_str());

            channelsForTask.emplace_back(getUNICODEString(channelIt->second).c_str());
            break;
        }

        const CTime stopTime = CTime::GetCurrentTime();
        const CTime startTime = stopTime -
            monitoring_interval_to_timespan((MonitoringInterval)std::stoi(timeIntervalIt->second));

        TaskInfo taskInfo;
        taskInfo.chatId = message->chat->id;
        taskInfo.userStatus = m_telegramUsers->GetUserStatus(message->from);

        // Reply to the user that the request is being executed, it may take a long time
        // the user may be afraid that nothing is happening
        m_telegramThread->SendMessage(message->chat->id,
                                      L"Выполняется расчёт данных, это может занять некоторое время.");

        m_monitoringTasksInfo.try_emplace(
            m_monitoringTaskService->AddTaskList(channelsForTask, startTime, stopTime,
                                                 IMonitoringTasksService::TaskPriority::eHigh),
            std::move(taskInfo));
    }
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::ExecuteCallbackRestart(const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message,
                                               const CallBackParams& params, bool /*gotAnswer*/)
{
    EXT_ASSERT(params.empty()) << L"Параметров не предусмотрено";

    // simulate that the user has completed the restart request
    execute_restart_command(message->chat->id, m_telegramThread.get());
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::ExecuteCallbackResend(const TgBot::User::Ptr& /*from*/, const TgBot::Message::Ptr& message,
                                              const CallBackParams& params, bool /*gotAnswer*/)
{
    const auto errorIdIt = params.find(resend::kParamId);
    if (errorIdIt == params.end())
        throw std::runtime_error(std::narrow(L"Нет необходимого параметра у колбэка пересылки сообщения.").c_str());

    // extract the guide from the string
    GUID errorGUID;
    if (FAILED(CLSIDFromString(CComBSTR(errorIdIt->second.c_str()), &errorGUID)))
        EXT_ASSERT(false) << L"Не удалось получить гуид!";

    const auto errorIt = std::find_if(m_monitoringErrors.begin(), m_monitoringErrors.end(),
                                      [&errorGUID](const ErrorInfo& errorInfo)
                                      {
                                          return IsEqualGUID(errorGUID, errorInfo.errorGUID);
                                      });
    if (errorIt == m_monitoringErrors.end())
    {
        std::wstring text = std::string_swprintf(
            L"Пересылаемой ошибки нет в списке, возможно ошибка является устаревшей (хранятся последние %u ошибок) или программа была перезапущена.",
            g_kMaxErrorInfoCount);
        m_telegramThread->SendMessage(message->chat->id, text);
        return;
    }

    if (errorIt->bResendToOrdinaryUsers)
    {
        m_telegramThread->SendMessage(message->chat->id, L"Ошибка уже была переслана.");
        return;
    }

    // forward the error to regular users
    const auto ordinaryUsersChatList = m_telegramUsers->GetAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser);
    m_telegramThread->SendMessage(ordinaryUsersChatList, errorIt->errorText);

    errorIt->bResendToOrdinaryUsers = true;

    m_telegramThread->SendMessage(message->chat->id, L"Ошибка была успешно переслана обычным пользователям.");
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::ExecuteCallbackAlert(const TgBot::User::Ptr& /*from*/, const TgBot::Message::Ptr& message,
                                             const CallBackParams& params, bool /*gotAnswer*/)
{
    // Callback format kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
    const auto enableParam = params.find(alertEnabling::kParamEnable);
    const auto channelParam = params.find(alertEnabling::kParamChan);

    if (enableParam == params.end() || channelParam == params.end())
        throw std::runtime_error(std::narrow(L"Нет необходимого параметра у колбэка управления оповещениями.").c_str());
    // enable/disable alerts
    const bool bEnableAlert = enableParam->second == "true";
    // message in response to the user
    std::wstring messageText;
    if (channelParam->second == alertEnabling::kValueAllChannels)
    {
        // set alerts for all channels
        size_t channelsCount = m_trendMonitoring->GetNumberOfMonitoringChannels();
        if (channelsCount == 0)
            throw std::runtime_error(std::narrow(L"Нет выбранных для мониторинга каналов, обратитесь к администратору").c_str());

        for (size_t channelInd = 0; channelInd < channelsCount; ++channelInd)
        {
            m_trendMonitoring->ChangeMonitoringChannelNotify(channelInd, bEnableAlert);
        }

        messageText = std::string_swprintf(L"Оповещения для всех каналов %s", bEnableAlert ? L"включены" : L"выключены");
    }
    else
    {
        // get a list of channels
        std::list<std::wstring> monitoringChannels = m_trendMonitoring->GetNamesOfMonitoringChannels();
        if (monitoringChannels.empty())
            throw std::runtime_error(std::narrow(L"Нет выбранных для мониторинга каналов, обратитесь к администратору").c_str());

        // channel name from callback
        const std::wstring callBackChannel = getUNICODEString(channelParam->second).c_str();
        // we assume that channels by name are not repeated in the monitoring list otherwise it's stupid
        const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
                                            [&callBackChannel](const auto& channelName)
                                            {
                                                return callBackChannel == channelName;
                                            });

        if (channelIt == monitoringChannels.cend())
            throw std::runtime_error(std::narrow(L"В данный момент в списке мониторинга нет выбранного вами канала.").c_str());

        m_trendMonitoring->ChangeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt),
                                                         bEnableAlert);

        messageText = std::string_swprintf(L"Оповещения для канала %s %s", callBackChannel.c_str(), bEnableAlert ? L"включены" : L"выключены");
    }

    EXT_ASSERT(!messageText.empty()) << L"Сообщение пользователю пустое.";
    m_telegramThread->SendMessage(message->chat->id, messageText);
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::ExecuteCallbackAlarmValue(const TgBot::User::Ptr& /*from*/, const TgBot::Message::Ptr& message,
                                                  const CallBackParams& params, bool gotAnswer)
{
    // Callback format kKeyWord kParamChan={'chan1'} kLevel={'5.5'}
    const auto channelParam = params.find(alarmingValue::kParamChan);

    if (channelParam == params.end())
        throw std::runtime_error(std::narrow(L"Нет необходимого параметра у колбэка управления оповещениями.").c_str());

    // get a list of channels
    const std::list<std::wstring> monitoringChannels = m_trendMonitoring->GetNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error(std::narrow(L"Нет выбранных для мониторинга каналов, обратитесь к администратору").c_str());

    const std::wstring callBackChannel = getUNICODEString(channelParam->second).c_str();
    const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
                                        [_channelName = std::wstring(callBackChannel.c_str())](const auto& channelName)
    {
        return channelName == _channelName;
    });
    if (channelIt == monitoringChannels.cend())
        throw std::runtime_error(std::narrow(L"Выбранный канал отсутствует в списке наблюдаемых каналов.").c_str());

    auto getNewLevelFromText = [](const std::string& text)
    {
        float newLevel = NAN;
        if (text != "NAN")
        {
            std::istringstream str(text);
            str >> newLevel;
            if (str.fail())
                throw std::runtime_error(std::narrow(L"Не удалось преобразовать переданное значение в число.").c_str());
        }

        return newLevel;
    };

    const auto newLevelParam = params.find(alarmingValue::kParamValue);
    if (newLevelParam == params.end())
    {
        if (!gotAnswer)
        {
            m_telegramThread->SendMessage(message->chat->id, L"Для того чтобы изменить допустимый уровень значений у канала '" + callBackChannel +
                                          L"' отправьте новый уровень ответным сообщением, отправьте NAN чтобы отключить оповещения совсем.");
            return;
        }

        std::wstring messageText;
        std::wstringstream newLevelText;

        const float newLevel = getNewLevelFromText(message->text);
        if (!isfinite(newLevel))
        {
            if (!m_trendMonitoring->GetMonitoringChannelData(std::distance(monitoringChannels.cbegin(), channelIt)).bNotify)
            {
                messageText = std::string_swprintf(L"Оповещения у канала '%s' уже выключены.", callBackChannel.c_str());
                m_telegramThread->SendMessage(message->chat->id, messageText);
                return;
            }

            newLevelText << L"NAN";
            messageText = std::string_swprintf(L"Отключить оповещения для канала '%s'?", callBackChannel.c_str());
        }
        else
        {
            newLevelText << newLevel;
            messageText = std::string_swprintf(L"Установить значение оповещений для канала '%s' как %s?", callBackChannel.c_str(), newLevelText.str().c_str());
        }

        // form the operation confirmation callback
        KeyboardCallback acceptCallBack(alarmingValue::kKeyWord);
        acceptCallBack.AddCallbackParam(alarmingValue::kParamChan, callBackChannel);
        acceptCallBack.AddCallbackParam(alarmingValue::kParamValue, newLevelText.str());

        TgBot::InlineKeyboardMarkup::Ptr acceptingOperationKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        acceptingOperationKeyboard->inlineKeyboard = {{ create_keyboard_button(L"Установить", acceptCallBack) }};

        m_telegramThread->SendMessage(message->chat->id, messageText, false, 0, acceptingOperationKeyboard);
    }
    else
    {
        const float newLevel = getNewLevelFromText(newLevelParam->second);

        std::wstring messageText;
        if (!isfinite(newLevel))
        {
            messageText = std::string_swprintf(L"Оповещения для канала '%s' выключены", callBackChannel.c_str());
            m_trendMonitoring->ChangeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt), false);
        }
        else
        {
            std::wstringstream newLevelText;
            newLevelText << newLevel;

            messageText = std::string_swprintf(L"Значение %s установлено для канала '%s' успешно", newLevelText.str().c_str(), callBackChannel.c_str());
            m_trendMonitoring->ChangeMonitoringChannelAlarmingValue(std::distance(monitoringChannels.cbegin(), channelIt), newLevel);
        }

        m_telegramThread->SendMessage(message->chat->id, messageText);
    }
}

} // namespace telegram::callback
