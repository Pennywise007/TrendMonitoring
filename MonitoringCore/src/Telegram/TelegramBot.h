#pragma once

#include <afx.h>
#include <memory>
#include <string>

#include <boost/core/noncopyable.hpp>

#include <include/ITelegramUsersList.h>
#include <include/ITrendMonitoring.h>

#include <TelegramDLL/TelegramThread.h>

#include "CommandsInfoService.h"
#include "TelegramCallbacks.h"

////////////////////////////////////////////////////////////////////////////////
// ����� ���������� �������� �����
class CTelegramBot
    : boost::noncopyable
{
public:
    CTelegramBot(const ITelegramUsersListPtr& telegramUsers, ITelegramThread* pDefaultTelegramThread = nullptr);
    ~CTelegramBot();

public:
    // ��������� �������� ����
    void setBotSettings(const TelegramBotSettings& botSettings);

    // ������� �������� ��������� ��������������� �������
    void sendMessageToAdmins(const CString& message) const;
    // ������� �������� ��������� ������������� �������
    void sendMessageToUsers(const CString& message) const;

// ��������� ������ ����
private:
    // �������� ������� �� �������
    void fillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                             CommandFunction& onUnknownCommand,
                             CommandFunction& onNonCommandMessage);
    // ��������� �� ������������������� ���������/�������
    void onNonCommandMessage(const MessagePtr& commandMessage);

    // ��������� ������ ������� �������� ���� ��������� ����������
    void onCommandMessage       (CommandsInfoService::Command command, const MessagePtr& message);
    void onCommandInfo          (const MessagePtr& commandMessage) const;
    void onCommandReport        (const MessagePtr& commandMessage) const;
    void onCommandRestart       (const MessagePtr& commandMessage) const;
    void onCommandAlert         (const MessagePtr& commandMessage, bool bEnable) const;
    void onCommandAlarmingValue (const MessagePtr& commandMessage) const;

private:
    // ����� ����������� ���������
    ITelegramThreadPtr m_telegramThread;
    // �������������� �� ��������� ����� ����������
    ITelegramThreadPtr m_defaultTelegramThread;
    // ������� ��������� ����
    TelegramBotSettings m_botSettings;
    // ������ � ������������� ����������
    ITelegramUsersListPtr m_telegramUsers;
    // ���������� �������� �� ���������
    callback::TelegramCallbacks m_callbacksHandler;
};
