#include "pch.h"

#include <regex>

#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/std_pair.hpp>

#include "Messages.h"
#include "TelegramBot.h"

// ����� ������� ���� ��������� ���� ��� �����������
const CString gBotPassword_User   = L"MonitoringAuth";      // ����������� �������������
const CString gBotPassword_Admin  = L"MonitoringAuthAdmin"; // ����������� �������

// ��������� ��� ������� ������
// kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(�����������) kParamInterval={'1000000'}
namespace reportCallBack
{
    const std::string kKeyWord          = R"(/report)";     // �������� �����
    // ���������
    const std::string kParamType        = "reportType";     // ��� ������
    const std::string kParamChan        = "chan";           // ����� �� �������� ����� �����
    const std::string kParamInterval    = "interval";       // ��������
};

// ��������� ��� ������� �������� �������
namespace restartCallBack
{
    const std::string kKeyWord          = R"(/restart)";    // �������� ������
};

// ��������� ��� ������� ��������� ����������
// /resend errorId={'GUID'}
namespace resendCallBack
{
    const std::string kKeyWord          = R"(/resend)";     // �������� �����
    // ���������
    const std::string kParamid          = "errorId";        // ������������� ������ � ������ ������(m_monitoringErrors)
};

// ����� �������
namespace
{
    //------------------------------------------------------------------------//
    // ������� ��������� ������� � ���������
    auto createKeyboardButton(const CString& text, const KeyboardCallback& callback)
    {
        TgBot::InlineKeyboardButton::Ptr channelButton(new TgBot::InlineKeyboardButton);

        // ����� ������ ���� � UTF-8
        channelButton->text = getUtf8Str(text);
        channelButton->callbackData = callback.buildReport();

        return channelButton;
    }
};

////////////////////////////////////////////////////////////////////////////////
// ���������� ����
CTelegramBot::CTelegramBot()
{
    // ������������� �� ������� � ���������� �����������
    EventRecipientImpl::subscribe(onCompletingMonitoringTask);
    // ������������� �� ������� � ����������� ������� � �������� ������� ����� ������
    EventRecipientImpl::subscribe(onMonitoringErrorEvent);
}

//----------------------------------------------------------------------------//
CTelegramBot::~CTelegramBot()
{
    if (m_telegramThread)
        m_telegramThread->stopTelegramThread();
}

//----------------------------------------------------------------------------//
void CTelegramBot::onAllertFromTelegram(const CString& allertMessage)
{
    send_message_to_log(LogMessageData::MessageType::eError, allertMessage);
}

//----------------------------------------------------------------------------//
void CTelegramBot::initBot(ITelegramUsersListPtr telegramUsers)
{
    m_telegramUsers = telegramUsers;
}

//----------------------------------------------------------------------------//
void CTelegramBot::setBotSettings(const TelegramBotSettings& botSettings)
{
    if (m_telegramThread)
    {
        m_telegramThread->stopTelegramThread();
        // ���� �� ������� � ���� ����� �� �������� �� �������� ����������
        // TODO ���������� �� BoostHttpOnlySslClient � dll
        //m_telegramThread.reset();
    }

    m_botSettings = botSettings;

    if (!m_botSettings.bEnable || m_botSettings.sToken.IsEmpty())
        return;

    // ��������� ����� �����������
    {
        ITelegramThreadPtr pTelegramThread(
            CreateTelegramThread(std::string(CStringA(m_botSettings.sToken)),
                                 static_cast<ITelegramAllerter*>(this)));
        m_telegramThread.swap(pTelegramThread);
    }

    // �������� ������ � ������� ����������� ��� ������ �������
    std::map<std::string, CommandFunction> commandsList;
    // ������� ����������� ��� ��������� ������ ���������
    CommandFunction onUnknownCommand;
    CommandFunction onNonCommandMessage;
    fillCommandHandlers(commandsList, onUnknownCommand, onNonCommandMessage);

    m_telegramThread->startTelegramThread(commandsList, onUnknownCommand, onNonCommandMessage);
}

