#include "pch.h"

#include <utility>
#include <xstring>

#include <ext/thread/invoker.h>

#include "KeyboardCallback.h"
#include "TelegramBot.h"

namespace telegram::bot {

using namespace users;
using namespace command;

// Text to be sent to the bot for authorization
constexpr std::wstring_view gBotPassword_User = L"MonitoringAuth";          // user authorization
constexpr std::wstring_view gBotPassword_Admin = L"MonitoringAuthAdmin";    // admin authorization

// Implementation of the bot
CTelegramBot::CTelegramBot(ext::ServiceProvider::Ptr provider,
                           std::shared_ptr<telegram::users::ITelegramUsersList>&& telegramUsers,
                           std::shared_ptr<ITelegramThread>&& thread)
    : ServiceProviderHolder(std::move(provider))
    , m_telegramUsers(std::move(telegramUsers))
    , m_callbacksHandler(CreateObject<callback::TelegramCallbacks>())
    , m_telegramThread(std::move(thread))
{
}

CTelegramBot::~CTelegramBot()
{
    if (m_telegramThread)
        m_telegramThread->StopTelegramThread();
}

void CTelegramBot::OnBotSettingsChanged(const bool newEnableValue, const std::wstring& newBotToken)
{
    // list of commands and functions executed when the command is called
    std::unordered_map<std::string, CommandFunction> commandsList;
    // command to be executed when any message is received
    CommandFunction onUnknownCommand;
    CommandFunction OnNonCommandMessage;
    FillCommandHandlers(commandsList, onUnknownCommand, OnNonCommandMessage);

    m_telegramThread->StartTelegramThread(commandsList, onUnknownCommand, OnNonCommandMessage);
}

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

void CTelegramBot::FillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                                       CommandFunction& onUnknownCommand,
                                       CommandFunction& OnNonCommandMessage)
{
    CommandsInfoService& commandsHelper = ext::get_service<CommandsInfoService>();
    commandsHelper.ResetCommandList(); // для тестов

    // list of users who have access to all commands by default
    const std::vector<ITelegramUsersList::UserStatus> kDefaultAvailability =
     { ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };

    static_assert(ITelegramUsersList::UserStatus::eLastStatus == 3,
                  "The list of the user has changed, it may be worth revising the availability of commands");

    typedef CommandsInfoService::Command Command;

    // adding a command to the list
    auto addCommand = [&](Command command, std::wstring commandText, std::wstring descr,
                          const std::vector<ITelegramUsersList::UserStatus>& availabilityStatuses,
                          std::set<std::string> callbacksKeyWords = {})
    {
        if (!commandsList.try_emplace(getUtf8Str(commandText),
                                      [this, command](const auto message)
                                      {
                                          this->OnCommandMessage(command, message);
                                      }).second)
            EXT_ASSERT(!"Commands must be unique!");

        commandsHelper.AddCommand(command, std::move(commandText), std::move(descr),
            std::move(callbacksKeyWords), availabilityStatuses);
    };

    static_assert(Command::eLastCommand == (Command)7,
        "The number of commands has changed, we need to add a handler and a test!");

    using namespace callback;

    // !all commands will be executed in the main thread so as not to add synchronization everywhere
    addCommand(Command::eInfo,    L"info",    L"Перечень команд бота.", kDefaultAvailability);
    addCommand(Command::eReport,  L"report",  L"Сформировать отчёт.",   kDefaultAvailability, { report::kKeyWord });
    addCommand(Command::eRestart, L"restart", L"Перезапустить систему мониторинга.",
               { ITelegramUsersList::eAdmin }, { restart::kKeyWord, resend::kKeyWord });
    addCommand(Command::eAlertingOn,  L"alertingOn",  L"Включить оповещения о событиях.",
               { ITelegramUsersList::eAdmin }, { alertEnabling::kKeyWord });
    addCommand(Command::eAlertingOff, L"alertingOff", L"Выключить оповещения о событиях.",
               { ITelegramUsersList::eAdmin }, { alertEnabling::kKeyWord });
    addCommand(Command::eAlarmingValue, L"alarmingValue", L"Изменить допустимый уровень значений у канала.",
               { ITelegramUsersList::eAdmin }, { alarmingValue::kKeyWord });

    // command to be executed when any message is received
    onUnknownCommand = OnNonCommandMessage =
        [this](const auto message)
    {
        ext::InvokeMethod([this, &message]() { this->OnNonCommandMessage(message); });
    };
    // working out callbacks on pressing the keyboard
    m_telegramThread->GetBotEvents().onCallbackQuery(
        [this](const auto param)
        {
            ext::InvokeMethod([this, &param]() { this->m_callbacksHandler->OnCallbackQuery(param); });
        });
}

