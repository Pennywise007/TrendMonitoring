#include "pch.h"

#include <utility>
#include <xstring>

#include <ext/thread/invoker.h>

#include "KeyboardCallback.h"
#include "TelegramBot.h"

namespace telegram::bot {

using namespace users;
using namespace command;

// ����� ������� ���� ��������� ���� ��� �����������
constexpr std::wstring_view gBotPassword_User   = L"MonitoringAuth";      // ����������� �������������
constexpr std::wstring_view gBotPassword_Admin  = L"MonitoringAuthAdmin"; // ����������� �������

////////////////////////////////////////////////////////////////////////////////
// ���������� ����
CTelegramBot::CTelegramBot(ext::ServiceProvider::Ptr provider,
                           std::shared_ptr<telegram::users::ITelegramUsersList>&& telegramUsers,
                           std::shared_ptr<ITelegramThread>&& thread)
    : ServiceProviderHolder(std::move(provider))
    , m_telegramUsers(std::move(telegramUsers))
    , m_callbacksHandler(CreateObject<callback::TelegramCallbacks>())
    , m_telegramThread(std::move(thread))
{
}

//----------------------------------------------------------------------------//
CTelegramBot::~CTelegramBot()
{
    if (m_telegramThread)
        m_telegramThread->StopTelegramThread();
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnBotSettingsChanged(const bool newEnableValue, const std::wstring& newBotToken)
{
    // �������� ������ � ������� ����������� ��� ������ �������
    std::unordered_map<std::string, CommandFunction> commandsList;
    // ������� ����������� ��� ��������� ������ ���������
    CommandFunction onUnknownCommand;
    CommandFunction OnNonCommandMessage;
    FillCommandHandlers(commandsList, onUnknownCommand, OnNonCommandMessage);

    m_telegramThread->StartTelegramThread(commandsList, onUnknownCommand, OnNonCommandMessage);
}

//----------------------------------------------------------------------------//
void CTelegramBot::SendMessageToAdmins(const std::wstring& message) const
{
    if (!m_telegramThread)
        return;
    const auto chats = m_telegramUsers->GetAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin);
    if (chats.empty())
        return;

    m_telegramThread->SendMessage(
        m_telegramUsers->GetAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin),
        message);
}

//----------------------------------------------------------------------------//
void CTelegramBot::SendMessageToUsers(const std::wstring& message) const
{
    if (!m_telegramThread)
        return;

    const auto chats = m_telegramUsers->GetAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin);
    if (chats.empty())
        return;

    m_telegramThread->SendMessage(
        m_telegramUsers->GetAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser),
        message);
}

