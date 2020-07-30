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
// ����� ���������� �������� �����
class CTelegramBot
    : public ITelegramAllerter
    , public EventRecipientImpl
{
public:
    CTelegramBot();
    virtual ~CTelegramBot();

public:
    // ������������� ����
    void initBot(ITelegramUsersListPtr telegramUsers);
    // ��������� �������� ����
    void setBotSettings(const TelegramBotSettings& botSettings);

    // ������� �������� ��������� ��������������� �������
    void sendMessageToAdmins(const CString& message);
    // ������� �������� ��������� ������������� �������
    void sendMessageToUsers(const CString& message);

// ITelegramAllerter
public:
    void onAllertFromTelegram(const CString& allertMessage) override;
// IEventRecipient
public:
    // ���������� � ������������ �������
    void onEvent(const EventId& code, float eventValue,
                 std::shared_ptr<IEventData> eventData) override;

// ��������� ������ ����
private:
    // �������� ������� �� �������
    void fillCommandHandlers(std::map<std::string, CommandFunction>& commandsList,
                             CommandFunction& onUnknownCommand,
                             CommandFunction& onNonCommandMessage);
    // ���������� ������� ������ � ���������� ����� � �����
    CString fillCommandListAndHint();
    // �������� ����������� �������� �� ���������
    // ���� �������� ������ - ������������ ����� ������� messageToUser
    bool needAnswerOnMessage(const MessagePtr message, CString& messageToUser);
    // ������� ����������� ��� ��������� ������ ���������
    void onNonCommandMessage(const MessagePtr commandMessage);
    // ��������� ������� /info
    void onCommandInfo(const MessagePtr commandMessage);
    // ��������� ������� /report
    void onCommandReport(const MessagePtr commandMessage);

// ������� ��������
private:
    // ��������� �������
    void onCallbackQuery(const TgBot::CallbackQuery::Ptr query);

    // ��� ������������ ������
    enum class ReportType : unsigned long
    {
        eAllChannels,       // ��� ������ ��� �����������
        eSpecialChannel     // ��������� �����
    };

    // ��������� ������������ ������
    using CallBackParams = std::map<std::string, std::string>;
    // ��������� ������� ������
    void executeCallbackReport(const TgBot::CallbackQuery::Ptr query,
                               const CallBackParams& params);
    // ��������� ������� ����������������� �������
    void executeCallbackRestart(const TgBot::CallbackQuery::Ptr query,
                                const CallBackParams& params);
    // ��������� ������� ���������� ��������� ��������� �������������
    void executeCallbackResend(const TgBot::CallbackQuery::Ptr query,
                               const CallBackParams& params);

private:
    // ����� ����������� ���������
    ITelegramThreadPtr m_telegramThread;
    // ������� ��������� ����
    TelegramBotSettings m_botSettings;

    // �������� ������ ����
    //   �������� �������  ��������
    std::map<CString, CString> m_botCommands;

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
        ErrorInfo(const CString& text)
            : errorText(text)
        {
            // ������� ������������� ������
            if (!SUCCEEDED(CoCreateGuid(&errorGUID)))
                assert(!"�� ������� ������� ����!");
        }
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
    KeyboardCallback(const std::string& keyWord);
    KeyboardCallback(const KeyboardCallback& reportCallback);

    // �������� ������ ��� ������� � ����� �������� - ��������, Unicode
    KeyboardCallback& addCallbackParam(const std::string& param, const CString& value);
    // �������� ������ ��� ������� � ����� �������� - ��������, Unicode
    KeyboardCallback& addCallbackParam(const std::string& param, const std::wstring& value);
    // �������� ������ ��� ������� � ����� �������� - ��������, UTF-8
    KeyboardCallback& addCallbackParam(const std::string& param, const std::string& value);
    // ������������ ������(UTF-8)
    std::string buildReport() const;

private:
    // ������ ������
    CString m_reportStr;
};
