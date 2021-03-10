#include "pch.h"

#include <filesystem>
#include <map>
#include <regex>
#include <utility>

#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/std_pair.hpp>

#include <Messages.h>
#include <DirsService.h>

#include "TelegramBot.h"
#include "TelegramCommands.h"
#include "Utils.h"

// ����� ������� ���� ��������� ���� ��� �����������
constexpr std::wstring_view gBotPassword_User   = L"MonitoringAuth";      // ����������� �������������
constexpr std::wstring_view gBotPassword_Admin  = L"MonitoringAuthAdmin"; // ����������� �������

// ��������� ��� ������� ������
// kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(�����������) kParamInterval={'1000000'}
namespace reportCallBack
{
    const std::string kKeyWord          = R"(/report)";     // �������� �����
    // ���������
    const std::string kParamType        = "type";           // ��� ������
    const std::string kParamChan        = "chan";           // ����� �� �������� ����� �����
    const std::string kParamInterval    = "interval";       // ��������
};

// ��������� ��� ������� �������� �������
namespace restartCallBack
{
    const std::string kKeyWord          = R"(/restart)";    // �������� ������
};

// ��������� ��� ������� ��������� ����������
// kKeyWord kParamid={'GUID'}
namespace resendCallBack
{
    const std::string kKeyWord          = R"(/resend)";     // �������� �����
    // ���������
    const std::string kParamid          = "errorId";        // ������������� ������ � ������ ������(m_monitoringErrors)
};

// ��������� ��� ������� ����������
// kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
namespace alertEnablingCallBack
{
    const std::string kKeyWord          = R"(/alert)";      // �������� �����
    // ���������
    const std::string kParamEnable      = "enable";         // ��������� ������������/�������������
    const std::string kParamChan        = "chan";           // ����� �� �������� ����� ��������� �����������
    const std::string kValueAllChannels = "allChannels";    // �������� ������� ������������� ���������� ���������� �� ���� �������
};

// ��������� ��� ������� ��������� ������ ����������
// kKeyWord kParamChan={'chan1'} kValue={'0.2'}
namespace alarmingValueCallBack
{
    const std::string kKeyWord = R"(/alarmV)";              // �������� �����
    // ���������
    const std::string kParamChan = "chan";                  // ����� �� �������� ����� ��������� ������� ����������
    const std::string kValue = "val";                       // ����� �������� ������ ����������
}

// ����� �������
namespace
{
    //------------------------------------------------------------------------//
    // �������� ������ � �������� � ���������, text ���������� ��� ������ ����� ����� ������������� � UTF-8, ����� ������ �� �����
    auto createKeyboardButton(const CString& text, const KeyboardCallback& callback)
    {
        TgBot::InlineKeyboardButton::Ptr channelButton(new TgBot::InlineKeyboardButton);

        // ����� ������ ���� � UTF-8
        channelButton->text = getUtf8Str(text.GetString());
        channelButton->callbackData = callback.buildReport();

        return channelButton;
    }
};

////////////////////////////////////////////////////////////////////////////////
// ���������� ����
CTelegramBot::CTelegramBot()
{
    m_commandHelper = std::make_shared<CommandsHelper>();

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
        ITelegramThreadPtr pTelegramThread;
        if (m_defaultTelegramThread)
            pTelegramThread.swap(m_defaultTelegramThread);
        else
            pTelegramThread = CreateTelegramThread(std::string(CStringA(m_botSettings.sToken)),
                                                   [](const std::wstring& alertMessage)
                                                   {
                                                       send_message_to_log(LogMessageData::MessageType::eError, CString(alertMessage.c_str()));
                                                   });

        m_telegramThread.swap(pTelegramThread);
    }

    // �������� ������ � ������� ����������� ��� ������ �������
    std::unordered_map<std::string, CommandFunction> commandsList;
    // ������� ����������� ��� ��������� ������ ���������
    CommandFunction onUnknownCommand;
    CommandFunction onNonCommandMessage;
    fillCommandHandlers(commandsList, onUnknownCommand, onNonCommandMessage);

    m_telegramThread->startTelegramThread(commandsList, onUnknownCommand, onNonCommandMessage);
}