//----------------------------------------------------------------------------//
void CTelegramBot::FillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                                       CommandFunction& onUnknownCommand,
                                       CommandFunction& OnNonCommandMessage)
{
    CommandsInfoService& commandsHelper = ext::get_service<CommandsInfoService>();
    commandsHelper.ResetCommandList(); // ��� ������

    // ������ ������������� ������� �������� ��� ������� �� ���������
    const std::vector<ITelegramUsersList::UserStatus> kDefaultAvailability =
     { ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };

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
                                          this->OnCommandMessage(command, message);
                                      }).second)
            EXT_ASSERT(!"������� ������ ���� �����������!");

        commandsHelper.AddCommand(command, std::move(commandText), std::move(descr),
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
    onUnknownCommand = OnNonCommandMessage =
        [this](const auto message)
        {
            ext::InvokeMethod([this, &message]() { this->OnNonCommandMessage(message); });
        };
    // ��������� �������� �� ������� ����������
    m_telegramThread->GetBotEvents().onCallbackQuery(
        [this](const auto param)
        {
            ext::InvokeMethod([this, &param]() { this->m_callbacksHandler->OnCallbackQuery(param); });
        });
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnNonCommandMessage(const MessagePtr& commandMessage)
{
    // ������������ ����������� ���������
    TgBot::User::Ptr pUser = commandMessage->from;
    // ����� ��������� ���������� ���������
    std::wstring messageText = getUNICODEString(commandMessage->text);
    std::string_trim_all(messageText);

    // ���������� ��� ���� ����� ������������
    m_telegramUsers->EnsureExist(pUser, commandMessage->chat->id);

    // ��������� ������� ����� ���������� ������������ � �����
    std::wstring messageToUser;

    if (_wcsicmp(messageText.c_str(), gBotPassword_User.data()) == 0)
    {
        // ���� ������ ��� �������� ������������
        switch (m_telegramUsers->GetUserStatus(pUser))
        {
        case ITelegramUsersList::UserStatus::eAdmin:
            messageToUser = L"������������ �������� ��������������� �������. ����������� �� ���������.";
            break;
        case ITelegramUsersList::UserStatus::eOrdinaryUser:
            messageToUser = L"������������ ��� �����������.";
            break;
        default:
            EXT_ASSERT(!"�� ��������� ��� ������������.");
            [[fallthrough]];
        case ITelegramUsersList::UserStatus::eNotAuthorized:
            m_telegramUsers->SetUserStatus(pUser, ITelegramUsersList::UserStatus::eOrdinaryUser);
            messageToUser = L"������������ ������� �����������.\n\n" +
                ext::get_service<CommandsInfoService>().GetAvailableCommandsWithDescription(ITelegramUsersList::UserStatus::eOrdinaryUser);
            break;
        }
    }
    else if (_wcsicmp(messageText.c_str(), gBotPassword_Admin.data()) == 0)
    {
        // ���� ������ ��� ��������������
        switch (m_telegramUsers->GetUserStatus(pUser))
        {
        case ITelegramUsersList::UserStatus::eAdmin:
            messageToUser = L"������������ ��� ����������� ��� �������������.";
            break;
        default:
            EXT_ASSERT(!"�� ��������� ��� ������������.");
            [[fallthrough]];
        case ITelegramUsersList::UserStatus::eOrdinaryUser:
        case ITelegramUsersList::UserStatus::eNotAuthorized:
            m_telegramUsers->SetUserStatus(pUser, ITelegramUsersList::UserStatus::eAdmin);
            messageToUser = L"������������ ������� ����������� ��� �������������.\n\n" +
                ext::get_service<CommandsInfoService>().GetAvailableCommandsWithDescription(ITelegramUsersList::UserStatus::eAdmin);
            break;
        }
    }
    else
    {
        if (m_callbacksHandler->GotResponseToPreviousCallback(commandMessage))
            return;

        // ����� ���������� �� � ���, ������ �� eUnknown ������������ ����� ������
        ext::get_service<CommandsInfoService>().EnsureNeedAnswerOnCommand(*m_telegramUsers, CommandsInfoService::Command::eUnknown,
                                                                          commandMessage->from, commandMessage->chat->id, messageToUser);
    }

    if (!messageToUser.empty())
        m_telegramThread->SendMessage(commandMessage->chat->id, messageToUser);
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnCommandMessage(CommandsInfoService::Command command, const MessagePtr& message)
{
    // �.�. ������� ����� ������ �� ������� ������ �� ����� �� ������ ��������������
    // ������������� ������������ ��� � �������� �����
    ext::InvokeMethod(
        [this, command, &message]()
        {
            std::wstring messageToUser;
            // ��������� ��� ���� ������������� �������� �� ������� ����� ������������
            if (ext::get_service<CommandsInfoService>().EnsureNeedAnswerOnCommand(
                *m_telegramUsers, command, message->from, message->chat->id, messageToUser))
            {
                m_telegramUsers->SetUserLastCommand(message->from, message->text);

                switch (command)
                {
                default:
                    EXT_ASSERT(!"����������� �������!.");
                    [[fallthrough]];
                case CommandsInfoService::Command::eInfo:
                    OnCommandInfo(message);
                    break;
                case CommandsInfoService::Command::eReport:
                    OnCommandReport(message);
                    break;
                case CommandsInfoService::Command::eRestart:
                    OnCommandRestart(message);
                    break;
                case CommandsInfoService::Command::eAlertingOn:
                case CommandsInfoService::Command::eAlertingOff:
                    OnCommandAlert(message, command == CommandsInfoService::Command::eAlertingOn);
                    break;
                case CommandsInfoService::Command::eAlarmingValue:
                    OnCommandAlarmingValue(message);
                    break;
                }
            }
            else if (!messageToUser.empty())
                // ���� ������������ ������� �� �������� ���������� ��� ���������� �� �����
                m_telegramThread->SendMessage(message->chat->id, messageToUser);
            else
                EXT_ASSERT(!"������ ���� ����� ��������� ������������");
        });
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnCommandInfo(const MessagePtr& commandMessage) const
{
    const ITelegramUsersList::UserStatus userStatus = m_telegramUsers->GetUserStatus(commandMessage->from);

    const std::wstring messageToUser = ext::get_service<CommandsInfoService>().GetAvailableCommandsWithDescription(userStatus);
    if (!messageToUser.empty())
        m_telegramThread->SendMessage(commandMessage->chat->id, messageToUser);
    else
        EXT_ASSERT(!"������ ���� ��������� � �����!");
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnCommandReport(const MessagePtr& commandMessage) const
{
    if (GetInterface<ITrendMonitoring>()->GetNamesOfMonitoringChannels().empty())
    {
        m_telegramThread->SendMessage(commandMessage->chat->id, L"������ ��� ����������� �� �������");
        return;
    }

    // ���������� ������������ ������ � ������� ������ �� �������� ����� �����
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // �������� ������ � ��������, text ���������� ��� ������ ����� ����� ������������� � UTF-8, ����� ������ �� �����
    auto addButton = [&keyboard](const std::wstring& text, const callback::report::ReportType reportType)
    {
        using namespace callback;

        // ������ �� ������ ������ ������ ���� ����
        // kKeyWord kParamType={'ReportType'}
        const auto button =
            create_keyboard_button(text,
                                   KeyboardCallback(report::kKeyWord).
                                       AddCallbackParam(report::kParamType, std::to_wstring(static_cast<unsigned long>(reportType))));

        keyboard->inlineKeyboard.push_back({ button });
    };

    // ��������� ������
    addButton(L"��� ������",         callback::report::ReportType::eAllChannels);
    addButton(L"������������ �����", callback::report::ReportType::eSpecialChannel);

    m_telegramThread->SendMessage(commandMessage->chat->id,
                                  L"�� ����� ������� ������������ �����?",
                                  false, 0, keyboard);
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnCommandRestart(const MessagePtr& commandMessage) const
{
    execute_restart_command(commandMessage->chat->id, m_telegramThread.get());
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnCommandAlert(const MessagePtr& commandMessage, bool bEnable) const
{
    // �������� ������ �������
    std::list<std::wstring> monitoringChannels = GetInterface<ITrendMonitoring>()->GetNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->SendMessage(commandMessage->chat->id, L"������ ��� ����������� �� �������");
        return;
    }

    // ���������� ������������ ������ � ������� ������ �� �������� ����� �����
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    using namespace callback;

    // ������ ������� ������ ���� � ������ ������
    KeyboardCallback defaultCallBack(alertEnabling::kKeyWord);
    defaultCallBack.AddCallbackParam(alertEnabling::kParamEnable, (bEnable ? L"true" : L"false"));

    // ��������� ������ ��� ������� ������
    for (const auto& channelName : monitoringChannels)
    {
        // ������ �� ������ ������ ������ ���� ����
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        KeyboardCallback channelCallBack(defaultCallBack);
        channelCallBack.AddCallbackParam(alertEnabling::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ create_keyboard_button(channelName, channelCallBack) });
    }

    // ��������� ������ �� ����� ��������
    if (monitoringChannels.size() > 1)
        keyboard->inlineKeyboard.push_back({
            create_keyboard_button(L"��� ������", KeyboardCallback(defaultCallBack).
                AddCallbackParam(alertEnabling::kParamChan, alertEnabling::kValueAllChannels)) });

    const std::wstring text = std::string_swprintf(L"�������� ����� ��� %s ����������.", bEnable ? L"���������" : L"����������");
    m_telegramThread->SendMessage(commandMessage->chat->id, text, false, 0, keyboard);
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnCommandAlarmingValue(const MessagePtr& commandMessage) const
{
    // �������� ������ �������
    std::list<std::wstring> monitoringChannels = GetInterface<ITrendMonitoring>()->GetNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->SendMessage(commandMessage->chat->id, L"������ ��� ����������� �� �������");
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
        channelCallBack.AddCallbackParam(alarmingValue::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ callback::create_keyboard_button(channelName, channelCallBack) });
    }

    m_telegramThread->SendMessage(commandMessage->chat->id, L"�������� ����� ��� ��������� ������ ����������.", false, 0, keyboard);
}

} // namespace telegram::bot