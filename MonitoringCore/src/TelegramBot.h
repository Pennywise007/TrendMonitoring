#pragma once

#include <afx.h>
#include <bitset>
#include <map>
#include <memory>
#include <time.h>

#include <TelegramDLL/TelegramThread.h>

#include "IMonitoringTasksService.h"
#include "ITelegramUsersList.h"
#include "ITrendMonitoring.h"

////////////////////////////////////////////////////////////////////////////////
// класс управления телеграм ботом
class CTelegramBot
    : public ITelegramAllerter
    , public EventRecipientImpl
{
public:
    CTelegramBot();
    virtual ~CTelegramBot();

public:
    // инициализация бота
    void initBot(ITelegramUsersListPtr telegramUsers);
    // установка исполняемого потока телеграма по умолчанию
    void setDefaultTelegramThread(ITelegramThreadPtr& pTelegramThread);
    // установка настроек бота
    void setBotSettings(const TelegramBotSettings& botSettings);

    // функция отправки сообщения администраторам системы
    void sendMessageToAdmins(const CString& message);
    // функция отправки сообщения пользователям системы
    void sendMessageToUsers(const CString& message);

// ITelegramAllerter
public:
    void onAllertFromTelegram(const CString& allertMessage) override;

// IEventRecipient
public:
    // оповещение о произошедшем событии
    void onEvent(const EventId& code, float eventValue,
                 std::shared_ptr<IEventData> eventData) override;

// Отработка команд бота
private:
    // добавить реакции на команды
    void fillCommandHandlers(std::map<std::string, CommandFunction>& commandsList,
                             CommandFunction& onUnknownCommand,
                             CommandFunction& onNonCommandMessage);
    // отработка не зарегистрированного сообщения/команды
    void onNonCommandMessage(const MessagePtr commandMessage);

    // Перечень команд
    enum class Command
    {
        eUnknown,       // Неизвестная комнада бота
        eInfo,          // Запрос информации бота
        eReport,        // Запрос отчёта за период
        eRestart,       // перезапуск мониторинга
        eAllertionsOn,  // оповещения о событиях канала включить
        eAllertionsOff, // оповещения о событиях канала выключить
        // Последняя команда
        eLastCommand
    };

    // изначальный обработчик всех команд
    void onCommandMessage(const Command command, const MessagePtr message);
    // обработка команды /info
    void onCommandInfo(const MessagePtr commandMessage);
    // обработка команды /report
    void onCommandReport(const MessagePtr commandMessage);
    // обработка команды /restart
    void onCommandRestart(const MessagePtr commandMessage);
    // обработка команды /allert, bEnable - true если ключаем, false если выключаем
    void onCommandAllert(const MessagePtr commandMessage, bool bEnable);

// парсинг колбэков
private:
    // отработка колбека
    void onCallbackQuery(const TgBot::CallbackQuery::Ptr query);

    // тип формируемого отчёта
    enum class ReportType : unsigned long
    {
        eAllChannels,       // все каналы для мониторинга
        eSpecialChannel     // выбранный канал
    };

    // Параметры формирования отчёта
    using CallBackParams = std::map<std::string, std::string>;
    // отработка колбэка отчёта
    void executeCallbackReport(const TgBot::CallbackQuery::Ptr query,
                               const CallBackParams& params);
    // отработка колбэка перестартовывания системы
    void executeCallbackRestart(const TgBot::CallbackQuery::Ptr query,
                                const CallBackParams& params);
    // отработка колбэка переправки сообщения остальным пользователям
    void executeCallbackResend(const TgBot::CallbackQuery::Ptr query,
                               const CallBackParams& params);
    // отработка колбэка включения/выключения оповещений
    void executeCallbackAllertion(const TgBot::CallbackQuery::Ptr query,
                                  const CallBackParams& params);

private:
    // поток работающего телеграма
    ITelegramThreadPtr m_telegramThread;
    // использующийся по умолчанию поток телеграмма
    ITelegramThreadPtr m_defaultTelegramThread;
    // текущие настройки бота
    TelegramBotSettings m_botSettings;
    // вспомогательный класс для работы с командами бота
    class CommandsHelper;
    std::shared_ptr<CommandsHelper> m_commandHelper = std::make_shared<CommandsHelper>();
    // данные о пользователях телеграмма
    ITelegramUsersListPtr m_telegramUsers;

// задания которые запускал бот
private:
    // структура с дополнительной информацией по заданию
    struct TaskInfo
    {
        // идентификатор чата из которого было получено задание
        int64_t chatId;
        // статус пользователя начавшего задание
        ITelegramUsersList::UserStatus userStatus;
    };
    // задания которые запускал телеграм бот
    std::map<TaskId, TaskInfo, TaskComparer> m_monitoringTasksInfo;

// список ошибок о которых оповещал бот
private:
    // структура с информацией об ошибках
    struct ErrorInfo
    {
    public:
        ErrorInfo(const MonitoringErrorEventData* errorData)
            : errorText(errorData->errorText)
            , errorGUID(errorData->errorGUID)
        {}

    public:
        // текст ошибки
        CString errorText;
        // флаг отправки ошибки обычным пользователям
        bool bResendToOrdinaryUsers = false;
        // время возникновения ошибки
        std::chrono::steady_clock::time_point timeOccurred = std::chrono::steady_clock::now();
        // идентификатор ошибки
        GUID errorGUID;
    };
    // Максимальное количество последних ошибок хранимых программой
    const size_t kMaxErrorInfoCount = 200;
    // ошибки которые возникали в мониторинге
    std::list<ErrorInfo> m_monitoringErrors;
};