//----------------------------------------------------------------------------//
void CTelegramBot::setDefaultTelegramThread(ITelegramThreadPtr& pTelegramThread)
{
    m_defaultTelegramThread.swap(pTelegramThread);
}

//----------------------------------------------------------------------------//
void CTelegramBot::sendMessageToAdmins(const CString& message) const
{
    if (!m_telegramThread)
        return;

    m_telegramThread->sendMessage(
        m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin),
        message.GetString());
}

//----------------------------------------------------------------------------//
void CTelegramBot::sendMessageToUsers(const CString& message) const
{
    if (!m_telegramThread)
        return;

    m_telegramThread->sendMessage(
        m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser),
        message.GetString());
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
        const bool bDetailedInfo = it->second.userStatus == ITelegramUsersList::UserStatus::eAdmin;
        // ���������� ������������� ����
        const int64_t chatId = it->second.chatId;

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

            reportText.AppendFormat(L"����� \"%s\":", channelResData.pTaskParameters->channelName.GetString());

            switch (channelResData.resultType)
            {
            case MonitoringResult::Result::eSucceeded:  // ������ ������� ��������
                {
                    if (bDetailedInfo)
                    {
                        // ��������, ��������� ������� ���������� ����������. �� ��������� - NAN
                        float alarmingValue = NAN;

                        // ���� ����� ��������� - ���� ����� �������������� �������� � ������
                        auto pMonitoringService = get_monitoring_service();
                        for (size_t i = 0, count = pMonitoringService->getNumberOfMonitoringChannels();
                             i < count; ++i)
                        {
                            const MonitoringChannelData& channelData = pMonitoringService->getMonitoringChannelData(i);
                            if (channelData.channelName == channelResData.pTaskParameters->channelName)
                            {
                                alarmingValue = channelData.alarmingValue;
                                break;
                            }
                        }

                        // ���� �� ����� �������� ��� ������� ����� ��������� - ��������� ���������� ����� ��������
                        if (isfinite(alarmingValue))
                        {
                            // ���� �� �������� ���� �� �������� ����� �� ����������
                            if ((alarmingValue >= 0 && channelResData.maxValue >= alarmingValue) ||
                                (alarmingValue < 0 && channelResData.minValue <= alarmingValue))
                                reportText.AppendFormat(L"���������� ������� %.02f ��� ��������, ",
                                                        alarmingValue);
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
        m_telegramThread->sendMessage(chatId, reportText.GetString());
    }
    else if (code == onMonitoringErrorEvent)
    {
        std::shared_ptr<MonitoringErrorEventData> errorData =
            std::static_pointer_cast<MonitoringErrorEventData>(eventData);
        assert(!errorData->errorText.IsEmpty());

        // �������� ������ ��������� �����
        auto adminsChats = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin);
        if (adminsChats.empty() || errorData->errorText.IsEmpty())
            return;

        // ��������� ����� ������ � ������ � ���������� � �������������
        m_monitoringErrors.emplace_back(errorData.get());

        if (m_monitoringErrors.size() > kMaxErrorInfoCount)
            m_monitoringErrors.pop_front();

        // ��������� ������ �������� ��� ���� ������
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

        // ������ �� ���������� �����������
        // kKeyWord
        KeyboardCallback callbackRestart(restartCallBack::kKeyWord);

        // ������ �� �������� ����� ��������� ������� �������������
        // kKeyWord kParamid={'GUID'}
        KeyboardCallback callBackOrdinaryUsers(resendCallBack::kKeyWord);
        callBackOrdinaryUsers.addCallbackParam(resendCallBack::kParamid, CString(CComBSTR(errorData->errorGUID)));

        keyboard->inlineKeyboard.push_back({ createKeyboardButton(L"������������� �������", callbackRestart),
                                             createKeyboardButton(L"���������� ������� �������������", callBackOrdinaryUsers) });

        // ���������� ���� ������� ����� ������ � ���������� ��� ������� �������
        m_telegramThread->sendMessage(adminsChats, errorData->errorText.GetString(), false, 0, keyboard);
    }
    else
        assert(!"����������� �������");
}

//----------------------------------------------------------------------------//
void CTelegramBot::fillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                                       CommandFunction& onUnknownCommand,
                                       CommandFunction& onNonCommandMessage)
{
    m_commandHelper = std::make_shared<CommandsHelper>();

    // ������ ������������� ������� �������� ��� ������� �� ���������
    const std::vector<ITelegramUsersList::UserStatus> kDefaultAvailability =
    {   ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };

    static_assert(ITelegramUsersList::UserStatus::eLastStatus == 3,
                  "������ ����������� ���������, �������� ����� ������������ ����������� ������");

    // ����������� ������� � ������
    auto addCommand = [&](const Command command, const std::wstring& commandText, const std::wstring& descr,
                          const std::vector<ITelegramUsersList::UserStatus>& availabilityStatuses)
    {
        m_commandHelper->addCommand(command, commandText,
                                    descr, availabilityStatuses);

        if (!commandsList.try_emplace(getUtf8Str(commandText),
                                      [this, command](const auto message)
                                      {
                                          this->onCommandMessage(command, message);
                                      }).second)
            assert(!"������� ������ ���� �����������!");
    };

    static_assert(Command::eLastCommand == (Command)7,
                  "���������� ������ ����������, ���� �������� ���������� � ����!");

    // !��� ������� ����� ��������� � �������� ������ ����� ����� �� ��������� �������������
    addCommand(Command::eInfo,    L"info",    L"�������� ������ ����.", kDefaultAvailability);
    addCommand(Command::eReport,  L"report",  L"������������ �����.",   kDefaultAvailability);
    addCommand(Command::eRestart, L"restart", L"������������� ������� �����������.",
               { ITelegramUsersList::eAdmin });
    addCommand(Command::eAlertingOn,  L"alertingOn",  L"�������� ���������� � ��������.",
               { ITelegramUsersList::eAdmin });
    addCommand(Command::eAlertingOff, L"alertingOff", L"��������� ���������� � ��������.",
               { ITelegramUsersList::eAdmin });
    addCommand(Command::eAlarmingValue, L"alarmingValue", L"�������� ���������� ������� �������� � ������.",
               { ITelegramUsersList::eAdmin });

    auto addCommandCallback = [commandCallbacks = &m_commandCallbacks]
    (const std::string& keyWord, const CommandCallback& callback)
    {
        if (!commandCallbacks->try_emplace(keyWord, callback).second)
            assert(!"�������� ����� � �������� ������ ����������!");
    };
    addCommandCallback(reportCallBack::       kKeyWord, &CTelegramBot::executeCallbackReport);
    addCommandCallback(restartCallBack::      kKeyWord, &CTelegramBot::executeCallbackRestart);
    addCommandCallback(resendCallBack::       kKeyWord, &CTelegramBot::executeCallbackResend);
    addCommandCallback(alertEnablingCallBack::kKeyWord, &CTelegramBot::executeCallbackAlert);
    addCommandCallback(alarmingValueCallBack::kKeyWord, &CTelegramBot::executeCallbackAlarmValue);

    // ������� ����������� ��� ��������� ������ ���������
    onUnknownCommand = onNonCommandMessage =
        [this](const auto message)
    {
        get_service<CMassages>().call([this, &message]() { this->onNonCommandMessage(message); });
    };
    // ��������� �������� �� ������� ����������
    m_telegramThread->getBotEvents().onCallbackQuery(
        [this](const auto param)
        {
            get_service<CMassages>().call([this, &param]() { this->onCallbackQuery(param); });
        });
}

