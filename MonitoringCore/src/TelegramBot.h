#pragma once

#include <afx.h>
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
    // возвращает спискок команд с подсказкой ввода в конце
    CString fillCommandListAndHint();
    // проверка возможности ответить на сообщение
    // если ответить нельзя - пользователю нужно вернуть messageToUser
    bool needAnswerOnMessage(const MessagePtr message, CString& messageToUser);
    // команда выполняемая при получении любого сообщения
    void onNonCommandMessage(const MessagePtr commandMessage);
    // обработка команды /info
    void onCommandInfo(const MessagePtr commandMessage);
    // обработка команды /report
    void onCommandReport(const MessagePtr commandMessage);

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

private:
    // поток работающего телеграма
    ITelegramThreadPtr m_telegramThread;
    // текущие настройки бота
    TelegramBotSettings m_botSettings;

    // перечень команд бота
    //   название команды  описание
    std::map<CString, CString> m_botCommands;

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
        ErrorInfo(const CString& text)
            : errorText(text)
        {
            // генерим идентификатор ошибки
            if (!SUCCEEDED(CoCreateGuid(&errorGUID)))
                assert(!"Не удалось создать гуид!");
        }
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
