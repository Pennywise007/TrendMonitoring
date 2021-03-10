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

// Текст который надо отправить боту для авторизации
constexpr std::wstring_view gBotPassword_User   = L"MonitoringAuth";      // авторизация пользователем
constexpr std::wstring_view gBotPassword_Admin  = L"MonitoringAuthAdmin"; // авторизация админом

// параметры для колбэка отчёта
// kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}
namespace reportCallBack
{
    const std::string kKeyWord          = R"(/report)";     // ключевое слово
    // параметры
    const std::string kParamType        = "type";           // тип отчёта
    const std::string kParamChan        = "chan";           // канал по которому нужен отчёт
    const std::string kParamInterval    = "interval";       // интервал
};

// параметры для колбэка рестарта системы
namespace restartCallBack
{
    const std::string kKeyWord          = R"(/restart)";    // ключевое словоа
};

// параметры для колбэка пересылки сообщенияы
// kKeyWord kParamid={'GUID'}
namespace resendCallBack
{
    const std::string kKeyWord          = R"(/resend)";     // ключевое слово
    // параметры
    const std::string kParamid          = "errorId";        // идентификатор ошибки в списке ошибок(m_monitoringErrors)
};

// параметры для колбэка оповещения
// kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
namespace alertEnablingCallBack
{
    const std::string kKeyWord          = R"(/alert)";      // ключевое слово
    // параметры
    const std::string kParamEnable      = "enable";         // состояние включаемости/выключаемости
    const std::string kParamChan        = "chan";           // канал по которому нужно настроить нотификацию
    const std::string kValueAllChannels = "allChannels";    // значение которое соответствует выключению оповещений по всем каналам
};

// параметры для колбэка изменения уровня оповещений
// kKeyWord kParamChan={'chan1'} kValue={'0.2'}
namespace alarmingValueCallBack
{
    const std::string kKeyWord = R"(/alarmV)";              // ключевое слово
    // параметры
    const std::string kParamChan = "chan";                  // канал по которому нужно настроить уровень оповещений
    const std::string kValue = "val";                       // новое значение уровня оповещений
}

// общие функции
namespace
{
    //------------------------------------------------------------------------//
    // создание кнопки с колбэком в телеграме, text передается как юникод чтобы потом преобразовать в UTF-8, иначе телега не умеет
    auto createKeyboardButton(const CString& text, const KeyboardCallback& callback)
    {
        TgBot::InlineKeyboardButton::Ptr channelButton(new TgBot::InlineKeyboardButton);

        // текст должен быть в UTF-8
        channelButton->text = getUtf8Str(text.GetString());
        channelButton->callbackData = callback.buildReport();

        return channelButton;
    }
};