void CTelegramBot::OnNonCommandMessage(const MessagePtr& commandMessage)
{
    // user who sent the message
    TgBot::User::Ptr pUser = commandMessage->from;
    // message text of the incoming message
    std::wstring messageText = getUNICODEString(commandMessage->text);
    std::string_trim_all(messageText);

    // make sure there is such a user
    m_telegramUsers->EnsureExist(pUser, commandMessage->chat->id);

    // message that will be sent to the user in response
    std::wstring messageToUser;

    if (_wcsicmp(messageText.c_str(), gBotPassword_User.data()) == 0)
    {
        // enter a password for a normal user
        switch (m_telegramUsers->GetUserStatus(pUser))
        {
        case ITelegramUsersList::UserStatus::eAdmin:
            messageToUser = L"Пользователь является администратором системы. Авторизация не требуется.";
            break;
        case ITelegramUsersList::UserStatus::eOrdinaryUser:
            messageToUser = L"Пользователь уже авторизован.";
            break;
        default:
            EXT_ASSERT(!"Не известный тип пользователя.");
            [[fallthrough]];
        case ITelegramUsersList::UserStatus::eNotAuthorized:
            m_telegramUsers->SetUserStatus(pUser, ITelegramUsersList::UserStatus::eOrdinaryUser);
            messageToUser = L"Пользователь успешно авторизован.\n\n" +
                ext::get_service<CommandsInfoService>().GetAvailableCommandsWithDescription(ITelegramUsersList::UserStatus::eOrdinaryUser);
            break;
        }
    }
    else if (_wcsicmp(messageText.c_str(), gBotPassword_Admin.data()) == 0)
    {
        // enter a password for an admin user
        switch (m_telegramUsers->GetUserStatus(pUser))
        {
        case ITelegramUsersList::UserStatus::eAdmin:
            messageToUser = L"Пользователь уже авторизован как администратор.";
            break;
        default:
            EXT_ASSERT(!"Не известный тип пользователя.");
            [[fallthrough]];
        case ITelegramUsersList::UserStatus::eOrdinaryUser:
        case ITelegramUsersList::UserStatus::eNotAuthorized:
            m_telegramUsers->SetUserStatus(pUser, ITelegramUsersList::UserStatus::eAdmin);
            messageToUser = L"Пользователь успешно авторизован как администратор.\n\n" +
                ext::get_service<CommandsInfoService>().GetAvailableCommandsWithDescription(ITelegramUsersList::UserStatus::eAdmin);
            break;
        }
    }
    else
    {
        if (m_callbacksHandler->GotResponseToPreviousCallback(commandMessage))
            return;

        // nothing to be sure of, it just returns the error text on eUnknown
        ext::get_service<CommandsInfoService>().EnsureNeedAnswerOnCommand(*m_telegramUsers, CommandsInfoService::Command::eUnknown,
                                                                          commandMessage->from, commandMessage->chat->id, messageToUser);
    }

    if (!messageToUser.empty())
        m_telegramThread->SendMessage(commandMessage->chat->id, messageToUser);
}

