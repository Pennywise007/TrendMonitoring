#include "pch.h"

#include <utility>
#include <xstring>

#include <ext/thread/invoker.h>

#include "KeyboardCallback.h"
#include "TelegramBot.h"

namespace telegram::bot {

using namespace users;
using namespace command;

// Текст который надо отправить боту для авторизации
constexpr std::wstring_view gBotPassword_User   = L"MonitoringAuth";      // авторизация пользователем
constexpr std::wstring_view gBotPassword_Admin  = L"MonitoringAuthAdmin"; // авторизация админом

////////////////////////////////////////////////////////////////////////////////
// Реализация бота
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
    // перечень команд и функции выполняемой при вызове команды
    std::unordered_map<std::string, CommandFunction> commandsList;
    // команда выполняемая при получении любого сообщения
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
    commandsHelper.ResetCommandList(); // для тестов

    // список пользователей которым доступны все команды по умолчанию
    const std::vector<ITelegramUsersList::UserStatus> kDefaultAvailability =
     { ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };

    static_assert(ITelegramUsersList::UserStatus::eLastStatus == 3,
                  "Список пользовтеля изменился, возможно стоит пересмотреть доступность команд");

    typedef CommandsInfoService::Command Command;

    // добавлеение команды в список
    auto addCommand = [&](Command command, std::wstring commandText, std::wstring descr,
                          const std::vector<ITelegramUsersList::UserStatus>& availabilityStatuses,
                          std::set<std::string> callbacksKeyWords = {})
    {
        if (!commandsList.try_emplace(getUtf8Str(commandText),
                                      [this, command](const auto message)
                                      {
                                          this->OnCommandMessage(command, message);
                                      }).second)
            EXT_ASSERT(!"Команды должны быть уникальными!");

        commandsHelper.AddCommand(command, std::move(commandText), std::move(descr),
                                   std::move(callbacksKeyWords), availabilityStatuses);
    };

    static_assert(Command::eLastCommand == (Command)7,
                  "Количество команд изменилось, надо добавить обработчик и тест!");

    using namespace callback;

    // !все команды будем выполнять в основном потоке чтобы везде не добавлять синхронизацию
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

