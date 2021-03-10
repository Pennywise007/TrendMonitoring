#pragma once

#include <afx.h>
#include <map>
#include <memory>
#include <string>

#include <TelegramDLL/TelegramThread.h>

#include "IMonitoringTasksService.h"
#include "ITelegramUsersList.h"
#include "ITrendMonitoring.h"

////////////////////////////////////////////////////////////////////////////////
// класс управления телеграм ботом
class CTelegramBot
    : private EventRecipientImpl
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
    void sendMessageToAdmins(const CString& message) const;
    // функция отправки сообщения пользователям системы
    void sendMessageToUsers(const CString& message) const;

// IEventRecipient
private:
    // оповещение о произошедшем событии
    void onEvent(const EventId& code, float eventValue,
                 std::shared_ptr<IEventData> eventData) override;

// Отработка команд бота
private:
    // добавить реакции на команды
    void fillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                             CommandFunction& onUnknownCommand,
                             CommandFunction& onNonCommandMessage);
    // отработка не зарегистрированного сообщения/команды
    void onNonCommandMessage(const MessagePtr& commandMessage);

    // Перечень команд
    enum class Command
    {
        eUnknown,           // Неизвестная комнада бота
        eInfo,              // Запрос информации бота
        eReport,            // Запрос отчёта за период
        eRestart,           // перезапуск мониторинга
        eAlertingOn,        // оповещения о событиях канала включить
        eAlertingOff,       // оповещения о событиях канала выключить
        eAlarmingValue,     // изменение допустимого уровня значений для канала
        // Последняя команда
        eLastCommand
    };

    // Обработка команд которые приходят боту отдельным сообщением
    void onCommandMessage       (const Command command, const MessagePtr& message);
    void onCommandInfo          (const MessagePtr& commandMessage) const;
    void onCommandReport        (const MessagePtr& commandMessage) const;
    void onCommandRestart       (const MessagePtr& commandMessage) const;
    void onCommandAlert         (const MessagePtr& commandMessage, bool bEnable) const;
    void onCommandAlarmingValue (const MessagePtr& commandMessage) const;

// парсинг колбэков
private:
    // отработка колбека
    void onCallbackQuery(const TgBot::CallbackQuery::Ptr& query);
    // получили сообщение как ответ на предыдущую команду боту
    bool gotResponseToPreviousCallback(const MessagePtr& commandMessage);

    // тип формируемого отчёта
    enum class ReportType : unsigned long
    {
        eAllChannels,       // все каналы для мониторинга
        eSpecialChannel     // выбранный канал
    };

    // Параметры формирования отчёта
    using CallBackParams = std::map<std::string, std::string>;

    // Отрабатывание программируемых кнопок самого телеграма
    void executeCallbackReport      (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackRestart     (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackResend      (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackAlert       (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackAlarmValue  (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    // Мапа с ключевыми словами колбэков и их выполняемыми функциями
    typedef void (CTelegramBot::*CommandCallback)(const TgBot::Message::Ptr&, const CallBackParams&, bool);
    std::map<std::string, CommandCallback> m_commandCallbacks;

private:
    // поток работающего телеграма
    ITelegramThreadPtr m_telegramThread;
    // использующийся по умолчанию поток телеграмма
    ITelegramThreadPtr m_defaultTelegramThread;
    // текущие настройки бота
    TelegramBotSettings m_botSettings;
    // вспомогательный класс для работы с командами бота
    class CommandsHelper;
    std::shared_ptr<CommandsHelper> m_commandHelper;
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
// Класс для формирования колбэка для кнопок телеграма, на выходе UTF-8
class KeyboardCallback
{
public:
    // стандартный конструктор
    explicit KeyboardCallback(const std::string& keyWord);
    explicit KeyboardCallback(const KeyboardCallback& reportCallback);

    // добавить строку для колбэка с парой параметр - значение, Unicode
    KeyboardCallback& addCallbackParam(const std::string& param, const CString& value);
    KeyboardCallback& addCallbackParam(const std::string& param, const std::wstring& value);
    // доабвить строку для колбэка с парой параметр - значение, UTF-8
    KeyboardCallback& addCallbackParam(const std::string& param, const std::string& value);
    // сформировать колбэк(UTF-8)
    std::string buildReport() const;

private:
    // строка отчёта
    CString m_reportStr;
};