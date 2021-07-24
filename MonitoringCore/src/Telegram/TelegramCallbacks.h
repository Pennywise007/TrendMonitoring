#pragma once

#include <string>

#include <include/ITelegramUsersList.h>
#include <include/IMonitoringTasksService.h>
#include <TelegramDLL/TelegramThread.h>

namespace callback
{
// ��������� ��� ������� ������
// kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(�����������) kParamInterval={'1000000'}
namespace report
{
    // ��� ������������ ������
    enum class ReportType : unsigned long
    {
        eAllChannels,       // ��� ������ ��� �����������
        eSpecialChannel     // ��������� �����
    };

const std::string kKeyWord              = R"(/report)";     // �������� �����
    // ���������
    const std::string kParamType        = "type";           // ��� ������
    const std::string kParamChan        = "chan";           // ����� �� �������� ����� �����
    const std::string kParamInterval    = "interval";       // ��������
};

// ��������� ��� ������� �������� �������
namespace restart
{
    const std::string kKeyWord          = R"(/restart)";    // �������� ������
};

// ��������� ��� ������� ��������� ����������
// kKeyWord kParamid={'GUID'}
namespace resend
{
    const std::string kKeyWord          = R"(/resend)";     // �������� �����
    // ���������
    const std::string kParamId          = "errorId";        // ������������� ������ � ������ ������(m_monitoringErrors)
};

// ��������� ��� ������� ����������
// kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
namespace alertEnabling
{
    const std::string kKeyWord          = R"(/alert)";      // �������� �����
    // ���������
    const std::string kParamEnable      = "enable";         // ��������� ������������/�������������
    const std::string kParamChan        = "chan";           // ����� �� �������� ����� ��������� �����������
    // ��������
    const std::string kValueAllChannels = "allChannels";    // �������� ������� ������������� ���������� ���������� �� ���� �������
};

// ��������� ��� ������� ��������� ������ ����������
// kKeyWord kParamChan={'chan1'} kParamValue={'0.2'}
namespace alarmingValue
{
    const std::string kKeyWord          = R"(/alarmV)";     // �������� �����
    // ���������
    const std::string kParamChan        = "chan";           // ����� �� �������� ����� ��������� ������� ����������
    const std::string kParamValue       = "val";            // ����� �������� ������ ����������
}

class TelegramCallbacks
    : private EventRecipientImpl
{
public:
    explicit TelegramCallbacks(ITelegramThreadPtr& telegramThread, ITelegramUsersListPtr& userList);

    // ��������� ������������ ������
    using CallBackParams = std::map<std::string, std::string>;
// ������� ��������
public:
    // ��������� �������
    void onCallbackQuery(const TgBot::CallbackQuery::Ptr& query);
    // �������� ��������� ��� ����� �� ���������� ������� ����
    bool gotResponseToPreviousCallback(const TgBot::Message::Ptr& commandMessage);

// IEventRecipient
private:
    // ���������� � ������������ �������
    void onEvent(const EventId& code, float eventValue, std::shared_ptr<IEventData> eventData) override;

private:
    // ������������� �������� �� ������� ����
    void executeCallbackReport      (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackRestart     (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackResend      (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackAlert       (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackAlarmValue  (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);

private:
    // ���� � ��������� ������� �������� � �� ������������ ���������
    typedef void (TelegramCallbacks::*CommandCallback)(const TgBot::Message::Ptr&, const CallBackParams&, bool);
    std::map<std::string, CommandCallback> m_commandCallbacks;
    // ����� ����������� ���������
    ITelegramThreadPtr& m_telegramThread;
    // ������ � ������������� ����������
    ITelegramUsersListPtr& m_telegramUsers;

// ������� ������� �������� ���
private:
    // ��������� � �������������� ����������� �� �������
    struct TaskInfo
    {
        // ������������� ���� �� �������� ���� �������� �������
        int64_t chatId;
        // ������ ������������ ��������� �������
        ITelegramUsersList::UserStatus userStatus;
    };
    // ������� ������� �������� �������� ���
    std::map<TaskId, TaskInfo, TaskComparer> m_monitoringTasksInfo;

// ������ ������ � ������� �������� ���
private:
    // ��������� � ����������� �� �������
    struct ErrorInfo
    {
        explicit ErrorInfo(const MonitoringErrorEventData* errorData) noexcept
            : errorText(errorData->errorText)
            , errorGUID(errorData->errorGUID)
        {}

        // ����� ������
        const CString errorText;
        // ���� �������� ������ ������� �������������
        bool bResendToOrdinaryUsers = false;
        // ����� ������������� ������
        const std::chrono::steady_clock::time_point timeOccurred = std::chrono::steady_clock::now();
        // ������������� ������
        const GUID errorGUID;
    };
    // ������ ������� ��������� � �����������
    std::list<ErrorInfo> m_monitoringErrors;
};

} // namespace callback