////////////////////////////////////////////////////////////////////////////////
// Реализация бота
CTelegramBot::CTelegramBot()
{
    m_commandHelper = std::make_shared<CommandsHelper>();

    // подписываемся на события о завершении мониторинга
    EventRecipientImpl::subscribe(onCompletingMonitoringTask);
    // подписываемся на события о возникающих ошибках в процессе запроса новых данных
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

        // проверяем что это наше задание
        auto it = m_monitoringTasksInfo.find(monitoringResult->m_taskId);
        if (it == m_monitoringTasksInfo.end())
            return;

        // флаг что нужен детальный отчёт
        const bool bDetailedInfo = it->second.userStatus == ITelegramUsersList::UserStatus::eAdmin;
        // запоминаем идентификатор чата
        const int64_t chatId = it->second.chatId;

        // удаляем задание из списка
        m_monitoringTasksInfo.erase(it);

        // формируем отчёт
        CString reportText;
        reportText.Format(L"Отчёт за %s - %s\n\n",
                          monitoringResult->m_taskResults.front().pTaskParameters->startTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString(),
                          monitoringResult->m_taskResults.front().pTaskParameters->endTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString());

        // формируем отчёт по всем каналам
        for (auto& channelResData : monitoringResult->m_taskResults)
        {
            // допустимое количество пропусков данных - десятая часть от интервала
            CTimeSpan permissibleEmptyDataTime =
                (channelResData.pTaskParameters->endTime - channelResData.pTaskParameters->startTime).GetTotalSeconds() / 30;

            reportText.AppendFormat(L"Канал \"%s\":", channelResData.pTaskParameters->channelName.GetString());

            switch (channelResData.resultType)
            {
            case MonitoringResult::Result::eSucceeded:  // данные успешно получены
                {
                    if (bDetailedInfo)
                    {
                        // значение, достугнув которое необходимо оповестить. Не оповещать - NAN
                        float alarmingValue = NAN;

                        // если отчёт подробный - ищем какое оповещательное значение у канала
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

                        // если мы нашли значение при котором стоит оповещать - проверяем превышение этого значения
                        if (isfinite(alarmingValue))
                        {
                            // если за интервал одно из значений вышло за допустимые
                            if ((alarmingValue >= 0 && channelResData.maxValue >= alarmingValue) ||
                                (alarmingValue < 0 && channelResData.minValue <= alarmingValue))
                                reportText.AppendFormat(L"допустимый уровень %.02f был превышен, ",
                                                        alarmingValue);
                        }
                    }

                    reportText.AppendFormat(L"значения за интервал [%.02f..%.02f], последнее показание - %.02f.",
                                            channelResData.minValue, channelResData.maxValue,
                                            channelResData.currentValue);

                    // если много пропусков данных
                    if (bDetailedInfo && channelResData.emptyDataTime > permissibleEmptyDataTime)
                        reportText.AppendFormat(L" Много пропусков данных (%s).",
                                                time_span_to_string(channelResData.emptyDataTime).GetString());
                }
                break;
            case MonitoringResult::Result::eNoData:     // в переданном интервале нет данных
            case MonitoringResult::Result::eErrorText:  // возникла ошибка
                {
                    // оповещаем о возникшей ошибке
                    if (!channelResData.errorText.IsEmpty())
                        reportText.Append(channelResData.errorText);
                    else
                        reportText.Append(L"Нет данных в запрошенном интервале.");
                }
                break;
            default:
                assert(!"Не известный тип результата");
                break;
            }

            reportText += L"\n";
        }

        // отправляем ответом текст отчёта
        m_telegramThread->sendMessage(chatId, reportText.GetString());
    }
    else if (code == onMonitoringErrorEvent)
    {
        std::shared_ptr<MonitoringErrorEventData> errorData =
            std::static_pointer_cast<MonitoringErrorEventData>(eventData);
        assert(!errorData->errorText.IsEmpty());

        // получаем список админских чатов
        auto adminsChats = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin);
        if (adminsChats.empty() || errorData->errorText.IsEmpty())
            return;

        // добавляем новую ошибку в список и запоминаем её идентификатор
        m_monitoringErrors.emplace_back(errorData.get());

        if (m_monitoringErrors.size() > kMaxErrorInfoCount)
            m_monitoringErrors.pop_front();

        // Добавляем кнопки действий для этой ошибки
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

        // колбэк на перезапуск мониторинга
        // kKeyWord
        KeyboardCallback callbackRestart(restartCallBack::kKeyWord);

        // колбэк на передачу этого сообщения обычным пользователям
        // kKeyWord kParamid={'GUID'}
        KeyboardCallback callBackOrdinaryUsers(resendCallBack::kKeyWord);
        callBackOrdinaryUsers.addCallbackParam(resendCallBack::kParamid, CString(CComBSTR(errorData->errorGUID)));

        keyboard->inlineKeyboard.push_back({ createKeyboardButton(L"Перезапустить систему", callbackRestart),
                                             createKeyboardButton(L"Оповестить обычных пользователей", callBackOrdinaryUsers) });

        // пересылаем всем админам текст ошибки и клавиатуру для решения проблем
        m_telegramThread->sendMessage(adminsChats, errorData->errorText.GetString(), false, 0, keyboard);
    }
    else
        assert(!"Неизвестное событие");
}