//----------------------------------------------------------------------------//
void CTelegramBot::sendMessageToAdmins(const CString& message)
{
    if (!m_telegramThread)
        return;

    m_telegramThread->sendMessage(
        m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin),
        message);
}

//----------------------------------------------------------------------------//
void CTelegramBot::sendMessageToUsers(const CString& message)
{
    if (!m_telegramThread)
        return;

    m_telegramThread->sendMessage(
        m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser),
        message);
}

//----------------------------------------------------------------------------//
// IEventRecipient
void CTelegramBot::onEvent(const EventId& code, float eventValue,
                           std::shared_ptr<IEventData> eventData)
{
    if (code == onCompletingMonitoringTask)
    {
        MonitoringResult::Ptr monitoringResult =
            std::static_pointer_cast<MonitoringResult>(eventData);

        // ��������� ��� ��� ���� �������
        auto it = m_monitoringTasksInfo.find(monitoringResult->m_taskId);
        if (it == m_monitoringTasksInfo.end())
            return;

        // ���� ��� ����� ��������� �����
        bool bDetailedInfo = it->second.userStatus == ITelegramUsersList::UserStatus::eAdmin;
        // ���������� ������������� ����
        int64_t chatId = it->second.chatId;

        // ������� ������� �� ������
        m_monitoringTasksInfo.erase(it);

        // ��������� �����
        CString reportText;
        reportText.Format(L"����� �� %s - %s\n\n",
                          monitoringResult->m_taskResults.front().pTaskParameters->startTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString(),
                          monitoringResult->m_taskResults.front().pTaskParameters->endTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString());

        // ��������� ����� �� ���� �������
        for (auto& channelResData : monitoringResult->m_taskResults)
        {
            // ���������� ���������� ��������� ������ - ������� ����� �� ���������
            CTimeSpan permissibleEmptyDataTime =
                (channelResData.pTaskParameters->endTime - channelResData.pTaskParameters->startTime).GetTotalSeconds() / 30;

            reportText.AppendFormat(L"����� \"%s\":", channelResData.pTaskParameters->channelName);

            switch (channelResData.resultType)
            {
            case MonitoringResult::Result::eSucceeded:  // ������ ������� ��������
                {
                    if (bDetailedInfo)
                    {
                        // ��������, ��������� ������� ���������� ����������. �� ��������� - NAN
                        float allarmingValue = NAN;

                        // ���� ����� ��������� - ���� ����� �������������� �������� � ������
                        auto pMonitoringService = get_monitoing_service();
                        for (size_t i = 0, count = pMonitoringService->getNumberOfMonitoringChannels();
                             i < count; ++i)
                        {
                            const MonitoringChannelData& channelData = pMonitoringService->getMonitoringChannelData(i);
                            if (channelData.channelName == channelResData.pTaskParameters->channelName)
                            {
                                allarmingValue = channelData.allarmingValue;
                                break;
                            }
                        }

                        // ���� �� ����� �������� ��� ������� ����� ��������� - ��������� ���������� ����� ��������
                        if (_finite(allarmingValue) != 0)
                        {
                            // ���� �� �������� ���� �� �������� ����� �� ����������
                            if ((allarmingValue >= 0 && channelResData.maxValue >= allarmingValue) ||
                                (allarmingValue < 0 && channelResData.minValue <= allarmingValue))
                                reportText.AppendFormat(L"���������� ������� %.02f ��� ��������, ",
                                                        allarmingValue);
                        }
                    }

                    reportText.AppendFormat(L"�������� �� �������� [%.02f..%.02f], ��������� ��������� - %.02f.",
                                            channelResData.minValue, channelResData.maxValue,
                                            channelResData.currentValue);

                    // ���� ����� ��������� ������
                    if (bDetailedInfo && channelResData.emptyDataTime > permissibleEmptyDataTime)
                        reportText.AppendFormat(L" ����� ��������� ������ (%s).",
                                                time_span_to_string(channelResData.emptyDataTime).GetString());
                }
                break;
            case MonitoringResult::Result::eNoData:     // � ���������� ��������� ��� ������
            case MonitoringResult::Result::eErrorText:  // �������� ������
                {
                    // ��������� � ��������� ������
                    if (!channelResData.errorText.IsEmpty())
                        reportText.Append(channelResData.errorText);
                    else
                        reportText.Append(L"��� ������ � ����������� ���������.");
                }
                break;
            default:
                assert(!"�� ��������� ��� ����������");
                break;
            }

            reportText += L"\n";
        }

        // ���������� ������� ����� ������
        m_telegramThread->sendMessage(chatId, reportText);
    }
    else if (code == onMonitoringErrorEvent)
    {
        std::shared_ptr<MessageTextData> errorData = std::static_pointer_cast<MessageTextData>(eventData);
        assert(!errorData->messageText.IsEmpty());

        // �������� ������ ��������� �����
        auto adminsChats = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin);
        if (adminsChats.empty() || errorData->messageText.IsEmpty())
            return;

        // ��������� ������ �������� ��� ���� ������
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

        // ������ �� ���������� �����������
        // kKeyWord
        auto buttonRestart = createKeyboardButton(L"������������� �������",
                                                  KeyboardCallback(restartCallBack::kKeyWord));

        // ��������� ����� ������ � ������ � ���������� � �������������
        GUID errorGUID = m_monitoringErrors.emplace_back(errorData->messageText).errorGUID;

        if (m_monitoringErrors.size() > kMaxErrorInfoCount)
            m_monitoringErrors.pop_front();

        // ������ �� �������� ����� ��������� ������� �������������
        // kKeyWord kParamid={'GUID'}
        auto buttonResendToOrdinary = createKeyboardButton(L"���������� ������� �������������",
                                                           KeyboardCallback(resendCallBack::kKeyWord).
                                                           addCallbackParam(resendCallBack::kParamid, CString(CComBSTR(errorGUID))));

        keyboard->inlineKeyboard.push_back({ buttonRestart, buttonResendToOrdinary });

        // ���������� ���� ������� ����� ������ � ���������� ��� ������� �������
        for (const int64_t chatId : adminsChats)
        {
            m_telegramThread->sendMessage(chatId, errorData->messageText, false, 0, keyboard);
        }


    }
    else
        assert(!"����������� �������");
}

