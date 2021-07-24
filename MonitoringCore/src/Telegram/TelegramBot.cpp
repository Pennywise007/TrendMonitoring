#include "pch.h"

#include <utility>

#include <Messages.h>
#include <DirsService.h>

#include "KeyboardCallback.h"
#include "TelegramBot.h"

// Текст который надо отправить боту для авторизации
constexpr std::wstring_view gBotPassword_User   = L"MonitoringAuth";      // авторизация пользователем
constexpr std::wstring_view gBotPassword_Admin  = L"MonitoringAuthAdmin"; // авторизация админом

////////////////////////////////////////////////////////////////////////////////
// Реализация бота
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
        // пока не ресетим и ждем чтобы не зависало на ожидании завершения
        // TODO переделать на BoostHttpOnlySslClient в dll
        //m_telegramThread.reset();
    }

    m_botSettings = botSettings;

    if (!m_botSettings.bEnable || m_botSettings.sToken.IsEmpty())
        return;

    // запускаем поток мониторинга
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

    // перечень команд и функции выполняемой при вызове команды
    std::unordered_map<std::string, CommandFunction> commandsList;
    // команда выполняемая при получении любого сообщения
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
    commandsHelper->resetCommandList(); // для тестов

    // список пользователей которым доступны все команды по умолчанию
    const std::vector<ITelegramUsersList::UserStatus> kDefaultAvailability =
    {   ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };

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
                                          this->onCommandMessage(command, message);
                                      }).second)
            assert(!"Команды должны быть уникальными!");

        commandsHelper->addCommand(command, std::move(commandText), std::move(descr),
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
    onUnknownCommand = onNonCommandMessage =
        [this](const auto message)
    {
        get_service<CMassages>().call([this, &message]() { this->onNonCommandMessage(message); });
    };
    // отработка колбэков на нажатие клавиатуры
    m_telegramThread->getBotEvents().onCallbackQuery(
        [this](const auto param)
        {
            get_service<CMassages>().call([this, &param]() { this->m_callbacksHandler.onCallbackQuery(param); });
        });
}

