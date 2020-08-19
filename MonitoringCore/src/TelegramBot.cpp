#include "pch.h"

#include <filesystem>
#include <regex>

#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/std_pair.hpp>

#include <Messages.h>
#include <DirsService.h>

#include "TelegramBot.h"
#include "Utils.h"

// Текст который надо отправить боту для авторизации
const CString gBotPassword_User   = L"MonitoringAuth";      // авторизация пользователем
const CString gBotPassword_Admin  = L"MonitoringAuthAdmin"; // авторизация админом

// параметры для колбэка отчёта
// kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}
namespace reportCallBack
{
    const std::string kKeyWord          = R"(/report)";     // ключевое слово
    // параметры
    const std::string kParamType        = "reportType";     // тип отчёта
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
namespace allertCallBack
{
    const std::string kKeyWord          = R"(/allert)";     // ключевое слово
    // параметры
    const std::string kParamEnable      = "enable";         // состояние включаемости/выключаемости
    const std::string kParamChan        = "chan";           // канал по которому нужно настроить нотификацию
    const std::string kValueAllChannels = "allChannels";    // значение которое соответствует выключению оповещений по всем каналам
};

// общие функции
namespace
{
    //------------------------------------------------------------------------//
    // Функция сощздания клавиши в телеграме
    auto createKeyboardButton(const CString& text, const KeyboardCallback& callback)
    {
        TgBot::InlineKeyboardButton::Ptr channelButton(new TgBot::InlineKeyboardButton);

        // текст должен быть в UTF-8
        channelButton->text = getUtf8Str(text);
        channelButton->callbackData = callback.buildReport();

        return channelButton;
    }
};

////////////////////////////////////////////////////////////////////////////////
// Реализация бота
CTelegramBot::CTelegramBot()
{
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
                                                   static_cast<ITelegramAllerter*>(this));

