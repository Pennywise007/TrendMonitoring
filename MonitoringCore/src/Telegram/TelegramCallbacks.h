#pragma once

#include <string>
#include <map>

#include <include/ITelegramUsersList.h>
#include <include/IMonitoringTasksService.h>
#include <TelegramDLL/TelegramThread.h>

namespace telegram::callback {

namespace report {
// параметры для колбэка отчёта
// kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}
// тип формируемого отчёта
enum class ReportType : unsigned long
{
    eAllChannels,       // все каналы для мониторинга
    eSpecialChannel     // выбранный канал
};

const std::string kKeyWord              = R"(/report)";     // ключевое слово
    // параметры
    const std::string kParamType        = "type";           // тип отчёта, может быть не задан если есть kParamChan
    const std::string kParamChan        = "chan";           // если не задан - запрашивается у пользователя
    const std::string kParamInterval    = "interval";       // интервал
};

// параметры для колбэка рестарта системы
namespace restart
{
    const std::string kKeyWord          = R"(/restart)";    // ключевое словоа
};

// параметры для колбэка пересылки сообщенияы
// kKeyWord kParamid={'GUID'}
namespace resend
{
    const std::string kKeyWord          = R"(/resend)";     // ключевое слово
    // параметры
    const std::string kParamId          = "errorId";        // идентификатор ошибки в списке ошибок(m_monitoringErrors)
};

// параметры для колбэка оповещения
// kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
namespace alertEnabling
{
    const std::string kKeyWord          = R"(/alert)";      // ключевое слово
    // параметры
    const std::string kParamEnable      = "enable";         // состояние включаемости/выключаемости
    const std::string kParamChan        = "chan";           // канал по которому нужно настроить нотификацию
    // значения
    const std::string kValueAllChannels = "allChannels";    // значение которое соответствует выключению оповещений по всем каналам
};

// параметры для колбэка изменения уровня оповещений
// kKeyWord kParamChan={'chan1'} kParamValue={'0.2'}
namespace alarmingValue
{
    const std::string kKeyWord          = R"(/alarmV)";     // ключевое слово
    // параметры
    const std::string kParamChan        = "chan";           // канал по которому нужно настроить уровень оповещений
    const std::string kParamValue       = "val";            // OPTIONAL новое значение уровня оповещений
}

class TelegramCallbacks
    : private EventRecipientImpl
{
public:
    explicit TelegramCallbacks(ITelegramThreadPtr& telegramThread, users::ITelegramUsersListPtr& userList);

    // Параметры формирования отчёта
    using CallBackParams = std::map<std::string, std::string>;
// парсинг колбэков
public:
    // отработка колбека
    void onCallbackQuery(const TgBot::CallbackQuery::Ptr& query);
    // получили сообщение как ответ на предыдущую команду боту
    bool gotResponseToPreviousCallback(const TgBot::Message::Ptr& commandMessage);

// IEventRecipient
private:
    // оповещение о произошедшем событии
    void onEvent(const EventId& code, float eventValue, const std::shared_ptr<IEventData>& eventData) override;

private:
    // Отрабатывание колбэков на команды бота
    void executeCallbackReport      (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackRestart     (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackResend      (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackAlert       (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackAlarmValue  (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);

private:
    // Мапа с ключевыми словами колбэков и их выполняемыми функциями
    typedef void (TelegramCallbacks::*CommandCallback)(const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams&, bool);
    std::map<std::string, CommandCallback> m_commandCallbacks;
    // поток работающего телеграма
    ITelegramThreadPtr& m_telegramThread;
    // данные о пользователях телеграмма
    users::ITelegramUsersListPtr& m_telegramUsers;

// задания которые запускал бот
private:
    // структура с дополнительной информацией по заданию
    struct TaskInfo
    {
        // идентификатор чата из которого было получено задание
        int64_t chatId;
        // статус пользователя начавшего задание
        users::ITelegramUsersList::UserStatus userStatus;
    };
    // задания которые запускал телеграм бот
    std::map<TaskId, TaskInfo, TaskComparer> m_monitoringTasksInfo;

// список ошибок о которых оповещал бот
private:
    // структура с информацией об ошибках
    struct ErrorInfo
    {
        explicit ErrorInfo(const MonitoringErrorEventData* errorData) noexcept
            : errorText(errorData->errorTextForAllChannels)
            , errorGUID(errorData->errorGUID)
        {}

        // текст ошибки
        const CString errorText;
        // флаг отправки ошибки обычным пользователям
        bool bResendToOrdinaryUsers = false;
        // время возникновения ошибки
        const std::chrono::steady_clock::time_point timeOccurred = std::chrono::steady_clock::now();
        // идентификатор ошибки
        const GUID errorGUID;
    };
    // ошибки которые возникали в мониторинге
    std::list<ErrorInfo> m_monitoringErrors;
};

} // namespace telegram::callback