//----------------------------------------------------------------------------//
void CTelegramBot::onNonCommandMessage(const MessagePtr& commandMessage)
{
    // ������������ ����������� ���������
    TgBot::User::Ptr pUser = commandMessage->from;
    // ����� ��������� ���������� ���������
    CString messageText = getUNICODEString(commandMessage->text).c_str();
    messageText.Trim();

    // ���������� ��� ���� ����� ������������
    m_telegramUsers->ensureExist(pUser, commandMessage->chat->id);

    // ��������� ������� ����� ���������� ������������ � �����
    std::wstring messageToUser;

    if (messageText.CompareNoCase(gBotPassword_User.data()) == 0)
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
    else if (messageText.CompareNoCase(gBotPassword_Admin.data()) == 0)
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
        if (gotResponseToPreviousCallback(commandMessage))
            return;

        // ����� ���������� �� � ���, ������ �� eUnknown ������������ ����� ������
        m_commandHelper->ensureNeedAnswerOnCommand(m_telegramUsers, Command::eUnknown,
                                                   commandMessage, messageToUser);
    }

    if (!messageToUser.empty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandMessage(const Command command, const MessagePtr& message)
{
    // �.�. ������� ����� ������ �� ������� ������ �� ����� �� ������ ��������������
    // ������������� ������������ ��� � �������� �����
    get_service<CMassages>().call(
        [this, &command, &message]()
        {
            std::wstring messageToUser;
            // ��������� ��� ���� ������������� �������� �� ������� ����� ������������
            if (m_commandHelper->ensureNeedAnswerOnCommand(m_telegramUsers, command,
                                                           message, messageToUser))
            {
                m_telegramUsers->setUserLastCommand(message->from, message->text);

                switch (command)
                {
                default:
                    assert(!"����������� �������!.");
                    [[fallthrough]];
                case Command::eInfo:
                    onCommandInfo(message);
                    break;
                case Command::eReport:
                    onCommandReport(message);
                    break;
                case Command::eRestart:
                    onCommandRestart(message);
                    break;
                case Command::eAlertingOn:
                case Command::eAlertingOff:
                    onCommandAlert(message, command == Command::eAlertingOn);
                    break;
                case Command::eAlarmingValue:
                    onCommandAlarmingValue(message);
                    break;
                }
            }
            else if (!messageToUser.empty())
                // ���� ������������ ������� �� �������� ���������� ��� ���������� �� �����
                m_telegramThread->sendMessage(message->chat->id, messageToUser);
            else
                assert(!"������ ���� ����� ��������� ������������");
        });
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandInfo(const MessagePtr& commandMessage) const
{
    ITelegramUsersList::UserStatus userStatus =
        m_telegramUsers->getUserStatus(commandMessage->from);

    std::wstring messageToUser = m_commandHelper->getAvailableCommandsWithDescr(userStatus);
    if (!messageToUser.empty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
    else
        assert(!"������ ���� ��������� � �����!");
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandReport(const MessagePtr& commandMessage) const
{
    // �������� ������ �������
    std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
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
        // kKeyWord kParamType={'ReportType'}
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

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandRestart(const MessagePtr& commandMessage) const
{
    // ���������� ������� ������ ����� ������ ��� ��� ���� ����� ����� ������� ��� ������ ������
    CString batFullPath = get_service<DirsService>().getExeDir() + kRestartSystemFileName;

    // ��������� ������������
    CString messageToUser;
    if (std::filesystem::is_regular_file(batFullPath.GetString()))
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"���������� ������� ��������������.");

        // ��������� ������
        STARTUPINFO cif = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION m_ProcInfo = { 0 };
        if (FALSE != CreateProcess(batFullPath.GetBuffer(),     // ��� ������������ ������
                                   nullptr,	                    // ��������� ������
                                   NULL,                        // ��������� �� ��������� SECURITY_ATTRIBUTES
                                   NULL,                        // ��������� �� ��������� SECURITY_ATTRIBUTES
                                   0,                           // ���� ������������ �������� ��������
                                   NULL,                        // ����� �������� �������� ��������
                                   NULL,                        // ��������� �� ���� �����
                                   NULL,                        // ������� ���� ��� �������
                                   &cif,                        // ��������� �� ��������� STARTUPINFO
                                   &m_ProcInfo))                // ��������� �� ��������� PROCESS_INFORMATION)
        {	// ������������� ������ �� �����
            CloseHandle(m_ProcInfo.hThread);
            CloseHandle(m_ProcInfo.hProcess);
        }
    }
    else
        m_telegramThread->sendMessage(commandMessage->chat->id, L"���� ��� ����������� �� ������.");
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandAlert(const MessagePtr& commandMessage, bool bEnable) const
{
    // �������� ������ �������
    std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"������ ��� ����������� �� �������");
        return;
    }

    // ���������� ������������ ������ � ������� ������ �� �������� ����� �����
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // ������ ������� ������ ���� � ������ ������
    KeyboardCallback defaultCallBack(alertEnablingCallBack::kKeyWord);
    defaultCallBack.addCallbackParam(alertEnablingCallBack::kParamEnable, std::wstring(bEnable ? L"true" : L"false"));

    // ��������� ������ ��� ������� ������
    for (const auto& channelName : monitoringChannels)
    {
        // ������ �� ������ ������ ������ ���� ����
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        KeyboardCallback channelCallBack(defaultCallBack);
        channelCallBack.addCallbackParam(alertEnablingCallBack::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ createKeyboardButton(channelName, channelCallBack) });
    }

    // ��������� ������ �� ����� ��������
    if (monitoringChannels.size() > 1)
        keyboard->inlineKeyboard.push_back({
            createKeyboardButton(L"��� ������", KeyboardCallback(defaultCallBack).
                addCallbackParam(alertEnablingCallBack::kParamChan, alertEnablingCallBack::kValueAllChannels)) });

    CString text;
    text.Format(L"�������� ����� ��� %s ����������.", bEnable ? L"���������" : L"����������");
    m_telegramThread->sendMessage(commandMessage->chat->id, text.GetString(), false, 0, keyboard);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandAlarmingValue(const MessagePtr& commandMessage) const
{
    // �������� ������ �������
    std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"������ ��� ����������� �� �������");
        return;
    }

    // ���������� ������������ ������ � ������� ������ �� �������� ����� �����
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // ��������� ������ ��� ������� ������
    for (const auto& channelName : monitoringChannels)
    {
        // ������ �� ������ ������ ������ ���� ����
        // kKeyWord kParamChan={'chan1'}
        KeyboardCallback channelCallBack(alarmingValueCallBack::kKeyWord);
        channelCallBack.addCallbackParam(alarmingValueCallBack::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ createKeyboardButton(channelName, channelCallBack) });
    }

    m_telegramThread->sendMessage(commandMessage->chat->id, L"�������� ����� ��� ��������� ������ ����������.", false, 0, keyboard);
}