        m_telegramThread.swap(pTelegramThread);
    }

    // перечень команд и функции выполняемой при вызове команды
    std::map<std::string, CommandFunction> commandsList;
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

        // проверяем что это наше задание
        auto it = m_monitoringTasksInfo.find(monitoringResult->m_taskId);
        if (it == m_monitoringTasksInfo.end())
            return;

        // флаг что нужен детальный отчёт
        bool bDetailedInfo = it->second.userStatus == ITelegramUsersList::UserStatus::eAdmin;
        // запоминаем идентификатор чата
        int64_t chatId = it->second.chatId;

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

            reportText.AppendFormat(L"Канал \"%s\":", channelResData.pTaskParameters->channelName);

            switch (channelResData.resultType)
            {
            case MonitoringResult::Result::eSucceeded:  // данные успешно получены
                {
                    if (bDetailedInfo)
                    {
                        // значение, достугнув которое необходимо оповестить. Не оповещать - NAN
                        float allarmingValue = NAN;

                        // если отчёт подробный - ищем какое оповещательное значение у канала
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

                        // если мы нашли значение при котором стоит оповещать - проверяем превышение этого значения
                        if (_finite(allarmingValue) != 0)
                        {
                            // если за интервал одно из значений вышло за допустимые
                            if ((allarmingValue >= 0 && channelResData.maxValue >= allarmingValue) ||
                                (allarmingValue < 0 && channelResData.minValue <= allarmingValue))
                                reportText.AppendFormat(L"допустимый уровень %.02f был превышен, ",
                                                        allarmingValue);
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
        m_telegramThread->sendMessage(chatId, reportText);
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

        // Добавляем кнопки действий для этой ошибки
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

        // колбэк на перезапуск мониторинга
        // kKeyWord
        auto buttonRestart = createKeyboardButton(L"Перезапустить систему",
                                                  KeyboardCallback(restartCallBack::kKeyWord));

        // добавляем новую ошибку в список и запоминаем её идентификатор
        m_monitoringErrors.emplace_back(errorData.get());

        if (m_monitoringErrors.size() > kMaxErrorInfoCount)
            m_monitoringErrors.pop_front();

        // колбэк на передачу этого сообщения обычным пользователям
        // kKeyWord kParamid={'GUID'}
        auto buttonResendToOrdinary = createKeyboardButton(L"Оповестить обычных пользователей",
            KeyboardCallback(resendCallBack::kKeyWord).
                addCallbackParam(resendCallBack::kParamid, CString(CComBSTR(errorData->errorGUID))));

        keyboard->inlineKeyboard.push_back({ buttonRestart, buttonResendToOrdinary });

        // пересылаем всем админам текст ошибки и клавиатуру для решения проблем
        m_telegramThread->sendMessage(adminsChats, errorData->errorText, false, 0, keyboard);
    }
    else
        assert(!"Неизвестное событие");
}

//----------------------------------------------------------------------------//
void CTelegramBot::fillCommandHandlers(std::map<std::string, CommandFunction>& commandsList,
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
    auto addCommand = [&]
        (const Command command, const CString& commandText, const CString& descr,
         const std::vector<ITelegramUsersList::UserStatus>& availabilityStatuses)
    {
        m_commandHelper ->addCommand(command, commandText,
                                     descr, availabilityStatuses);
        commandsList.try_emplace(getUtf8Str(commandText),
                                 [this, command](const auto message)
                                 {
                                     this->onCommandMessage(command, message);
                                 });
    };

    static_assert(Command::eLastCommand == (Command)6,
                  "Количество команд изменилось, надо добавить обработчик и тест!");

    // !все команды будем выполнять в основном потоке чтобы везде не добавлять синхронизацию
    addCommand(Command::eInfo,    L"info",    L"Перечень команд бота.", kDefaultAvailability);
    addCommand(Command::eReport,  L"report",  L"Сформировать отчёт.",   kDefaultAvailability);
    addCommand(Command::eRestart, L"restart", L"Перезапустить систему мониторинга.",
               { ITelegramUsersList::eAdmin });
    addCommand(Command::eAllertionsOn,  L"allertionOn",  L"Включить оповещения о событиях.",
               { ITelegramUsersList::eAdmin });
    addCommand(Command::eAllertionsOff, L"allertionOff", L"Выключить оповещения о событиях.",
               { ITelegramUsersList::eAdmin });

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
void CTelegramBot::onNonCommandMessage(const MessagePtr commandMessage)
{
    // пользователь отправивший сообщение
    TgBot::User::Ptr pUser = commandMessage->from;
    // текст сообщения пришедшего сообщения
    CString messageText = getUNICODEString(commandMessage->text.c_str()).Trim();

    // убеждаемся что есть такой пользователь
    m_telegramUsers->ensureExist(pUser, commandMessage->chat->id);

    // сообщение которое будет отправлено пользователю в ответ
    CString messageToUser;

    if (messageText.CompareNoCase(gBotPassword_User) == 0)
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
    else if (messageText.CompareNoCase(gBotPassword_Admin) == 0)
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
        // особо убеждаться не в чем, прросто на eUnknown возвращается текст ошибки
        m_commandHelper->ensureNeedAnswerOnCommand(m_telegramUsers, Command::eUnknown,
                                                   commandMessage, messageToUser);
    }

    if (!messageToUser.IsEmpty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandMessage(const Command command, const MessagePtr message)
{
    // т.к. команда может придти из другого потока то чтобы не делать дополнительную
    // синхронизацию переправляем все в основной поток
    get_service<CMassages>().call(
        [this, &command, &message]()
        {
            CString messageToUser;
            // проверяем что есть необходимость отвечать на команду этому пользователю
            if (m_commandHelper->ensureNeedAnswerOnCommand(m_telegramUsers, command,
                                                           message, messageToUser))
            {
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
                case Command::eAllertionsOn:
                case Command::eAllertionsOff:
                    onCommandAllert(message, command == Command::eAllertionsOn);
                    break;
                }
            }
            else if (!messageToUser.IsEmpty())
                // если пользователю команда не доступна возвращаем ему оповещение об этомы
                m_telegramThread->sendMessage(message->chat->id, messageToUser);
            else
                assert(!"Должен быть текст сообщений пользователю");
        });
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandInfo(const MessagePtr commandMessage)
{
    ITelegramUsersList::UserStatus userStatus =
        m_telegramUsers->getUserStatus(commandMessage->from);

    CString messageToUser = m_commandHelper->getAvailableCommandsWithDescr(userStatus);
    if (!messageToUser.IsEmpty())
        m_telegramThread->sendMessage(commandMessage->chat->id, messageToUser);
    else
        assert(!"Должно быть сообщение в ответ!");
}

//----------------------------------------------------------------------------//
void CTelegramBot::onCommandReport(const MessagePtr commandMessage)
{
    // получаем список каналов
    std::list<CString> monitoringChannels = get_monitoing_service()->getNamesOfMonitoringChannels();
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
void CTelegramBot::onCommandRestart(const MessagePtr commandMessage)
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
void CTelegramBot::onCommandAllert(const MessagePtr commandMessage, bool bEnable)
{
    // получаем список каналов
    std::list<CString> monitoringChannels = get_monitoing_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
    {
        m_telegramThread->sendMessage(commandMessage->chat->id, L"Каналы для мониторинга не выбраны");
        return;
    }

    // показываем пользователю кнопки в выбором канала по которому нужен отчёт
    TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    // колбэк который должен быть у каждой кнопки
    KeyboardCallback defaultCallBack(allertCallBack::kKeyWord);
    defaultCallBack.addCallbackParam(allertCallBack::kParamEnable, CString(bEnable ? L"true" : L"false"));

    // создание кнопки с колбэком, text передается как юникод чтобы потом преобразовать в UTF-8, иначе телега не умеет
    auto addButtonWithChannel = [&keyboard, &defaultCallBack](const CString& channelName)
    {
        // колбэк на запрос отчёта должен быть вида
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        auto button =
            createKeyboardButton(
                channelName,
                KeyboardCallback(defaultCallBack).
                addCallbackParam(allertCallBack::kParamChan, channelName));

        keyboard->inlineKeyboard.push_back({ button });
    };

    // добавляем кнопки для каждого канала
    for (const auto& channel : monitoringChannels)
    {
        addButtonWithChannel(channel);
    }

    // добавляем кнопку со всеми каналами
    if (monitoringChannels.size() > 1)
        keyboard->inlineKeyboard.push_back({
            createKeyboardButton(L"Все каналы",
                                 KeyboardCallback(defaultCallBack).
                                    addCallbackParam(allertCallBack::kParamChan, allertCallBack::kValueAllChannels)) });

    CString text;
    text.Format(L"Выберите канал для %s оповещений.", bEnable ? L"включения" : L"выключения");
    m_telegramThread->sendMessage(commandMessage->chat->id, text, false, 0, keyboard);
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
void CTelegramBot::onCallbackQuery(const TgBot::CallbackQuery::Ptr query)
{
    try
    {
        // парсим колбэк и проверяем каких параметров не хватает
        CallBackParams callBackParams;
        if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                     reportCallBack::kKeyWord >> CallbackParser::parser, callBackParams))
            executeCallbackReport(query, callBackParams);
        else if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                          restartCallBack::kKeyWord >> CallbackParser::parser, callBackParams))
            executeCallbackRestart(query, callBackParams);
        else if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                          resendCallBack::kKeyWord >> CallbackParser::parser, callBackParams))
            executeCallbackResend(query, callBackParams);
        else if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                          allertCallBack::kKeyWord >> CallbackParser::parser, callBackParams))
            executeCallbackAllertion(query, callBackParams);
        else
            throw std::runtime_error("Ошибка разбора команды");
    }
    catch (const std::exception& exc)
    {
        assert(false);

        CString errorStr;
        // дополняем ошибку текстом запроса
        errorStr.Format(L"%s. Обратитесь к администратору системы, текст запроса \"%s\"",
                        CString(exc.what()).GetString(), getUNICODEString(query->data).GetString());

        // отвечаем хоть что-то пользователю
        m_telegramThread->sendMessage(query->message->chat->id, errorStr);
        // оповещаем об ошибке
        onAllertFromTelegram(errorStr);
    }
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackReport(const TgBot::CallbackQuery::Ptr query,
                                         const CallBackParams& reportParams)
{
    // В колбэке отчёта должны быть определенные параметры, итоговый колбэк должен быть вида
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}

    // тип отчёта
    auto reportTypeIt = reportParams.find(reportCallBack::kParamType);
    // проверяем какой вид колбэка пришел и каких параметров не хватает
    if (reportTypeIt == reportParams.end())
        throw std::runtime_error("Не известный колбэк.");

    // получаем список каналов
    std::list<CString> monitoringChannels = get_monitoing_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("Не удалось получить список каналов, попробуйте повторить попытку");

    // имя канала по которому делаем отчёт
    auto channelIt = reportParams.find(reportCallBack::kParamChan);

    // тип отчёта
    ReportType reportType = (ReportType)std::stoul(reportTypeIt->second);

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
                KeyboardCallback defCallBack = KeyboardCallback(reportCallBack::kKeyWord).
                    addCallbackParam(reportTypeIt->first, reportTypeIt->second);

                // показываем пользователю кнопки в выбором канала по которому нужен отчёт
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
                                              L"Выберите канал",
                                              false, 0, keyboard);
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

        m_telegramThread->sendMessage(query->message->chat->id,
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

            channelsForTask.emplace_back(getUNICODEString(channelIt->second));
            break;
        }

        CTime stopTime = CTime::GetCurrentTime();
        CTime startTime = stopTime -
            monitoring_interval_to_timespan((MonitoringInterval)std::stoi(timeIntervalIt->second));

        TaskInfo taskInfo;
        taskInfo.chatId = query->message->chat->id;
        taskInfo.userStatus = m_telegramUsers->getUserStatus(query->from);

        // Отвечаем пользователю что запрос выполняется, выполняться может долго
        // пользователь может испугаться что ничего не происходит
        m_telegramThread->sendMessage(query->message->chat->id,
                                      L"Выполняется расчёт данных, это может занять некоторое время.");

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
    assert(params.empty() && "Параметров не предусмотрено");

    CString messageToUser;
    if (!m_commandHelper->ensureNeedAnswerOnCommand(m_telegramUsers, Command::eRestart,
                                                    query->message, messageToUser))
    {
        assert(!"Пользователь запросил перезапуск без разрешения на выполнение такого действия.");
        m_telegramThread->sendMessage(query->message->chat->id,
                                      L"У вас нет разрешения на перезапуск системы, обратитесь к администратору!");
    }
    else
        // имитируем что пользователь выполнил запрос рестарта
        onCommandRestart(query->message);
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackResend(const TgBot::CallbackQuery::Ptr query,
                                         const CallBackParams& params)
{
    auto errorIdIt = params.find(resendCallBack::kParamid);
    if (errorIdIt == params.end())
        throw std::runtime_error("Нет необходимого параметра у колбэка пересылки сообщения.");

    // вытаскиваем гуид из строки
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
        text.Format(L"Пересылаемой ошибки нет в списке, возможно ошибка является устаревшей (хранятся последние %u ошибок) или программа была перещапущена.",
                    kMaxErrorInfoCount);
        m_telegramThread->sendMessage(query->message->chat->id, text);
        return;
    }

    if (errorIt->bResendToOrdinaryUsers)
    {
        m_telegramThread->sendMessage(query->message->chat->id,
                                      L"Ошибка уже была переслана.");
        return;
    }

    // пересылаем ошибку обычным пользователям
    auto ordinaryUsersChatList = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser);
    m_telegramThread->sendMessage(ordinaryUsersChatList, errorIt->errorText);

    errorIt->bResendToOrdinaryUsers = true;

    m_telegramThread->sendMessage(query->message->chat->id,
                                  L"Ошибка была успешно переслана обычным пользователям.");
}