//----------------------------------------------------------------------------//
void CTelegramBot::fillCommandHandlers(std::map<std::string, CommandFunction>& commandsList,
                                       CommandFunction& onUnknownCommand,
                                       CommandFunction& onNonCommandMessage)
{
    m_botCommands.clear();
    // ����������� ������� � ������
    auto addCommand = [&commandsList, &botCommands = m_botCommands]
        (const CString& command, const CString& descr, const CommandFunction& funct)
    {
        botCommands .try_emplace(command, descr);
        commandsList.try_emplace(getUtf8Str(command), funct);
    };

    // TODO C++20 ������� ����� ��������� ������

    // !��� ������� ����� ��������� � �������� ������ ����� ����� �� ��������� �������������
    addCommand(L"info", L"�������� ������ ����.",
               [this](const auto param)
               {
                   get_service<CMassages>().call([this, &param]() { this->onCommandInfo(param); });
               });
    addCommand(L"report", L"������������ �����.",
               [this](const auto param)
               {
                   get_service<CMassages>().call([this, &param]() { this->onCommandReport(param); });
               });

    // ������� ����������� ��� ��������� ������ ���������
    onUnknownCommand = onNonCommandMessage =
        [this](const auto param)
        {
            get_service<CMassages>().call([this, &param]() { this->onNonCommandMessage(param); });
        };
    // ��������� �������� �� ������� ����������
    m_telegramThread->getBotEvents().onCallbackQuery(
        [this](const auto param)
        {
            get_service<CMassages>().call([this, &param]() { this->onCallbackQuery(param); });
        });
}