void CTelegramBot::OnCommandMessage(CommandsInfoService::Command command, const MessagePtr& message)
{
    // because the command may come from another thread so as not to do an additional
     // synchronization forward everything to the main thread
    ext::InvokeMethod(
        [this, command, &message]()
        {
            std::wstring messageToUser;
            // check that there is a need to respond to the command for this user
            if (ext::get_service<CommandsInfoService>().EnsureNeedAnswerOnCommand(
                *m_telegramUsers, command, message->from, message->chat->id, messageToUser))
            {
                m_telegramUsers->SetUserLastCommand(message->from, message->text);

                switch (command)
                {
                default:
                    EXT_ASSERT("Unknown command!.");
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
                // если пользователю команда не доступна возвращаем ему оповещение об этомы
                m_telegramThread->SendMessage(message->chat->id, messageToUser);
            else
                EXT_ASSERT(!"Should be the text of messages to the user");
        });
}

void CTelegramBot::OnCommandInfo(const MessagePtr& commandMessage) const
{
    const ITelegramUsersList::UserStatus userStatus = m_telegramUsers->GetUserStatus(commandMessage->from);

    const std::wstring messageToUser = ext::get_service<CommandsInfoService>().GetAvailableCommandsWithDescription(userStatus);
    if (!messageToUser.empty())
        m_telegramThread->SendMessage(commandMessage->chat->id, messageToUser);
    else
        EXT_ASSERT(!"Should be the text of messages to the user");
}

void CTelegramBot::OnCommandReport(const MessagePtr& commandMessage) const
{
    if (GetInterface<ITrendMonitoring>()->GetNamesOfMonitoringChannels().empty())
    {
        m_telegramThread->SendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
        return;
    }

    // show the user the buttons in the choice of the channel for which the report is needed
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // creating a button with a callback, text is passed as unicode to be later converted to UTF-8, otherwise the cart cannot
    auto addButton = [&keyboard](const std::wstring& text, const callback::report::ReportType reportType)
    {
        using namespace callback;

        // callback for the report request should be of the form
        // kKeyWord kParamType={'ReportType'}
        const auto button =
            create_keyboard_button(text,
                KeyboardCallback(report::kKeyWord).
                AddCallbackParam(report::kParamType, std::to_wstring(static_cast<unsigned long>(reportType))));

        keyboard->inlineKeyboard.push_back({ button });
    };

    // add buttons
    addButton(L"Все каналы",         callback::report::ReportType::eAllChannels);
    addButton(L"Определенный канал", callback::report::ReportType::eSpecialChannel);

    m_telegramThread->SendMessage(commandMessage->chat->id,
                                  L"По каким каналам сформировать отчёт?",
                                  false, 0, keyboard);
}

void CTelegramBot::OnCommandRestart(const MessagePtr& commandMessage) const
{
    execute_restart_command(commandMessage->chat->id, m_telegramThread.get());
}

void CTelegramBot::OnCommandAlert(const MessagePtr& commandMessage, bool bEnable) const
{
    // получаем список каналов
    std::list<std::wstring> monitoringChannels = GetInterface<ITrendMonitoring>()->GetNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->SendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
        return;
    }

    // show the user the buttons in the choice of the channel for which the report is needed
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    using namespace callback;

    // callback that every button should have
    KeyboardCallback defaultCallBack(alertEnabling::kKeyWord);
    defaultCallBack.AddCallbackParam(alertEnabling::kParamEnable, (bEnable ? L"true" : L"false"));

    // add buttons for each channel
    for (const auto& channelName : monitoringChannels)
    {
        // callback for the report request should be of the form
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        KeyboardCallback channelCallBack(defaultCallBack);
        channelCallBack.AddCallbackParam(alertEnabling::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ create_keyboard_button(channelName, channelCallBack) });
    }

    // add a button with all channels
    if (monitoringChannels.size() > 1)
        keyboard->inlineKeyboard.push_back({
            create_keyboard_button(L"Все каналы", KeyboardCallback(defaultCallBack).
                AddCallbackParam(alertEnabling::kParamChan, alertEnabling::kValueAllChannels)) });

    const std::wstring text = std::string_swprintf(L"Выберите канал для %s оповещений.", bEnable ? L"включения" : L"выключения");
    m_telegramThread->SendMessage(commandMessage->chat->id, text, false, 0, keyboard);
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnCommandAlarmingValue(const MessagePtr& commandMessage) const
{
    // get a list of channels
    std::list<std::wstring> monitoringChannels = GetInterface<ITrendMonitoring>()->GetNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->SendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
        return;
    }

    // show the user the buttons in the choice of the channel for which the report is needed
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    using namespace callback;
    // add buttons for each channel
    for (const auto& channelName : monitoringChannels)
    {
        // callback for the report request should be of the form
        // kKeyWord kParamChan={'chan1'}
        KeyboardCallback channelCallBack(alarmingValue::kKeyWord);
        channelCallBack.AddCallbackParam(alarmingValue::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ callback::create_keyboard_button(channelName, channelCallBack) });
    }

    m_telegramThread->SendMessage(commandMessage->chat->id, L"Выберите канал для изменения уровня оповещений.", false, 0, keyboard);
}

} // namespace telegram::bot