//----------------------------------------------------------------------------//
void CTelegramBot::executeCallbackAllertion(const TgBot::CallbackQuery::Ptr query,
                                            const CallBackParams& params)
{
    // Формат колбэка kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
    auto enableIt = params.find(allertCallBack::kParamEnable);
    auto channelIt = params.find(allertCallBack::kParamChan);

    if (enableIt == params.end() || channelIt == params.end())
        throw std::runtime_error("Нет необходимого параметра у колбэка управления оповещениями.");
    // включаемость/выключаемость оповещений
    bool bEnableAllertion = enableIt->second == "true";
    // сервис мониторинга
    auto monitoringService = get_monitoing_service();
    // сообщение в ответ пользователю
    CString messageText;
    if (channelIt->second == allertCallBack::kValueAllChannels)
    {
        // настраивают оповещения для всех каналов
        size_t channelsCount = monitoringService->getNumberOfMonitoringChannels();
        if (channelsCount == 0)
            throw std::runtime_error("Нет выбранных для мониторинга каналов, обратитесь к администратору");

        for (size_t channelInd = 0; channelInd < channelsCount; ++channelInd)
        {
            monitoringService->changeMonitoingChannelNotify(channelInd, bEnableAllertion);
        }

        messageText.Format(L"Оповещения для всех каналов %s", bEnableAllertion ? L"включены" : L"выключены");
    }
    else
    {
        // получаем список каналов
        std::list<CString> monitoringChannels = monitoringService->getNamesOfMonitoringChannels();
        if (monitoringChannels.empty())
            throw std::runtime_error("Нет выбранных для мониторинга каналов, обратитесь к администратору");

        // имя канала из колбэка
        CString callBackChannel = getUNICODEString(channelIt->second);
        // считаем что в списке мониторинга каналы по именам не повторяются иначе это глупо
        auto channelIt = std::find_if(monitoringChannels.begin(), monitoringChannels.end(),
                                      [&callBackChannel](const auto& channelName)
                                      {
                                          return callBackChannel == channelName;
                                      });

        if (channelIt == monitoringChannels.end())
            throw std::runtime_error("В данный момент в списке мониторинга нет выбранного вами канала.");

        monitoringService->changeMonitoingChannelNotify(std::distance(monitoringChannels.begin(), channelIt),
                                                        bEnableAllertion);

        messageText.Format(L"Оповещения для канала %s %s", callBackChannel, bEnableAllertion ? L"включены" : L"выключены");
    }

    assert(!messageText.IsEmpty() && "Сообщение пользователю пустое.");
    m_telegramThread->sendMessage(query->message->chat->id, messageText);
}