//----------------------------------------------------------------------------//
void CTelegramBot::onNonCommandMessage(const MessagePtr& commandMessage)
{
    // пользователь отправивший сообщение
    TgBot::User::Ptr pUser = commandMessage->from;
    // текст сообщения пришедшего сообщения
    CString messageText = getUNICODEString(commandMessage->text).c_str();
    messageText.Trim();

    // убеждаемся что есть такой пользователь
    m_telegramUsers->ensureExist(pUser, commandMessage->chat->id);

    // сообщение которое будет отправлено пользователю в ответ
    std::wstring messageToUser;

    if (messageText.CompareNoCase(gBotPassword_User.data()) == 0)
    {
        // ввод пароля для обычного пользователя
        switch (m_telegramUsers->getUserStatus(pUser))
        {
        case ITelegramUsersList::UserStatus::eAdmin:
            messageToUser = L"Пользователь является администратором системы. Авторизация не требуется.";
            break;
        case ITelegramUsersList::UserStatus::eOrdinaryUser:
            messageToUser = L"Пользователь уже авторизован.";
            break;
        default:
            assert(!"Не известный тип пользователя.");
            [[fallthrough]];
        case ITelegramUsersList::UserStatus::eNotAuthorized:
            m_telegramUsers->setUserStatus(pUser, ITelegramUsersList::UserStatus::eOrdinaryUser);
            messageToUser = L"Пользователь успешно авторизован.";
            break;
        }
    }
    else if (messageText.CompareNoCase(gBotPassword_Admin.data()) == 0)
    {
        // ввод пароля для администратора
        switch (m_telegramUsers->getUserStatus(pUser))
        {
        case ITelegramUsersList::UserStatus::eAdmin:
            messageToUser = L"Пользователь уже авторизован как администратор.";
            break;
        default:
            assert(!"Не известный тип пользователя.");
            [[fallthrough]];
        case ITelegramUsersList::UserStatus::eOrdinaryUser:
        case ITelegramUsersList::UserStatus::eNotAuthorized:
            m_telegramUsers->setUserStatus(pUser, ITelegramUsersList::UserStatus::eAdmin);
            messageToUser = L"Пользователь успешно авторизован как администратор.";
            break;
        }
    }
    else
    {
        if (m_callbacksHandler.gotResponseToPreviousCallback(commandMessage))
            return;

        // особо убеждаться не в чем, просто на eUnknown возвращается текст ошибки
        get_service<CommandsInfoService>().ensureNeedAnswerOnCommand(m_telegramUsers, CommandsInfoService::Command::eUnknown,
                                                                     commandMessage, messageToUser);
    }

    if (!messageToUser.empty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandMessage(CommandsInfoService::Command command, const MessagePtr& message)
{
    // т.к. команда может придти из другого потока то чтобы не делать дополнительную
    // синхронизацию переправляем все в основной поток
    get_service<CMassages>().call(
        [this, command, &message]()
        {
            std::wstring messageToUser;
            // проверяем что есть необходимость отвечать на команду этому пользователю
            if (get_service<CommandsInfoService>().ensureNeedAnswerOnCommand(m_telegramUsers, command,
                                                                             message, messageToUser))
            {
                m_telegramUsers->setUserLastCommand(message->from, message->text);

                switch (command)
                {
                default:
                    assert(!"Неизвестная команда!.");
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
                // если пользователю команда не доступна возвращаем ему оповещение об этомы
                m_telegramThread->sendMessage(message->chat->id, messageToUser);
            else
                assert(!"Должен быть текст сообщений пользователю");
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
        assert(!"Должно быть сообщение в ответ!");
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandReport(const MessagePtr& commandMessage) const
{
    if (get_monitoring_service()->getNamesOfMonitoringChannels().empty())
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
        return;
    }

    // показываем пользователю кнопки в выбором канала по которому нужен отчёт
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // создание кнопки с колбэком, text передается как юникод чтобы потом преобразовать в UTF-8, иначе телега не умеет
    auto addButton = [&keyboard](const CString& text, const callback::report::ReportType reportType)
    {
        using namespace callback;

        // колбэк на запрос отчёта должен быть вида
        // kKeyWord kParamType={'ReportType'}
        const auto button =
            create_keyboard_button(text,
                                   KeyboardCallback(report::kKeyWord).
                                       addCallbackParam(report::kParamType, std::to_wstring(static_cast<unsigned long>(reportType))));

        keyboard->inlineKeyboard.push_back({ button });
    };

    // добавляем кнопок
    addButton(L"Все каналы",         callback::report::ReportType::eAllChannels);
    addButton(L"Определенный канал", callback::report::ReportType::eSpecialChannel);

    m_telegramThread->sendMessage(commandMessage->chat->id,
                                  L"По каким каналам сформировать отчёт?",
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
    // получаем список каналов
    std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
        return;
    }

    // показываем пользователю кнопки в выбором канала по которому нужен отчёт
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    using namespace callback;

    // колбэк который должен быть у каждой кнопки
    KeyboardCallback defaultCallBack(alertEnabling::kKeyWord);
    defaultCallBack.addCallbackParam(alertEnabling::kParamEnable, std::wstring(bEnable ? L"true" : L"false"));

    // добавляем кнопки для каждого канала
    for (const auto& channelName : monitoringChannels)
    {
        // колбэк на запрос отчёта должен быть вида
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        KeyboardCallback channelCallBack(defaultCallBack);
        channelCallBack.addCallbackParam(alertEnabling::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ create_keyboard_button(channelName, channelCallBack) });
    }

    // добавляем кнопку со всеми каналами
    if (monitoringChannels.size() > 1)
        keyboard->inlineKeyboard.push_back({
            create_keyboard_button(L"Все каналы", KeyboardCallback(defaultCallBack).
                addCallbackParam(alertEnabling::kParamChan, alertEnabling::kValueAllChannels)) });

    CString text;
    text.Format(L"Выберите канал для %s оповещений.", bEnable ? L"включения" : L"выключения");
    m_telegramThread->sendMessage(commandMessage->chat->id, text.GetString(), false, 0, keyboard);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandAlarmingValue(const MessagePtr& commandMessage) const
{
    // получаем список каналов
    std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
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
        channelCallBack.addCallbackParam(alarmingValue::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ create_keyboard_button(channelName, channelCallBack) });
    }

    m_telegramThread->sendMessage(commandMessage->chat->id, L"Выберите канал для изменения уровня оповещений.", false, 0, keyboard);
}
