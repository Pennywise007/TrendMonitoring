#pragma once

#include <afx.h>
#include <memory>
#include <string>

#include <boost/core/noncopyable.hpp>

#include <include/ITelegramUsersList.h>
#include <include/ITelegramBot.h>

#include <TelegramDLL/TelegramThread.h>

#include "CommandsInfoService.h"
#include "TelegramCallbacks.h"

namespace telegram::bot {

////////////////////////////////////////////////////////////////////////////////
// ����� ���������� �������� �����
class CTelegramBot
    : public ITelegramBot
    , boost::noncopyable
{
public:
    CTelegramBot(const users::ITelegramUsersListPtr& telegramUsers, ITelegramThread* pDefaultTelegramThread = nullptr);
    ~CTelegramBot();

// ITelegramBot
public:
    // ��������� �������� ����
    void setBotSettings(const TelegramBotSettings& botSettings) override;
    // ������� �������� ��������� ��������������� �������
    void sendMessageToAdmins(const CString& message) const override;
    // ������� �������� ��������� ������������� �������
    void sendMessageToUsers(const CString& message) const override;

// ��������� ������ ����
private:
    // �������� ������� �� �������
    void fillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                             CommandFunction& onUnknownCommand,
                             CommandFunction& onNonCommandMessage);
    // ��������� �� ������������������� ���������/�������
    void onNonCommandMessage(const MessagePtr& commandMessage);

    // ��������� ������ ������� �������� ���� ��������� ����������
    void onCommandMessage       (command::CommandsInfoService::Command command, const MessagePtr& message);
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
    users::ITelegramUsersListPtr m_telegramUsers;
    // ���������� �������� �� ���������
    callback::TelegramCallbacks m_callbacksHandler;
};

} // namespace telegram::bot