////////////////////////////////////////////////////////////////////////////////
// Формирование колбэка
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
    // колбэк должен быть вида
    // /%COMMAND% %PARAM_1%={'%VALUE_1%'} %PARAM_2%={'%VALUE_2%'} ...
    // Формируем пару %PARAM_N%={'%VALUE_N'}
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
    // Список экранируемых символов, если не экранировать - ругается телеграм
    const std::wregex escapedCharacters(LR"(['])");
    const std::wstring rep(LR"(\$&)");

    // из-за того что TgTypeParser::appendToJson сам не добавляет '}'
    // если последним символом является '}' добавляем в конце пробел
    CString resultReport = std::regex_replace((m_reportStr + " ").GetString(),
                                              escapedCharacters,
                                              rep).c_str();

    // максимальное ограничение не размер запроса телеграма 65 символов
    //constexpr int maxReportSize = 65;
    //assert(resultReport.GetLength() <= maxReportSize);

    return getUtf8Str(resultReport);
}

////////////////////////////////////////////////////////////////////////////////
// вспомогательный клас для работы с командами телеграма
void CTelegramBot::CommandsHelper::
addCommand(const CTelegramBot::Command command,
           const CString& commandText, const CString& descr,
           const std::vector<AvailableStatus>& availabilityStatuses)
{
    auto commandDescrIt = m_botCommands.try_emplace(command, commandText, descr);
    assert(commandDescrIt.second && "Команда уже добавлена.");

    CommandDescription& commandDescr = commandDescrIt.first->second;
    for (auto& status : availabilityStatuses)
        commandDescr.m_availabilityForUsers[status] = true;
}