////////////////////////////////////////////////////////////////////////////////
// вспомогательный клас для работы с командами телеграма
class CTelegramBot::CommandsHelper
{
public:
    // статус пользователей которым доступно использование команды
    typedef ITelegramUsersList::UserStatus AvailableStatus;
    CommandsHelper() = default;

public:
    // Добавление описания команды
    // @param command - идентификатор исполняемой команды
    // @param commandtext - текст команды
    // @param descr - описание команды
    // @param availabilityStatuses - перечень статусов пользователей которым доступна команда
    void addCommand(const CTelegramBot::Command command,
                    const CString& commandText, const CString& descr,
                    const std::vector<AvailableStatus>& availabilityStatuses);

    // Получить список команд с описанием для определенного пользователя
    CString getAvailableCommandsWithDescr(const AvailableStatus userStatus) const;

    // Проверка что надо ответить на команду пользователю
    // если false - в messageToUser будет ответ пользователю
    bool ensureNeedAnswerOnCommand(ITelegramUsersList* usersList,
                                   const CTelegramBot::Command command,
                                   const MessagePtr commandMessage,
                                   CString& messageToUser) const;

private:
    // Описание команды телеграм бота
    struct CommandDescription
    {
        CommandDescription(const CString& text, const CString& descr)
            : m_text(text) , m_description(descr)
        {}

        // тект который надо отправить для выполнения команды
        CString m_text;
        // описание команды
        CString m_description;
        // доступность команды для различных типов пользователей
        std::bitset<AvailableStatus::eLastStatus> m_availabilityForUsers;
    };

    // перечень команд бота
    //          команда            |  описание команды
    std::map<CTelegramBot::Command, CommandDescription> m_botCommands;
};

////////////////////////////////////////////////////////////////////////////////
// Класс для формирования колбэка для кнопок телеграма, на выходе UTF-8
class KeyboardCallback
{
public:
    // стандартный конструктор
    KeyboardCallback(const std::string& keyWord);
    KeyboardCallback(const KeyboardCallback& reportCallback);

    // добавить строку для колбэка с парой параметр - значение, Unicode
    KeyboardCallback& addCallbackParam(const std::string& param, const CString& value);
    // добавить строку для колбэка с парой параметр - значение, Unicode
    KeyboardCallback& addCallbackParam(const std::string& param, const std::wstring& value);
    // доабвить строку для колбэка с парой параметр - значение, UTF-8
    KeyboardCallback& addCallbackParam(const std::string& param, const std::string& value);
    // сформировать колбэк(UTF-8)
    std::string buildReport() const;

private:
    // строка отчёта
    CString m_reportStr;
};