//----------------------------------------------------------------------------//
CString CTelegramBot::fillCommandListAndHint()
{
    CString message = L"�������������� ������� ����:\n\n\n";

    for (auto& command : m_botCommands)
    {
        message += L"/" + command.first + L" - " + command.second + L"\n";
    }

    return message + L"\n\n��� ���� ����� �� ������������ ���������� �������� �� ����� ����(����������� ������������ ���� ����� ������� �������(/)!). ��� ������ � ���� ����, ��� ������ ��������������.";
}

//----------------------------------------------------------------------------//
bool CTelegramBot::needAnswerOnMessage(const MessagePtr message, CString& messageToUser)
{
    // ������������ ����������� ���������
    TgBot::User::Ptr pUser = message->from;

    // ���������� ��� ���� ����� ������������
    m_telegramUsers->ensureExist(pUser, message->chat->id);

    if (m_telegramUsers->getUserStatus(pUser) != ITelegramUsersList::UserStatus::eNotAuthorized)
        return true;
    else
    {
        messageToUser = L"��� ������ ���������� ��������������, ������� ������ �������� ����������.";
        return false;
    }
}

//----------------------------------------------------------------------------//
void CTelegramBot::onNonCommandMessage(const MessagePtr commandMessage)
{
    // ������������ ����������� ���������
    TgBot::User::Ptr pUser = commandMessage->from;
    // ����� ��������� ���������� ���������
    CString messageText = getUNICODEString(commandMessage->text.c_str()).Trim();

    // ���������� ��� ���� ����� ������������
    m_telegramUsers->ensureExist(pUser, commandMessage->chat->id);

    // ��������� ������� ����� ���������� ������������ � �����
    CString messageToUser;

    if (messageText.CompareNoCase(gBotPassword_User) == 0)
    {
        // ���� ������ ��� �������� ������������
        switch (m_telegramUsers->getUserStatus(pUser))
        {
        case ITelegramUsersList::UserStatus::eAdmin:
            messageToUser = L"������������ �������� ��������������� �������. ����������� �� ���������.";
            break;
        case ITelegramUsersList::UserStatus::eOrdinaryUser:
            messageToUser = L"������������ ��� �����������.";
            break;
        default:
            assert(!"�� ��������� ��� ������������.");
            [[fallthrough]];
        case ITelegramUsersList::UserStatus::eNotAuthorized:
            m_telegramUsers->setUserStatus(pUser, ITelegramUsersList::UserStatus::eOrdinaryUser);
            messageToUser = L"������������ ������� �����������.";
            break;
        }
    }
    else if (messageText.CompareNoCase(gBotPassword_Admin) == 0)
    {
        // ���� ������ ��� ��������������
        switch (m_telegramUsers->getUserStatus(pUser))
        {
        case ITelegramUsersList::UserStatus::eAdmin:
            messageToUser = L"������������ ��� ����������� ��� �������������.";
            break;
        default:
            assert(!"�� ��������� ��� ������������.");
            [[fallthrough]];
        case ITelegramUsersList::UserStatus::eOrdinaryUser:
        case ITelegramUsersList::UserStatus::eNotAuthorized:
            m_telegramUsers->setUserStatus(pUser, ITelegramUsersList::UserStatus::eAdmin);
            messageToUser = L"������������ ������� ����������� ��� �������������.";
            break;
        }
    }
    else
    {
        if (needAnswerOnMessage(commandMessage, messageToUser))
            messageToUser = fillCommandListAndHint();

        messageToUser = L"�� ��������� �������.\n" + messageToUser;
    }

    if (!messageToUser.IsEmpty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandInfo(const MessagePtr commandMessage)
{
    // ��������� ������� ����� ���������� ������������ � �����
    CString messageToUser;

    if (needAnswerOnMessage(commandMessage, messageToUser))
        messageToUser = fillCommandListAndHint();

    if (!messageToUser.IsEmpty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
    else
        assert(!"������ ���� ��������� � �����!");
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandReport(const MessagePtr commandMessage)
{
    // ��������� ������� ����� ���������� ������������ � �����
    CString messageToUser;

    if (!needAnswerOnMessage(commandMessage, messageToUser))
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
        return;
    }

    // �������� ������ �������
    std::list<CString> monitoringChannels = get_monitoing_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"������ ��� ����������� �� �������");
        return;
    }

    // ���������� ������������ ������ � ������� ������ �� �������� ����� �����
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // �������� ������ � ��������, text ���������� ��� ������ ����� ����� ������������� � UTF-8, ����� ������ �� �����
    auto addButton = [&keyboard](const CString& text, const ReportType reportType)
    {
        // ������ �� ������ ������ ������ ���� ����
        // kCallbackReport reporType={'ReportType'}
        auto button =
            createKeyboardButton(
                text,
                KeyboardCallback(reportCallBack::kKeyWord).
                    addCallbackParam(reportCallBack::kParamType, std::to_wstring((unsigned long)reportType)));

        keyboard->inlineKeyboard.push_back({ button });
    };

    // ��������� ������
    addButton(L"��� ������",         ReportType::eAllChannels);
    addButton(L"������������ �����", ReportType::eSpecialChannel);

    m_telegramThread->sendMessage(commandMessage->chat->id,
                                  L"�� ����� ������� ������������ �����?",
                                  false, 0, keyboard);
}

// ������� ��������
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

    auto value  = quoted('\'') | quoted('"');
    auto key    = lexeme[+alpha];
    auto pair   = key >> "={" >> value >> '}';
    auto parser = skip(space) [ * as<std::pair<std::string, std::string>>[pair] ];
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCallbackQuery(const TgBot::CallbackQuery::Ptr query)
{
    try
    {
        // ������ ������ � ��������� ����� ���������� �� �������
        CallBackParams reportParams;
        if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                     reportCallBack::kKeyWord >> CallbackParser::parser, reportParams))
            executeCallbackReport(query, reportParams);
        else if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                          restartCallBack::kKeyWord >> CallbackParser::parser, reportParams))
            executeCallbackRestart(query, reportParams);
        else if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                          resendCallBack::kKeyWord >> CallbackParser::parser, reportParams))
            executeCallbackResend(query, reportParams);
        else
            throw std::runtime_error("������ ������� �������");
    }
    catch (const std::exception& exc)
    {
        assert(false);

        CString errorStr;
        // ��������� ������ ������� �������
        errorStr.Format(L"%s. ���������� � �������������� �������, ����� ������� \"%s\"",
                        CString(exc.what()).GetString(), getUNICODEString(query->data).GetString());

        // �������� ���� ���-�� ������������
        m_telegramThread->sendMessage(query->message->chat->id, errorStr);
        // ��������� �� ������
        onAllertFromTelegram(errorStr);
    }
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackReport(const TgBot::CallbackQuery::Ptr query,
                                         const CallBackParams& reportParams)
{
    // � ������� ������ ������ ���� ������������ ���������, �������� ������ ������ ���� ����
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(�����������) kParamInterval={'1000000'}

    // ��� ������
    auto reportTypeIt = reportParams.find(reportCallBack::kParamType);
    // ��������� ����� ��� ������� ������ � ����� ���������� �� �������
    if (reportTypeIt == reportParams.end())
        throw std::runtime_error("�� ��������� ������.");

    // �������� ������ �������
    std::list<CString> monitoringChannels = get_monitoing_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("�� ������� �������� ������ �������, ���������� ��������� �������");

    // ��� ������ �� �������� ������ �����
    auto channelIt = reportParams.find(reportCallBack::kParamChan);

    // ��� ������
    ReportType reportType = (ReportType)std::stoul(reportTypeIt->second);

    // ������ ������ ������� "kKeyWord kParamType={'ReportType'}" � ��������� ����� ���� �������� �� ������ ������ ����� �����
    switch (reportType)
    {
    default:
        assert(!"�� ��������� ��� ������.");
        [[fallthrough]];
    case ReportType::eSpecialChannel:
        {
            // ���� ����� �� ������ ���� ��� ���������
            if (channelIt == reportParams.end())
            {
                // ��������� ������
                KeyboardCallback defCallBack = KeyboardCallback(reportCallBack::kKeyWord).
                    addCallbackParam(reportTypeIt->first, reportTypeIt->second);

                // ���������� ������������ ������ � ������� ������ �� �������� ����� �����
                TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
                keyboard->inlineKeyboard.reserve(monitoringChannels.size());

                for (const auto& channel : monitoringChannels)
                {
                    auto channelButton = createKeyboardButton(channel,
                                                              KeyboardCallback(defCallBack).
                                                              addCallbackParam(reportCallBack::kParamChan, channel));

                    keyboard->inlineKeyboard.push_back({ channelButton });
                }

                m_telegramThread->sendMessage(query->message->chat->id,
                                              L"�������� �����",
                                              false, 0, keyboard);
                return;
            }
        }
        break;

    case ReportType::eAllChannels:
        break;

    }

    auto timeIntervalIt = reportParams.find(reportCallBack::kParamInterval);
    // ��������� ��� kParamInterval �����
    if (timeIntervalIt == reportParams.end())
    {
        // ��������� ������
        KeyboardCallback defCallBack(reportCallBack::kKeyWord);
        defCallBack.addCallbackParam(reportTypeIt->first, reportTypeIt->second);

        // ���� ������� ��� - ��������� ���
        if (channelIt != reportParams.end())
            defCallBack.addCallbackParam(channelIt->first, channelIt->second);

        // ������ ������������ ������ ��������
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        keyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);

        // ��������� ������ �� ����� �����������
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            auto intervalButton = createKeyboardButton(
                monitoring_interval_to_string(MonitoringInterval(i)),
                KeyboardCallback(defCallBack).
                addCallbackParam(reportCallBack::kParamInterval, std::to_wstring(i)));

            keyboard->inlineKeyboard[i] = { intervalButton };
        }

        m_telegramThread->sendMessage(query->message->chat->id,
                                      L"�������� �������� ������� �� ������� ����� �������� �����",
                                      false, 0, keyboard);
    }
    else
    {
        // �������� ��� ����������� ���������, ��������� ������� �����������
        // ������ ������� ��� �����������
        std::list<CString> channelsForTask;

        switch (reportType)
        {
        default:
            assert(!"�� ��������� ��� ������.");
            [[fallthrough]];
        case ReportType::eAllChannels:
            channelsForTask = std::move(monitoringChannels);
            break;

        case ReportType::eSpecialChannel:
            if (channelIt == reportParams.end())
                throw std::runtime_error("�� ������� ���������� ��� ������, ���������� ��������� �������");

            channelsForTask.push_back(std::move(getUNICODEString(channelIt->second)));
            break;
        }

        CTime stopTime = CTime::GetCurrentTime();
        CTime startTime = stopTime -
            monitoring_interval_to_timespan((MonitoringInterval)std::stoi(timeIntervalIt->second));

        TaskInfo taskInfo;
        taskInfo.chatId = query->message->chat->id;
        taskInfo.userStatus = m_telegramUsers->getUserStatus(query->from);

        // �������� ������������ ��� ������ �����������, ����������� ����� �����
        // ������������ ����� ���������� ��� ������ �� ����������
        m_telegramThread->sendMessage(query->message->chat->id,
                                      L"����������� ������ ������, ��� ����� ������ ��������� �����.");

        m_monitoringTasksInfo.try_emplace(
            get_monitoing_tasks_service()->addTaskList(channelsForTask, startTime, stopTime,
                                                       IMonitoringTasksService::eHigh),
            std::move(taskInfo));
    }
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackRestart(const TgBot::CallbackQuery::Ptr query,
                                          const CallBackParams& params)
{
    assert(params.empty() && "���������� �� �������������");


}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackResend(const TgBot::CallbackQuery::Ptr query,
                                         const CallBackParams& params)
{
    auto errorIdIt = params.find(resendCallBack::kParamid);
    if (errorIdIt == params.end())
        throw std::runtime_error("��� ������������ ��������� � ������� ��������� ���������.");

    // ����������� ���� �� ������
    GUID errorGUID;
    CLSIDFromString(CComBSTR(errorIdIt->second.c_str()), &errorGUID);

    auto errorIt = std::find_if(m_monitoringErrors.begin(), m_monitoringErrors.end(),
                                [&errorGUID](const ErrorInfo& errorInfo)
                                {
                                    return IsEqualGUID(errorGUID, errorInfo.errorGUID);
                                });
    if (errorIt == m_monitoringErrors.end())
    {
        CString text;
        text.Format(L"������������ ������ ��� � ������, �������� ������ �������� ���������� (�������� ��������� %ud ������) ��� ��������� ���� ������������.",
                    kMaxErrorInfoCount);
        m_telegramThread->sendMessage(query->message->chat->id, text);
        return;
    }

    if (errorIt->bResendToOrdinaryUsers)
    {
        m_telegramThread->sendMessage(query->message->chat->id,
                                      L"������ ��� ���� ���������.");
        return;
    }

    // ���������� ������ ������� �������������
    auto ordinaryUsersChatList = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser);
    for (const auto channelId : ordinaryUsersChatList)
    {
        m_telegramThread->sendMessage(channelId, errorIt->errorText);
    }

    errorIt->bResendToOrdinaryUsers = true;
}