//----------------------------------------------------------------------------//
CString CTelegramBot::CommandsHelper::
getAvailableCommandsWithDescr(const AvailableStatus userStatus) const
{
    CString message;
    for (auto& command : m_botCommands)
    {
        if (command.second.m_availabilityForUsers[userStatus])
            message += L"/" + command.second.m_text + L" - " + command.second.m_description + L"\n";
    }

    if (!message.IsEmpty())
        message = L"Поддерживаемые команды бота:\n\n\n" + message + L"\n\nДля того чтобы их использовать необходимо написать их этому боту(обязательно использовать слэш перед текстом команды(/)!). Или нажать в этом окне, они должны подсвечиваться.";
    return message;
}

//----------------------------------------------------------------------------//
bool CTelegramBot::CommandsHelper::
ensureNeedAnswerOnCommand(ITelegramUsersList* usersList,
                          const CTelegramBot::Command command,
                          const MessagePtr commandMessage,
                          CString& messageToUser) const
{

    // пользователь отправивший сообщение
    const TgBot::User::Ptr& pUser = commandMessage->from;

    // убеждаемся что есть такой пользователь
    usersList->ensureExist(pUser, commandMessage->chat->id);
    // получаем статус пользователя
    auto userStatus = usersList->getUserStatus(pUser);
    // проверяем что пользователь может использовать команду
    auto commandIt = m_botCommands.find(command);
    if (commandIt == m_botCommands.end() ||
        !commandIt->second.m_availabilityForUsers[userStatus])
    {
        // формируем ответ пользователю со списком доступных ему команд
        CString availableCommands = getAvailableCommandsWithDescr(userStatus);
        if (availableCommands.IsEmpty())
        {
            if (userStatus == ITelegramUsersList::eNotAuthorized)
                messageToUser = L"Для работы бота вам необходимо авторизоваться.";
            else
                messageToUser = L"Неизвестная команда. У вас нет доступных команд, обратитесь к администратору";
        }
        else
            messageToUser = L"Неизвестная команда. " + std::move(availableCommands);
        assert(!messageToUser.IsEmpty());

        return false;
    }

    return true;
}