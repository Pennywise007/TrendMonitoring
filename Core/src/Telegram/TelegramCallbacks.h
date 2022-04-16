#pragma once

#include <string>
#include <map>

#include <ext/std/memory.h>

#include <include/ITelegramUsersList.h>
#include <include/IMonitoringTasksService.h>
#include <TelegramThread.h>

namespace telegram::callback {

namespace report {
// Report callback parameters
// kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ÎÏÖÈÎÍÀËÜÍÎ) kParamInterval={'1000000'}
enum class ReportType : unsigned long
{
    eAllChannels,
    eSpecialChannel
};

const std::string kKeyWord              = R"(/report)";
    // Params
    const std::string kParamType        = "type";           // report type, may not be set if kParamChan is present
    const std::string kParamChan        = "chan";           // if not set - requested from the user
    const std::string kParamInterval    = "interval";
};

// Restart system callback parameters
namespace restart
{
    const std::string kKeyWord          = R"(/restart)";
};

// Resending massage callback parameters
// kKeyWord kParamid={'GUID'}
namespace resend
{
    const std::string kKeyWord          = R"(/resend)";
    // Params
    const std::string kParamId          = "errorId";        // error identifier in the error list (m_monitoringErrors)
};

// Notification callback parameters
// kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
namespace alertEnabling
{
    const std::string kKeyWord          = R"(/alert)";
    // Params
    const std::string kParamEnable      = "enable";         // eneble/disable notifications
    const std::string kParamChan        = "chan";           // channel name
    // Values
    const std::string kValueAllChannels = "allChannels";    // value that corresponds to turning off/on notifications on all channels
};

// Changing alarming value callback parameters
// kKeyWord kParamChan={'chan1'} kParamValue={'0.2'}
namespace alarmingValue
{
    const std::string kKeyWord          = R"(/alarmV)";
    // Params
    const std::string kParamChan        = "chan";           // the channel on which you want to set the alert level
    const std::string kParamValue       = "val";            // OPTIONAL new alert level value
}

class TelegramCallbacks
    : ext::events::ScopeSubscription<IMonitoringTaskEvent, IMonitoringErrorEvents>
{
public:
    explicit TelegramCallbacks(ITrendMonitoring::Ptr&& trendMonitoring,
                               std::shared_ptr<IMonitoringTasksService>&& monitoringTaskService,
                               std::shared_ptr<ITelegramThread>&& telegramThread,
                               std::shared_ptr<users::ITelegramUsersList>&& userList);

    // Report generation parameters
    using CallBackParams = std::map<std::string, std::string>;
// parsing callbacks
public:
    // callback processing
    void OnCallbackQuery(const TgBot::CallbackQuery::Ptr& query);
    // received a message as a response to the previous command to the bot
    bool GotResponseToPreviousCallback(const TgBot::Message::Ptr& commandMessage);

// IMonitoringTaskEvent
public:
    // Event about finishing monitoring task
    void OnCompleteTask(const TaskId& taskId, IMonitoringTaskEvent::ResultsPtrList monitoringResult) override;

// IMonitoringErrorEvents
private:
    void OnError(const std::shared_ptr<IMonitoringErrorEvents::EventData>& errorData) override;

// Processing callbacks for bot commands
private:
    void ExecuteCallbackReport      (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void ExecuteCallbackRestart     (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void ExecuteCallbackResend      (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void ExecuteCallbackAlert       (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void ExecuteCallbackAlarmValue  (const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);

private:
    // Map with callback keywords and their functions
    typedef void (TelegramCallbacks::*CommandCallback)(const TgBot::User::Ptr& from, const TgBot::Message::Ptr& message, const CallBackParams&, bool);
    std::map<std::string, CommandCallback> m_commandCallbacks;
    
    ITrendMonitoring::Ptr m_trendMonitoring;
    std::shared_ptr<IMonitoringTasksService> m_monitoringTaskService;
    std::shared_ptr<ITelegramThread> m_telegramThread;
    std::shared_ptr<users::ITelegramUsersList> m_telegramUsers;

// tasks that the bot started
private:
    // structure with additional information on the task
    struct TaskInfo
    {
        // identifier of the chat from which the task was received
        int64_t chatId;
        // status of the user who started the task
        users::ITelegramUsersList::UserStatus userStatus;
    };
    // tasks that were launched by the telegram bot
    std::map<TaskId, TaskInfo, ext::task::TaskIdHelper> m_monitoringTasksInfo;

// list of errors reported by the bot
private:
    // structure with error information
    struct ErrorInfo
    {
        explicit ErrorInfo(const IMonitoringErrorEvents::EventData* errorData) noexcept
            : errorText(errorData->errorTextForAllChannels)
            , errorGUID(errorData->errorGUID)
        {}

        // error text
        const std::wstring errorText;
        // flag for sending an error to ordinary users
        bool bResendToOrdinaryUsers = false;
        // error time
        const std::chrono::steady_clock::time_point timeOccurred = std::chrono::steady_clock::now();
        // error ID
        const GUID errorGUID;
    };
    // errors that occurred in monitoring
    std::list<ErrorInfo> m_monitoringErrors;
};

} // namespace telegram::callback