////////////////////////////////////////////////////////////////////////////////
// ������������ �������
KeyboardCallback::KeyboardCallback(const std::string& keyWord)
    : m_reportStr(keyWord.c_str())
{}

//----------------------------------------------------------------------------//
KeyboardCallback::KeyboardCallback(const KeyboardCallback& reportCallback)
    : m_reportStr(reportCallback.m_reportStr)
{}

//----------------------------------------------------------------------------//
KeyboardCallback& KeyboardCallback::addCallbackParam(const std::string& param,
                                                     const CString& value)
{
    // ������ ������ ���� ����
    // /%COMMAND% %PARAM_1%={'%VALUE_1%'} %PARAM_2%={'%VALUE_2%'} ...
    // ��������� ���� %PARAM_N%={'%VALUE_N'}
    m_reportStr.AppendFormat(L" %s={\'%s\'}",
                             getUNICODEString(param).GetString(), value.GetString());

    return *this;
}

//----------------------------------------------------------------------------//
KeyboardCallback& KeyboardCallback::addCallbackParam(const std::string& param,
                                                     const std::wstring& value)
{
    return addCallbackParam(param, CString(value.c_str()));
}

//----------------------------------------------------------------------------//
KeyboardCallback& KeyboardCallback::addCallbackParam(const std::string& param,
                                                     const std::string& value)
{
    return addCallbackParam(param, getUNICODEString(value));
}

//----------------------------------------------------------------------------//
std::string KeyboardCallback::buildReport() const
{
    // ������ ������������ ��������, ���� �� ������������ - �������� ��������
    const std::wregex escapedCharacters(LR"(['])");
    const std::wstring rep(LR"(\$&)");

    // ��-�� ���� ��� TgTypeParser::appendToJson ��� �� ��������� '}'
    // ���� ��������� �������� �������� '}' ��������� � ����� ������
    CString resultReport = std::regex_replace((m_reportStr + " ").GetString(),
                                              escapedCharacters,
                                              rep).c_str();

    // ������������ ����������� �� ������ ������� ��������� 65 ��������
    constexpr int maxReportSize = 65;
    assert(resultReport.GetLength() <= maxReportSize);

    return getUtf8Str(resultReport);
}