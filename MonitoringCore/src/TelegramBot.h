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
// ����� ���������� �������� �����
class CTelegramBot
    : private EventRecipientImpl
{
public:
    CTelegramBot();
    virtual ~CTelegramBot();

public:
    // ������������� ����
    void initBot(ITelegramUsersListPtr telegramUsers);
    // ��������� ������������ ������ ��������� �� ���������
    void setDefaultTelegramThread(ITelegramThreadPtr& pTelegramThread);
    // ��������� �������� ����
    void setBotSettings(const TelegramBotSettings& botSettings);

    // ������� �������� ��������� ��������������� �������
    void sendMessageToAdmins(const CString& message) const;
    // ������� �������� ��������� ������������� �������
    void sendMessageToUsers(const CString& message) const;

// IEventRecipient
private:
    // ���������� � ������������ �������
    void onEvent(const EventId& code, float eventValue,
                 std::shared_ptr<IEventData> eventData) override;

// ��������� ������ ����
private:
    // �������� ������� �� �������
    void fillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                             CommandFunction& onUnknownCommand,
                             CommandFunction& onNonCommandMessage);
    // ��������� �� ������������������� ���������/�������
    void onNonCommandMessage(const MessagePtr& commandMessage);

    // �������� ������
    enum class Command
    {
        eUnknown,           // ����������� ������� ����
        eInfo,              // ������ ���������� ����
        eReport,            // ������ ������ �� ������
        eRestart,           // ���������� �����������
        eAlertingOn,        // ���������� � �������� ������ ��������
        eAlertingOff,       // ���������� � �������� ������ ���������
        eAlarmingValue,     // ��������� ����������� ������ �������� ��� ������
        // ��������� �������
        eLastCommand
    };

    // ��������� ������ ������� �������� ���� ��������� ����������
    void onCommandMessage       (const Command command, const MessagePtr& message);
    void onCommandInfo          (const MessagePtr& commandMessage) const;
    void onCommandReport        (const MessagePtr& commandMessage) const;
    void onCommandRestart       (const MessagePtr& commandMessage) const;
    void onCommandAlert         (const MessagePtr& commandMessage, bool bEnable) const;
    void onCommandAlarmingValue (const MessagePtr& commandMessage) const;

// ������� ��������
private:
    // ��������� �������
    void onCallbackQuery(const TgBot::CallbackQuery::Ptr& query);
    // �������� ��������� ��� ����� �� ���������� ������� ����
    bool gotResponseToPreviousCallback(const MessagePtr& commandMessage);

    // ��� ������������ ������
    enum class ReportType : unsigned long
    {
        eAllChannels,       // ��� ������ ��� �����������
        eSpecialChannel     // ��������� �����
    };

    // ��������� ������������ ������
    using CallBackParams = std::map<std::string, std::string>;

    // ������������� ��������������� ������ ������ ���������
    void executeCallbackReport      (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackRestart     (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackResend      (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackAlert       (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    void executeCallbackAlarmValue  (const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer);
    // ���� � ��������� ������� �������� � �� ������������ ���������
    typedef void (CTelegramBot::*CommandCallback)(const TgBot::Message::Ptr&, const CallBackParams&, bool);
    std::map<std::string, CommandCallback> m_commandCallbacks;

private:
    // ����� ����������� ���������
    ITelegramThreadPtr m_telegramThread;
    // �������������� �� ��������� ����� ����������
    ITelegramThreadPtr m_defaultTelegramThread;
    // ������� ��������� ����
    TelegramBotSettings m_botSettings;
    // ��������������� ����� ��� ������ � ��������� ����
    class CommandsHelper;
    std::shared_ptr<CommandsHelper> m_commandHelper;
    // ������ � ������������� ����������
    ITelegramUsersListPtr m_telegramUsers;

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
    public:
        ErrorInfo(const MonitoringErrorEventData* errorData)
            : errorText(errorData->errorText)
            , errorGUID(errorData->errorGUID)
        {}

    public:
        // ����� ������
        CString errorText;
        // ���� �������� ������ ������� �������������
        bool bResendToOrdinaryUsers = false;
        // ����� ������������� ������
        std::chrono::steady_clock::time_point timeOccurred = std::chrono::steady_clock::now();
        // ������������� ������
        GUID errorGUID;
    };
    // ������������ ���������� ��������� ������ �������� ����������
    const size_t kMaxErrorInfoCount = 200;
    // ������ ������� ��������� � �����������
    std::list<ErrorInfo> m_monitoringErrors;
};

////////////////////////////////////////////////////////////////////////////////
// ����� ��� ������������ ������� ��� ������ ���������, �� ������ UTF-8
class KeyboardCallback
{
public:
    // ����������� �����������
    explicit KeyboardCallback(const std::string& keyWord);
    explicit KeyboardCallback(const KeyboardCallback& reportCallback);

    // �������� ������ ��� ������� � ����� �������� - ��������, Unicode
    KeyboardCallback& addCallbackParam(const std::string& param, const CString& value);
    KeyboardCallback& addCallbackParam(const std::string& param, const std::wstring& value);
    // �������� ������ ��� ������� � ����� �������� - ��������, UTF-8
    KeyboardCallback& addCallbackParam(const std::string& param, const std::string& value);
    // ������������ ������(UTF-8)
    std::string buildReport() const;

private:
    // ������ ������
    CString m_reportStr;
};