//----------------------------------------------------------------------------//
void CTelegramBot::fillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                                       CommandFunction& onUnknownCommand,
                                       CommandFunction& onNonCommandMessage)
{
    m_commandHelper = std::make_shared<CommandsHelper>();

    // список пользователей которым доступны все команды по умолчанию
    const std::vector<ITelegramUsersList::UserStatus> kDefaultAvailability =
    {   ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };

    static_assert(ITelegramUsersList::UserStatus::eLastStatus == 3,
                  "Список пользовтеля изменился, возможно стоит пересмотреть доступность команд");

    // добавлеение команды в список
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
            assert(!"Команды должны быть уникальными!");
    };

    static_assert(Command::eLastCommand == (Command)7,
                  "Количество команд изменилось, надо добавить обработчик и тест!");

    // !все команды будем выполнять в основном потоке чтобы везде не добавлять синхронизацию
    addCommand(Command::eInfo,    L"info",    L"Перечень команд бота.", kDefaultAvailability);
    addCommand(Command::eReport,  L"report",  L"Сформировать отчёт.",   kDefaultAvailability);
    addCommand(Command::eRestart, L"restart", L"Перезапустить систему мониторинга.",
               { ITelegramUsersList::eAdmin });
    addCommand(Command::eAlertingOn,  L"alertingOn",  L"Включить оповещения о событиях.",
               { ITelegramUsersList::eAdmin });
    addCommand(Command::eAlertingOff, L"alertingOff", L"Выключить оповещения о событиях.",
               { ITelegramUsersList::eAdmin });
    addCommand(Command::eAlarmingValue, L"alarmingValue", L"Изменить допустимый уровень значений у канала.",
               { ITelegramUsersList::eAdmin });

    auto addCommandCallback = [commandCallbacks = &m_commandCallbacks]
    (const std::string& keyWord, const CommandCallback& callback)
    {
        if (!commandCallbacks->try_emplace(keyWord, callback).second)
            assert(!"Ключевые слова у колбэков должны отличаться!");
    };
    addCommandCallback(reportCallBack::       kKeyWord, &CTelegramBot::executeCallbackReport);
    addCommandCallback(restartCallBack::      kKeyWord, &CTelegramBot::executeCallbackRestart);
    addCommandCallback(resendCallBack::       kKeyWord, &CTelegramBot::executeCallbackResend);
    addCommandCallback(alertEnablingCallBack::kKeyWord, &CTelegramBot::executeCallbackAlert);
    addCommandCallback(alarmingValueCallBack::kKeyWord, &CTelegramBot::executeCallbackAlarmValue);

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
            get_service<CMassages>().call([this, &param]() { this->onCallbackQuery(param); });
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
        if (gotResponseToPreviousCallback(commandMessage))
            return;

        // особо убеждаться не в чем, просто на eUnknown возвращается текст ошибки
        m_commandHelper->ensureNeedAnswerOnCommand(m_telegramUsers, Command::eUnknown,
                                                   commandMessage, messageToUser);
    }

    if (!messageToUser.empty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandMessage(const Command command, const MessagePtr& message)
{
    // т.к. команда может придти из другого потока то чтобы не делать дополнительную
    // синхронизацию переправляем все в основной поток
    get_service<CMassages>().call(
        [this, &command, &message]()
        {
            std::wstring messageToUser;
            // проверяем что есть необходимость отвечать на команду этому пользователю
            if (m_commandHelper->ensureNeedAnswerOnCommand(m_telegramUsers, command,
                                                           message, messageToUser))
            {
                m_telegramUsers->setUserLastCommand(message->from, message->text);

                switch (command)
                {
                default:
                    assert(!"Неизвестная команда!.");
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
                // если пользователю команда не доступна возвращаем ему оповещение об этомы
                m_telegramThread->sendMessage(message->chat->id, messageToUser);
            else
                assert(!"Должен быть текст сообщений пользователю");
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
        assert(!"Должно быть сообщение в ответ!");
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandReport(const MessagePtr& commandMessage) const
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

    // создание кнопки с колбэком, text передается как юникод чтобы потом преобразовать в UTF-8, иначе телега не умеет
    auto addButton = [&keyboard](const CString& text, const ReportType reportType)
    {
        // колбэк на запрос отчёта должен быть вида
        // kKeyWord kParamType={'ReportType'}
        auto button =
            createKeyboardButton(
                text,
                KeyboardCallback(reportCallBack::kKeyWord).
                addCallbackParam(reportCallBack::kParamType, std::to_wstring((unsigned long)reportType)));

        keyboard->inlineKeyboard.push_back({ button });
    };

    // добавляем кнопок
    addButton(L"Все каналы",         ReportType::eAllChannels);
    addButton(L"Определенный канал", ReportType::eSpecialChannel);

    m_telegramThread->sendMessage(commandMessage->chat->id,
                                  L"По каким каналам сформировать отчёт?",
                                  false, 0, keyboard);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandRestart(const MessagePtr& commandMessage) const
{
    // перезапуск системы делаем через батник так как надо много всего сделать для разных систем
    CString batFullPath = get_service<DirsService>().getExeDir() + kRestartSystemFileName;

    // сообщение пользователю
    CString messageToUser;
    if (std::filesystem::is_regular_file(batFullPath.GetString()))
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"Перезапуск системы осуществляется.");

        // запускаем батник
        STARTUPINFO cif = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION m_ProcInfo = { 0 };
        if (FALSE != CreateProcess(batFullPath.GetBuffer(),     // имя исполняемого модуля
                                   nullptr,	                    // Командная строка
                                   NULL,                        // Указатель на структуру SECURITY_ATTRIBUTES
                                   NULL,                        // Указатель на структуру SECURITY_ATTRIBUTES
                                   0,                           // Флаг наследования текущего процесса
                                   NULL,                        // Флаги способов создания процесса
                                   NULL,                        // Указатель на блок среды
                                   NULL,                        // Текущий диск или каталог
                                   &cif,                        // Указатель на структуру STARTUPINFO
                                   &m_ProcInfo))                // Указатель на структуру PROCESS_INFORMATION)
        {	// идентификатор потока не нужен
            CloseHandle(m_ProcInfo.hThread);
            CloseHandle(m_ProcInfo.hProcess);
        }
    }
    else
        m_telegramThread->sendMessage(commandMessage->chat->id, L"Файл для перезапуска не найден.");
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

    // колбэк который должен быть у каждой кнопки
    KeyboardCallback defaultCallBack(alertEnablingCallBack::kKeyWord);
    defaultCallBack.addCallbackParam(alertEnablingCallBack::kParamEnable, std::wstring(bEnable ? L"true" : L"false"));

    // добавляем кнопки для каждого канала
    for (const auto& channelName : monitoringChannels)
    {
        // колбэк на запрос отчёта должен быть вида
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        KeyboardCallback channelCallBack(defaultCallBack);
        channelCallBack.addCallbackParam(alertEnablingCallBack::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ createKeyboardButton(channelName, channelCallBack) });
    }

    // добавляем кнопку со всеми каналами
    if (monitoringChannels.size() > 1)
        keyboard->inlineKeyboard.push_back({
            createKeyboardButton(L"Все каналы", KeyboardCallback(defaultCallBack).
                addCallbackParam(alertEnablingCallBack::kParamChan, alertEnablingCallBack::kValueAllChannels)) });

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

    // добавляем кнопки для каждого канала
    for (const auto& channelName : monitoringChannels)
    {
        // колбэк на запрос отчёта должен быть вида
        // kKeyWord kParamChan={'chan1'}
        KeyboardCallback channelCallBack(alarmingValueCallBack::kKeyWord);
        channelCallBack.addCallbackParam(alarmingValueCallBack::kParamChan, channelName);

        keyboard->inlineKeyboard.push_back({ createKeyboardButton(channelName, channelCallBack) });
    }

    m_telegramThread->sendMessage(commandMessage->chat->id, L"Выберите канал для изменения уровня оповещений.", false, 0, keyboard);
}

////////////////////////////////////////////////////////////////////////////////
// парсинг колбэков
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
        // парсим колбэк и проверяем каких параметров не хватает
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

        throw std::runtime_error("Ошибка разбора команды");
    }
    catch (const std::exception& exc)
    {
        assert(false);

        CString errorStr;
        // дополняем ошибку текстом запроса
        errorStr.Format(L"%s. Обратитесь к администратору системы, текст запроса \"%s\"",
                        CString(exc.what()).GetString(), getUNICODEString(query->data).c_str());

        // отвечаем хоть что-то пользователю
        m_telegramThread->sendMessage(query->message->chat->id, errorStr.GetString());
        // оповещаем об ошибке
        send_message_to_log(LogMessageData::MessageType::eError, errorStr);
    }
}

//----------------------------------------------------------------------------//
bool CTelegramBot::gotResponseToPreviousCallback(const MessagePtr& commandMessage)
{
    const std::string lastBotCommand = m_telegramUsers->getUserLastCommand(commandMessage->from);

    try
    {
        // парсим колбэк и проверяем каких параметров не хватает
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
                    throw std::runtime_error("Отсутствует обработчик для команды " + callbackKeyWord + "!");
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        OUTPUT_LOG_FUNC;
        OUTPUT_LOG_SET_TEXT(L"Неизвестное сообщение от пользователя: %s\nТекст сообщения: %s\nПоследняя команда пользователя: %s\nОшибка: %s",
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
    // В колбэке отчёта должны быть определенные параметры, итоговый колбэк должен быть вида
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}

    // тип отчёта
    auto reportTypeIt = reportParams.find(reportCallBack::kParamType);
    // проверяем какой вид колбэка пришел и каких параметров не хватает
    if (reportTypeIt == reportParams.end())
        throw std::runtime_error("Не известный колбэк.");

    // получаем список каналов
    std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("Не удалось получить список каналов, попробуйте повторить попытку");

    // имя канала по которому делаем отчёт
    const auto channelIt = reportParams.find(reportCallBack::kParamChan);

    // тип отчёта
    const ReportType reportType = (ReportType)std::stoul(reportTypeIt->second);

    // первый колбэк формата "kKeyWord kParamType={'ReportType'}" и проверяем может надо спросить по какому каналу нужен отчёт
    switch (reportType)
    {
    default:
        assert(!"Не известный тип отчёта.");
        [[fallthrough]];
    case ReportType::eSpecialChannel:
        {
            // если канал не указан надо его запросить
            if (channelIt == reportParams.end())
            {
                // формируем колбэк
                KeyboardCallback defCallBack(reportCallBack::kKeyWord);
                defCallBack.addCallbackParam(reportTypeIt->first, reportTypeIt->second);

                // показываем пользователю кнопки в выбором канала по которому нужен отчёт
                TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
                keyboard->inlineKeyboard.reserve(monitoringChannels.size());

                for (const auto& channel : monitoringChannels)
                {
                    const auto channelButton =
                        createKeyboardButton(channel, KeyboardCallback(defCallBack).addCallbackParam(reportCallBack::kParamChan, channel));

                    keyboard->inlineKeyboard.push_back({ channelButton });
                }

                m_telegramThread->sendMessage(message->chat->id, L"Выберите канал", false, 0, keyboard);
                return;
            }
        }
        break;

    case ReportType::eAllChannels:
        break;
    }

    auto timeIntervalIt = reportParams.find(reportCallBack::kParamInterval);
    // проверяем что kParamInterval задан
    if (timeIntervalIt == reportParams.end())
    {
        // формируем колбэк
        KeyboardCallback defCallBack(reportCallBack::kKeyWord);
        defCallBack.addCallbackParam(reportTypeIt->first, reportTypeIt->second);

        // если указано имя - добавляем его
        if (channelIt != reportParams.end())
            defCallBack.addCallbackParam(channelIt->first, channelIt->second);

        // просим пользователя задать интервал
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        keyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);

        // добавляем кнопки со всеми интервалами
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            auto intervalButton = createKeyboardButton(
                monitoring_interval_to_string(MonitoringInterval(i)),
                KeyboardCallback(defCallBack).
                addCallbackParam(reportCallBack::kParamInterval, std::to_wstring(i)));

            keyboard->inlineKeyboard[i] = { intervalButton };
        }

        m_telegramThread->sendMessage(message->chat->id,
                                      L"Выберите интервал времени за который нужно показать отчёт",
                                      false, 0, keyboard);
    }
    else
    {
        // получили все необходимые параметры, запускаем задание мониторинга
        // список каналов для мониторинга
        std::list<CString> channelsForTask;

        switch (reportType)
        {
        default:
            assert(!"Не известный тип отчёта.");
            [[fallthrough]];
        case ReportType::eAllChannels:
            channelsForTask = std::move(monitoringChannels);
            break;

        case ReportType::eSpecialChannel:
            if (channelIt == reportParams.end())
                throw std::runtime_error("Не удалось распознать имя канала, попробуйте повторить попытку");

            channelsForTask.emplace_back(getUNICODEString(channelIt->second).c_str());
            break;
        }

        const CTime stopTime = CTime::GetCurrentTime();
        const CTime startTime = stopTime -
            monitoring_interval_to_timespan((MonitoringInterval)std::stoi(timeIntervalIt->second));

        TaskInfo taskInfo;
        taskInfo.chatId = message->chat->id;
        taskInfo.userStatus = m_telegramUsers->getUserStatus(message->from);

        // Отвечаем пользователю что запрос выполняется, выполняться может долго
        // пользователь может испугаться что ничего не происходит
        m_telegramThread->sendMessage(message->chat->id,
                                      L"Выполняется расчёт данных, это может занять некоторое время.");

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
    assert(params.empty() && "Параметров не предусмотрено");

    std::wstring messageToUser;
    if (!m_commandHelper->ensureNeedAnswerOnCommand(m_telegramUsers, Command::eRestart, message, messageToUser))
    {
        assert(!"Пользователь запросил перезапуск без разрешения на выполнение такого действия.");
        m_telegramThread->sendMessage(message->chat->id,
                                      L"У вас нет разрешения на перезапуск системы, обратитесь к администратору!");
    }
    else
        // имитируем что пользователь выполнил запрос рестарта
        onCommandRestart(message);
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackResend(const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer)
{
    auto errorIdIt = params.find(resendCallBack::kParamid);
    if (errorIdIt == params.end())
        throw std::runtime_error("Нет необходимого параметра у колбэка пересылки сообщения.");

    // вытаскиваем гуид из строки
    GUID errorGUID;
    if (FAILED(CLSIDFromString(CComBSTR(errorIdIt->second.c_str()), &errorGUID)))
        assert(!"Не удалось получить гуид!");

    auto errorIt = std::find_if(m_monitoringErrors.begin(), m_monitoringErrors.end(),
                                [&errorGUID](const ErrorInfo& errorInfo)
                                {
                                    return IsEqualGUID(errorGUID, errorInfo.errorGUID);
                                });
    if (errorIt == m_monitoringErrors.end())
    {
        CString text;
        text.Format(L"Пересылаемой ошибки нет в списке, возможно ошибка является устаревшей (хранятся последние %u ошибок) или программа была перезапущена.",
                    kMaxErrorInfoCount);
        m_telegramThread->sendMessage(message->chat->id, text.GetString());
        return;
    }

    if (errorIt->bResendToOrdinaryUsers)
    {
        m_telegramThread->sendMessage(message->chat->id,
                                      L"Ошибка уже была переслана.");
        return;
    }

    // пересылаем ошибку обычным пользователям
    const auto ordinaryUsersChatList = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser);
    m_telegramThread->sendMessage(ordinaryUsersChatList, errorIt->errorText.GetString());

    errorIt->bResendToOrdinaryUsers = true;

    m_telegramThread->sendMessage(message->chat->id,
                                  L"Ошибка была успешно переслана обычным пользователям.");
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackAlert(const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer)
{
    // Формат колбэка kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
    const auto enableIt = params.find(alertEnablingCallBack::kParamEnable);
    const auto channelIt = params.find(alertEnablingCallBack::kParamChan);

    if (enableIt == params.end() || channelIt == params.end())
        throw std::runtime_error("Нет необходимого параметра у колбэка управления оповещениями.");
    // включаемость/выключаемость оповещений
    bool bEnableAlertion = enableIt->second == "true";
    // сервис мониторинга
    auto monitoringService = get_monitoring_service();
    // сообщение в ответ пользователю
    CString messageText;
    if (channelIt->second == alertEnablingCallBack::kValueAllChannels)
    {
        // настраивают оповещения для всех каналов
        size_t channelsCount = monitoringService->getNumberOfMonitoringChannels();
        if (channelsCount == 0)
            throw std::runtime_error("Нет выбранных для мониторинга каналов, обратитесь к администратору");

        for (size_t channelInd = 0; channelInd < channelsCount; ++channelInd)
        {
            monitoringService->changeMonitoringChannelNotify(channelInd, bEnableAlertion);
        }

        messageText.Format(L"Оповещения для всех каналов %s", bEnableAlertion ? L"включены" : L"выключены");
    }
    else
    {
        // получаем список каналов
        std::list<CString> monitoringChannels = monitoringService->getNamesOfMonitoringChannels();
        if (monitoringChannels.empty())
            throw std::runtime_error("Нет выбранных для мониторинга каналов, обратитесь к администратору");

        // имя канала из колбэка
        const CString callBackChannel = getUNICODEString(channelIt->second).c_str();
        // считаем что в списке мониторинга каналы по именам не повторяются иначе это глупо
        const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
            [&callBackChannel](const auto& channelName)
            {
                return callBackChannel == channelName;
            });

        if (channelIt == monitoringChannels.cend())
            throw std::runtime_error("В данный момент в списке мониторинга нет выбранного вами канала.");

        monitoringService->changeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt),
                                                         bEnableAlertion);

        messageText.Format(L"Оповещения для канала %s %s", callBackChannel.GetString(), bEnableAlertion ? L"включены" : L"выключены");
    }

    assert(!messageText.IsEmpty() && "Сообщение пользователю пустое.");
    m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackAlarmValue(const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer)
{
    // Формат колбэка kKeyWord kParamChan={'chan1'} kLevel={'5.5'}
    const auto channelParam = params.find(alarmingValueCallBack::kParamChan);

    if (channelParam == params.end())
        throw std::runtime_error("Нет необходимого параметра у колбэка управления оповещениями.");

    // получаем список каналов
    const std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("Нет выбранных для мониторинга каналов, обратитесь к администратору");

    const std::wstring callBackChannel = getUNICODEString(channelParam->second).c_str();
    const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
                                                             [_channelName = CString(callBackChannel.c_str())](const auto& channelName)
                                                             {
                                                                 return channelName == _channelName;
                                                             });
    if (channelIt == monitoringChannels.cend())
        throw std::runtime_error("Выбранный канал отсутствует в списке наблюдаемых каналов.");

    auto getNewLevelFromText = [](const std::string& text)
    {
        float newLevel = NAN;
        if (text != "NAN")
        {
            std::istringstream str(text);
            str >> newLevel;
            if (str.fail())
                throw std::runtime_error("Не удалось преобразовать переданное значение в число.");
        }

        return newLevel;
    };

    const auto newLevelParam = params.find(alarmingValueCallBack::kValue);
    if (newLevelParam == params.end())
    {
        if (!gotAnswer)
        {
            m_telegramThread->sendMessage(message->chat->id, L"Для того чтобы изменить допустимый уровень значений у канала '" + callBackChannel +
                                          L"' отправьте новый уровень ответным сообщением, отправьте NAN чтобы отключить оповещения совсем.");
            return;
        }

        CString messageText;
        std::wstringstream newLevelText;

        const float newLevel = getNewLevelFromText(message->text);
        if (!isfinite(newLevel))
        {
            if (!get_monitoring_service()->getMonitoringChannelData(std::distance(monitoringChannels.cbegin(), channelIt)).bNotify)
            {
                messageText.Format(L"Оповещения у канала '%s' уже выключены.", callBackChannel.c_str());
                m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
                return;
            }

            newLevelText << L"NAN";
            messageText.Format(L"Отключить оповещения для канала '%s'?", callBackChannel.c_str());
        }
        else
        {
            newLevelText << newLevel;
            messageText.Format(L"Установить значение оповещений для канала '%s' как %s?", callBackChannel.c_str(), newLevelText.str().c_str());
        }

        // формируем колбэк подтверждения операции
        KeyboardCallback acceptCallBack(alarmingValueCallBack::kKeyWord);
        acceptCallBack.addCallbackParam(alarmingValueCallBack::kParamChan, callBackChannel);
        acceptCallBack.addCallbackParam(alarmingValueCallBack::kValue, newLevelText.str());

        TgBot::InlineKeyboardMarkup::Ptr acceptingOperationKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        acceptingOperationKeyboard->inlineKeyboard = {{ createKeyboardButton(L"Установить", acceptCallBack) }};

        m_telegramThread->sendMessage(message->chat->id, messageText.GetString(), false, 0, acceptingOperationKeyboard);
    }
    else
    {
        const float newLevel = getNewLevelFromText(newLevelParam->second);

        CString messageText;
        if (!isfinite(newLevel))
        {
            messageText.Format(L"Оповещения для канала '%s' выключены", callBackChannel.c_str());
            get_monitoring_service()->changeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt), false);
        }
        else
        {
            std::wstringstream newLevelText;
            newLevelText << newLevel;

            messageText.Format(L"Значение %s установлено для канала '%s' успешно", newLevelText.str().c_str(), callBackChannel.c_str());
            get_monitoring_service()->changeMonitoringChannelAlarmingValue(std::distance(monitoringChannels.cbegin(), channelIt), newLevel);
        }

        m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
    }
}