    // команда выполняемая при получении любого сообщения
    onUnknownCommand = OnNonCommandMessage =
        [this](const auto message)
        {
            ext::InvokeMethod([this, &message]() { this->OnNonCommandMessage(message); });
        };
    // отработка колбэков на нажатие клавиатуры
    m_telegramThread->GetBotEvents().onCallbackQuery(
        [this](const auto param)
        {
            ext::InvokeMethod([this, &param]() { this->m_callbacksHandler->OnCallbackQuery(param); });
        });
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnNonCommandMessage(const MessagePtr& commandMessage)
{
    // пользователь отправивший сообщение
    TgBot::User::Ptr pUser = commandMessage->from;
    // текст сообщения пришедшего сообщения
    std::wstring messageText = getUNICODEString(commandMessage->text);
    std::string_trim_all(messageText);

    // убеждаемся что есть такой пользователь
    m_telegramUsers->EnsureExist(pUser, commandMessage->chat->id);

    // сообщение которое будет отправлено пользователю в ответ
    std::wstring messageToUser;

    if (_wcsicmp(messageText.c_str(), gBotPassword_User.data()) == 0)
    {
        // ввод пароля для обычного пользователя
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
        // ввод пароля для администратора
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

        // особо убеждаться не в чем, просто на eUnknown возвращается текст ошибки
        ext::get_service<CommandsInfoService>().EnsureNeedAnswerOnCommand(*m_telegramUsers, CommandsInfoService::Command::eUnknown,
                                                                          commandMessage->from, commandMessage->chat->id, messageToUser);
    }

    if (!messageToUser.empty())
        m_telegramThread->SendMessage(commandMessage->chat->id, messageToUser);
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnCommandMessage(CommandsInfoService::Command command, const MessagePtr& message)
{
    // т.к. команда может придти из другого потока то чтобы не делать дополнительную
    // синхронизацию переправляем все в основной поток
    ext::InvokeMethod(
        [this, command, &message]()
        {
            std::wstring messageToUser;
            // проверяем что есть необходимость отвечать на команду этому пользователю
            if (ext::get_service<CommandsInfoService>().EnsureNeedAnswerOnCommand(
                *m_telegramUsers, command, message->from, message->chat->id, messageToUser))
            {
                m_telegramUsers->SetUserLastCommand(message->from, message->text);

                switch (command)
                {
                default:
                    EXT_ASSERT(!"Неизвестная команда!.");
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
                EXT_ASSERT(!"Должен быть текст сообщений пользователю");
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
        EXT_ASSERT(!"Должно быть сообщение в ответ!");
}

//----------------------------------------------------------------------------//
void CTelegramBot::OnCommandReport(const MessagePtr& commandMessage) const
{
    if (GetInterface<ITrendMonitoring>()->GetNamesOfMonitoringChannels().empty())
    {
        m_telegramThread->SendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
        return;
    }

    // показываем пользователю кнопки в выбором канала по которому нужен отчёт
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // создание кнопки с колбэком, text передается как юникод чтобы потом преобразовать в UTF-8, иначе телега не умеет
    auto addButton = [&keyboard](const std::wstring& text, const callback::report::ReportType reportType)
    {
        using namespace callback;

        // колбэк на запрос отчёта должен быть вида
        // kKeyWord kParamType={'ReportType'}
        const auto button =
            create_keyboard_button(text,
                                   KeyboardCallback(report::kKeyWord).
                                       AddCallbackParam(report::kParamType, std::to_wstring(static_cast<unsigned long>(reportType))));

        keyboard->inlineKeyboard.push_back({ button });
    };

    // добавляем кнопок
    addButton(L"Все каналы",         callback::report::ReportType::eAllChannels);
    addButton(L"Определенный канал", callback::report::ReportType::eSpecialChannel);

    m_telegramThread->SendMessage(commandMessage->chat->id,
                                  L"По каким каналам сформировать отчёт?",
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
    // получаем список каналов
    std::list<std::wstring> monitoringChannels = GetInterface<ITrendMonitoring>()->GetNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->SendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
        return;
    }

    // показываем пользователю кнопки в выбором канала по которому нужен отчёт
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    using namespace callback;

    // колбэк который должен быть у каждой кнопки
    KeyboardCallback defaultCallBack(alertEnabling::kKeyWord);
    defaultCallBack.AddCallbackParam(alertEnabling::kParamEnable, (bEnable ? L"true" : L"false"));

    // добавляем кнопки для каждого канала
    for (const auto& channelName : monitoringChannels)
    {
        // колбэк на запрос отчёта должен быть вида
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        KeyboardCallback channelCallBack(defaultCallBack);
        channelCallBack.AddCallbackParam(alertEnabling::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ create_keyboard_button(channelName, channelCallBack) });
    }

    // добавляем кнопку со всеми каналами
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
    // получаем список каналов
    std::list<std::wstring> monitoringChannels = GetInterface<ITrendMonitoring>()->GetNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->SendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
        return;
    }

    // показываем пользователю кнопки в выбором канала по которому нужен отчёт
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    using namespace callback;
    // добавляем кнопки для каждого канала
    for (const auto& channelName : monitoringChannels)
    {
        // колбэк на запрос отчёта должен быть вида
        // kKeyWord kParamChan={'chan1'}
        KeyboardCallback channelCallBack(alarmingValue::kKeyWord);
        channelCallBack.AddCallbackParam(alarmingValue::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ callback::create_keyboard_button(channelName, channelCallBack) });
    }

    m_telegramThread->SendMessage(commandMessage->chat->id, L"Выберите канал для изменения уровня оповещений.", false, 0, keyboard);
}

} // namespace telegram::bot