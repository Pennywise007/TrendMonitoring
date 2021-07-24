#include "pch.h"

#include <utility>

#include <Messages.h>
#include <DirsService.h>

#include "KeyboardCallback.h"
#include "TelegramBot.h"

// ����� ������� ���� ��������� ���� ��� �����������
constexpr std::wstring_view gBotPassword_User   = L"MonitoringAuth";      // ����������� �������������
constexpr std::wstring_view gBotPassword_Admin  = L"MonitoringAuthAdmin"; // ����������� �������

////////////////////////////////////////////////////////////////////////////////
// ���������� ����
CTelegramBot::CTelegramBot(const ITelegramUsersListPtr& telegramUsers, ITelegramThread* pDefaultTelegramThread)
    : m_defaultTelegramThread(pDefaultTelegramThread)
    , m_telegramUsers(telegramUsers)
    , m_callbacksHandler(m_telegramThread, m_telegramUsers)
{
}

//----------------------------------------------------------------------------//
CTelegramBot::~CTelegramBot()
{
    if (m_telegramThread)
        m_telegramThread->stopTelegramThread();
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
void CTelegramBot::fillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                                       CommandFunction& onUnknownCommand,
                                       CommandFunction& onNonCommandMessage)
{
    CommandsInfoService* commandsHelper = &get_service<CommandsInfoService>();
    commandsHelper->resetCommandList(); // ��� ������

    // ������ ������������� ������� �������� ��� ������� �� ���������
    const std::vector<ITelegramUsersList::UserStatus> kDefaultAvailability =
    {   ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };

    static_assert(ITelegramUsersList::UserStatus::eLastStatus == 3,
                  "������ ����������� ���������, �������� ����� ������������ ����������� ������");

    typedef CommandsInfoService::Command Command;

    // ����������� ������� � ������
    auto addCommand = [&](Command command, std::wstring commandText, std::wstring descr,
                          const std::vector<ITelegramUsersList::UserStatus>& availabilityStatuses,
                          std::set<std::string> callbacksKeyWords = {})
    {
        if (!commandsList.try_emplace(getUtf8Str(commandText),
                                      [this, command](const auto message)
                                      {
                                          this->onCommandMessage(command, message);
                                      }).second)
            assert(!"������� ������ ���� �����������!");

        commandsHelper->addCommand(command, std::move(commandText), std::move(descr),
                                   std::move(callbacksKeyWords), availabilityStatuses);
    };

    static_assert(Command::eLastCommand == (Command)7,
                  "���������� ������ ����������, ���� �������� ���������� � ����!");

    using namespace callback;

    // !��� ������� ����� ��������� � �������� ������ ����� ����� �� ��������� �������������
    addCommand(Command::eInfo,    L"info",    L"�������� ������ ����.", kDefaultAvailability);
    addCommand(Command::eReport,  L"report",  L"������������ �����.",   kDefaultAvailability, { report::kKeyWord });
    addCommand(Command::eRestart, L"restart", L"������������� ������� �����������.",
               { ITelegramUsersList::eAdmin }, { restart::kKeyWord, resend::kKeyWord });
    addCommand(Command::eAlertingOn,  L"alertingOn",  L"�������� ���������� � ��������.",
               { ITelegramUsersList::eAdmin }, { alertEnabling::kKeyWord });
    addCommand(Command::eAlertingOff, L"alertingOff", L"��������� ���������� � ��������.",
               { ITelegramUsersList::eAdmin }, { alertEnabling::kKeyWord });
    addCommand(Command::eAlarmingValue, L"alarmingValue", L"�������� ���������� ������� �������� � ������.",
               { ITelegramUsersList::eAdmin }, { alarmingValue::kKeyWord });

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
            get_service<CMassages>().call([this, &param]() { this->m_callbacksHandler.onCallbackQuery(param); });
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
        if (m_callbacksHandler.gotResponseToPreviousCallback(commandMessage))
            return;

        // ����� ���������� �� � ���, ������ �� eUnknown ������������ ����� ������
        get_service<CommandsInfoService>().ensureNeedAnswerOnCommand(m_telegramUsers, CommandsInfoService::Command::eUnknown,
                                                                     commandMessage, messageToUser);
    }

    if (!messageToUser.empty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandMessage(CommandsInfoService::Command command, const MessagePtr& message)
{
    // �.�. ������� ����� ������ �� ������� ������ �� ����� �� ������ ��������������
    // ������������� ������������ ��� � �������� �����
    get_service<CMassages>().call(
        [this, command, &message]()
        {
            std::wstring messageToUser;
            // ��������� ��� ���� ������������� �������� �� ������� ����� ������������
            if (get_service<CommandsInfoService>().ensureNeedAnswerOnCommand(m_telegramUsers, command,
                                                                             message, messageToUser))
            {
                m_telegramUsers->setUserLastCommand(message->from, message->text);

                switch (command)
                {
                default:
                    assert(!"����������� �������!.");
                    [[fallthrough]];
                case CommandsInfoService::Command::eInfo:
                    onCommandInfo(message);
                    break;
                case CommandsInfoService::Command::eReport:
                    onCommandReport(message);
                    break;
                case CommandsInfoService::Command::eRestart:
                    onCommandRestart(message);
                    break;
                case CommandsInfoService::Command::eAlertingOn:
                case CommandsInfoService::Command::eAlertingOff:
                    onCommandAlert(message, command == CommandsInfoService::Command::eAlertingOn);
                    break;
                case CommandsInfoService::Command::eAlarmingValue:
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
    const ITelegramUsersList::UserStatus userStatus = m_telegramUsers->getUserStatus(commandMessage->from);

    const std::wstring messageToUser = get_service<CommandsInfoService>().getAvailableCommandsWithDescription(userStatus);
    if (!messageToUser.empty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
    else
        assert(!"������ ���� ��������� � �����!");
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandReport(const MessagePtr& commandMessage) const
{
    if (get_monitoring_service()->getNamesOfMonitoringChannels().empty())
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"������ ��� ����������� �� �������");
        return;
    }

    // ���������� ������������ ������ � ������� ������ �� �������� ����� �����
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // �������� ������ � ��������, text ���������� ��� ������ ����� ����� ������������� � UTF-8, ����� ������ �� �����
    auto addButton = [&keyboard](const CString& text, const callback::report::ReportType reportType)
    {
        using namespace callback;

        // ������ �� ������ ������ ������ ���� ����
        // kKeyWord kParamType={'ReportType'}
        const auto button =
            create_keyboard_button(text,
                                   KeyboardCallback(report::kKeyWord).
                                       addCallbackParam(report::kParamType, std::to_wstring(static_cast<unsigned long>(reportType))));

        keyboard->inlineKeyboard.push_back({ button });
    };

    // ��������� ������
    addButton(L"��� ������",         callback::report::ReportType::eAllChannels);
    addButton(L"������������ �����", callback::report::ReportType::eSpecialChannel);

    m_telegramThread->sendMessage(commandMessage->chat->id,
                                  L"�� ����� ������� ������������ �����?",
                                  false, 0, keyboard);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandRestart(const MessagePtr& commandMessage) const
{
    execute_restart_command(commandMessage, m_telegramThread.get());
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

    using namespace callback;

    // ������ ������� ������ ���� � ������ ������
    KeyboardCallback defaultCallBack(alertEnabling::kKeyWord);
    defaultCallBack.addCallbackParam(alertEnabling::kParamEnable, std::wstring(bEnable ? L"true" : L"false"));

    // ��������� ������ ��� ������� ������
    for (const auto& channelName : monitoringChannels)
    {
        // ������ �� ������ ������ ������ ���� ����
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        KeyboardCallback channelCallBack(defaultCallBack);
        channelCallBack.addCallbackParam(alertEnabling::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ create_keyboard_button(channelName, channelCallBack) });
    }

    // ��������� ������ �� ����� ��������
    if (monitoringChannels.size() > 1)
        keyboard->inlineKeyboard.push_back({
            create_keyboard_button(L"��� ������", KeyboardCallback(defaultCallBack).
                addCallbackParam(alertEnabling::kParamChan, alertEnabling::kValueAllChannels)) });

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

    using namespace callback;
    // ��������� ������ ��� ������� ������
    for (const auto& channelName : monitoringChannels)
    {
        // ������ �� ������ ������ ������ ���� ����
        // kKeyWord kParamChan={'chan1'}
        KeyboardCallback channelCallBack(alarmingValue::kKeyWord);
        channelCallBack.addCallbackParam(alarmingValue::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ create_keyboard_button(channelName, channelCallBack) });
    }

    m_telegramThread->sendMessage(commandMessage->chat->id, L"�������� ����� ��� ��������� ������ ����������.", false, 0, keyboard);
}