////////////////////////////////////////////////////////////////////////////////
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
void CTelegramBot::onCallbackQuery(const TgBot::CallbackQuery::Ptr& query)
{
    m_telegramUsers->setUserLastCommand(query->from, query->data);

    try
    {
        // ������ ������ � ��������� ����� ���������� �� �������
        CallBackParams callBackParams;

        for (auto& [keyWord, callback] : m_commandCallbacks)
        {
            if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                         keyWord >> CallbackParser::parser, callBackParams))
            {
                (this->*callback)(query->message, callBackParams, false);
                return;
            }
        }

        throw std::runtime_error("������ ������� �������");
    }
    catch (const std::exception& exc)
    {
        assert(false);

        CString errorStr;
        // ��������� ������ ������� �������
        errorStr.Format(L"%s. ���������� � �������������� �������, ����� ������� \"%s\"",
                        CString(exc.what()).GetString(), getUNICODEString(query->data).c_str());

        // �������� ���� ���-�� ������������
        m_telegramThread->sendMessage(query->message->chat->id, errorStr.GetString());
        // ��������� �� ������
        send_message_to_log(LogMessageData::MessageType::eError, errorStr);
    }
}

//----------------------------------------------------------------------------//
bool CTelegramBot::gotResponseToPreviousCallback(const MessagePtr& commandMessage)
{
    const std::string lastBotCommand = m_telegramUsers->getUserLastCommand(commandMessage->from);

    try
    {
        // ������ ������ � ��������� ����� ���������� �� �������
        CallBackParams callBackParams;

        const std::initializer_list<std::string> callbacksWithAnswer = { alarmingValueCallBack::kKeyWord };
        for (const auto& callbackKeyWord : callbacksWithAnswer)
        {
            if (boost::spirit::x3::parse(lastBotCommand.begin(), lastBotCommand.end(),
                                         callbackKeyWord >> CallbackParser::parser, callBackParams))
            {
                if (const auto callbackIt = m_commandCallbacks.find(callbackKeyWord);
                    callbackIt != m_commandCallbacks.end())
                {
                    const CommandCallback& callback = callbackIt->second;
                    (this->*callback)(commandMessage, callBackParams, true);
                    return true;
                }
                else
                {
                    assert(false);
                    throw std::runtime_error("����������� ���������� ��� ������� " + callbackKeyWord + "!");
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        OUTPUT_LOG_FUNC;
        OUTPUT_LOG_SET_TEXT(L"����������� ��������� �� ������������: %s\n����� ���������: %s\n��������� ������� ������������: %s\n������: %s",
                            getUNICODEString(commandMessage->from->username).c_str(),
                            getUNICODEString(commandMessage->text).c_str(),
                            getUNICODEString(lastBotCommand).c_str(),
                            CString(exception.what()).GetString());
    }

    return false;
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackReport(const TgBot::Message::Ptr& message, const CallBackParams& reportParams, bool gotAnswer)
{
    // � ������� ������ ������ ���� ������������ ���������, �������� ������ ������ ���� ����
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(�����������) kParamInterval={'1000000'}

    // ��� ������
    auto reportTypeIt = reportParams.find(reportCallBack::kParamType);
    // ��������� ����� ��� ������� ������ � ����� ���������� �� �������
    if (reportTypeIt == reportParams.end())
        throw std::runtime_error("�� ��������� ������.");

    // �������� ������ �������
    std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("�� ������� �������� ������ �������, ���������� ��������� �������");

    // ��� ������ �� �������� ������ �����
    const auto channelIt = reportParams.find(reportCallBack::kParamChan);

    // ��� ������
    const ReportType reportType = (ReportType)std::stoul(reportTypeIt->second);

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
                KeyboardCallback defCallBack(reportCallBack::kKeyWord);
                defCallBack.addCallbackParam(reportTypeIt->first, reportTypeIt->second);

                // ���������� ������������ ������ � ������� ������ �� �������� ����� �����
                TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
                keyboard->inlineKeyboard.reserve(monitoringChannels.size());

                for (const auto& channel : monitoringChannels)
                {
                    const auto channelButton =
                        createKeyboardButton(channel, KeyboardCallback(defCallBack).addCallbackParam(reportCallBack::kParamChan, channel));

                    keyboard->inlineKeyboard.push_back({ channelButton });
                }

                m_telegramThread->sendMessage(message->chat->id, L"�������� �����", false, 0, keyboard);
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

        m_telegramThread->sendMessage(message->chat->id,
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

            channelsForTask.emplace_back(getUNICODEString(channelIt->second).c_str());
            break;
        }

        const CTime stopTime = CTime::GetCurrentTime();
        const CTime startTime = stopTime -
            monitoring_interval_to_timespan((MonitoringInterval)std::stoi(timeIntervalIt->second));

        TaskInfo taskInfo;
        taskInfo.chatId = message->chat->id;
        taskInfo.userStatus = m_telegramUsers->getUserStatus(message->from);

        // �������� ������������ ��� ������ �����������, ����������� ����� �����
        // ������������ ����� ���������� ��� ������ �� ����������
        m_telegramThread->sendMessage(message->chat->id,
                                      L"����������� ������ ������, ��� ����� ������ ��������� �����.");

        m_monitoringTasksInfo.try_emplace(
            get_monitoring_tasks_service()->addTaskList(channelsForTask, startTime, stopTime,
                                                       IMonitoringTasksService::eHigh),
            std::move(taskInfo));
    }
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackRestart(const TgBot::Message::Ptr& message,
                                          const CallBackParams& params,
                                          bool gotAnswer)
{
    assert(params.empty() && "���������� �� �������������");

    std::wstring messageToUser;
    if (!m_commandHelper->ensureNeedAnswerOnCommand(m_telegramUsers, Command::eRestart, message, messageToUser))
    {
        assert(!"������������ �������� ���������� ��� ���������� �� ���������� ������ ��������.");
        m_telegramThread->sendMessage(message->chat->id,
                                      L"� ��� ��� ���������� �� ���������� �������, ���������� � ��������������!");
    }
    else
        // ��������� ��� ������������ �������� ������ ��������
        onCommandRestart(message);
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackResend(const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer)
{
    auto errorIdIt = params.find(resendCallBack::kParamid);
    if (errorIdIt == params.end())
        throw std::runtime_error("��� ������������ ��������� � ������� ��������� ���������.");

    // ����������� ���� �� ������
    GUID errorGUID;
    if (FAILED(CLSIDFromString(CComBSTR(errorIdIt->second.c_str()), &errorGUID)))
        assert(!"�� ������� �������� ����!");

    auto errorIt = std::find_if(m_monitoringErrors.begin(), m_monitoringErrors.end(),
                                [&errorGUID](const ErrorInfo& errorInfo)
                                {
                                    return IsEqualGUID(errorGUID, errorInfo.errorGUID);
                                });
    if (errorIt == m_monitoringErrors.end())
    {
        CString text;
        text.Format(L"������������ ������ ��� � ������, �������� ������ �������� ���������� (�������� ��������� %u ������) ��� ��������� ���� ������������.",
                    kMaxErrorInfoCount);
        m_telegramThread->sendMessage(message->chat->id, text.GetString());
        return;
    }

    if (errorIt->bResendToOrdinaryUsers)
    {
        m_telegramThread->sendMessage(message->chat->id,
                                      L"������ ��� ���� ���������.");
        return;
    }

    // ���������� ������ ������� �������������
    const auto ordinaryUsersChatList = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser);
    m_telegramThread->sendMessage(ordinaryUsersChatList, errorIt->errorText.GetString());

    errorIt->bResendToOrdinaryUsers = true;

    m_telegramThread->sendMessage(message->chat->id,
                                  L"������ ���� ������� ��������� ������� �������������.");
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackAlert(const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer)
{
    // ������ ������� kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
    const auto enableIt = params.find(alertEnablingCallBack::kParamEnable);
    const auto channelIt = params.find(alertEnablingCallBack::kParamChan);

    if (enableIt == params.end() || channelIt == params.end())
        throw std::runtime_error("��� ������������ ��������� � ������� ���������� ������������.");
    // ������������/������������� ����������
    bool bEnableAlertion = enableIt->second == "true";
    // ������ �����������
    auto monitoringService = get_monitoring_service();
    // ��������� � ����� ������������
    CString messageText;
    if (channelIt->second == alertEnablingCallBack::kValueAllChannels)
    {
        // ����������� ���������� ��� ���� �������
        size_t channelsCount = monitoringService->getNumberOfMonitoringChannels();
        if (channelsCount == 0)
            throw std::runtime_error("��� ��������� ��� ����������� �������, ���������� � ��������������");

        for (size_t channelInd = 0; channelInd < channelsCount; ++channelInd)
        {
            monitoringService->changeMonitoringChannelNotify(channelInd, bEnableAlertion);
        }

        messageText.Format(L"���������� ��� ���� ������� %s", bEnableAlertion ? L"��������" : L"���������");
    }
    else
    {
        // �������� ������ �������
        std::list<CString> monitoringChannels = monitoringService->getNamesOfMonitoringChannels();
        if (monitoringChannels.empty())
            throw std::runtime_error("��� ��������� ��� ����������� �������, ���������� � ��������������");

        // ��� ������ �� �������
        const CString callBackChannel = getUNICODEString(channelIt->second).c_str();
        // ������� ��� � ������ ����������� ������ �� ������ �� ����������� ����� ��� �����
        const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
            [&callBackChannel](const auto& channelName)
            {
                return callBackChannel == channelName;
            });

        if (channelIt == monitoringChannels.cend())
            throw std::runtime_error("� ������ ������ � ������ ����������� ��� ���������� ���� ������.");

        monitoringService->changeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt),
                                                         bEnableAlertion);

        messageText.Format(L"���������� ��� ������ %s %s", callBackChannel.GetString(), bEnableAlertion ? L"��������" : L"���������");
    }

    assert(!messageText.IsEmpty() && "��������� ������������ ������.");
    m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackAlarmValue(const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer)
{
    // ������ ������� kKeyWord kParamChan={'chan1'} kLevel={'5.5'}
    const auto channelParam = params.find(alarmingValueCallBack::kParamChan);

    if (channelParam == params.end())
        throw std::runtime_error("��� ������������ ��������� � ������� ���������� ������������.");

    // �������� ������ �������
    const std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("��� ��������� ��� ����������� �������, ���������� � ��������������");

    const std::wstring callBackChannel = getUNICODEString(channelParam->second).c_str();
    const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
                                                             [_channelName = CString(callBackChannel.c_str())](const auto& channelName)
                                                             {
                                                                 return channelName == _channelName;
                                                             });
    if (channelIt == monitoringChannels.cend())
        throw std::runtime_error("��������� ����� ����������� � ������ ����������� �������.");

    auto getNewLevelFromText = [](const std::string& text)
    {
        float newLevel = NAN;
        if (text != "NAN")
        {
            std::istringstream str(text);
            str >> newLevel;
            if (str.fail())
                throw std::runtime_error("�� ������� ������������� ���������� �������� � �����.");
        }

        return newLevel;
    };

    const auto newLevelParam = params.find(alarmingValueCallBack::kValue);
    if (newLevelParam == params.end())
    {
        if (!gotAnswer)
        {
            m_telegramThread->sendMessage(message->chat->id, L"��� ���� ����� �������� ���������� ������� �������� � ������ '" + callBackChannel +
                                          L"' ��������� ����� ������� �������� ����������, ��������� NAN ����� ��������� ���������� ������.");
            return;
        }

        CString messageText;
        std::wstringstream newLevelText;

        const float newLevel = getNewLevelFromText(message->text);
        if (!isfinite(newLevel))
        {
            if (!get_monitoring_service()->getMonitoringChannelData(std::distance(monitoringChannels.cbegin(), channelIt)).bNotify)
            {
                messageText.Format(L"���������� � ������ '%s' ��� ���������.", callBackChannel.c_str());
                m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
                return;
            }

            newLevelText << L"NAN";
            messageText.Format(L"��������� ���������� ��� ������ '%s'?", callBackChannel.c_str());
        }
        else
        {
            newLevelText << newLevel;
            messageText.Format(L"���������� �������� ���������� ��� ������ '%s' ��� %s?", callBackChannel.c_str(), newLevelText.str().c_str());
        }

        // ��������� ������ ������������� ��������
        KeyboardCallback acceptCallBack(alarmingValueCallBack::kKeyWord);
        acceptCallBack.addCallbackParam(alarmingValueCallBack::kParamChan, callBackChannel);
        acceptCallBack.addCallbackParam(alarmingValueCallBack::kValue, newLevelText.str());

        TgBot::InlineKeyboardMarkup::Ptr acceptingOperationKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        acceptingOperationKeyboard->inlineKeyboard = {{ createKeyboardButton(L"����������", acceptCallBack) }};

        m_telegramThread->sendMessage(message->chat->id, messageText.GetString(), false, 0, acceptingOperationKeyboard);
    }
    else
    {
        const float newLevel = getNewLevelFromText(newLevelParam->second);

        CString messageText;
        if (!isfinite(newLevel))
        {
            messageText.Format(L"���������� ��� ������ '%s' ���������", callBackChannel.c_str());
            get_monitoring_service()->changeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt), false);
        }
        else
        {
            std::wstringstream newLevelText;
            newLevelText << newLevel;

            messageText.Format(L"�������� %s ����������� ��� ������ '%s' �������", newLevelText.str().c_str(), callBackChannel.c_str());
            get_monitoring_service()->changeMonitoringChannelAlarmingValue(std::distance(monitoringChannels.cbegin(), channelIt), newLevel);
        }

        m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
    }
}
