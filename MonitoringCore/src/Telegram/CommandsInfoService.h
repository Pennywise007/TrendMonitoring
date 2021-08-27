#pragma once

#include <bitset>
#include <string>
#include <vector>

#include <include/ITelegramUsersList.h>
#include <TelegramDLL/TelegramThread.h>

#include "Singleton.h"
#include "TelegramCallbacks.h"

namespace telegram::command {

// ���������� ������� �� �������
void execute_restart_command(const TgBot::Message::Ptr& message, ITelegramThread* telegramThread);

////////////////////////////////////////////////////////////////////////////////
// ��������������� ���� ��� ������ � ��������� ���������
class CommandsInfoService
{
    friend class CSingleton<CommandsInfoService>;
public:
    // ������ ������������� ������� �������� ������������� �������
    typedef users::ITelegramUsersList::UserStatus AvailableStatus;

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

public:
    // ���������� �������� �������
    // @param command - ������������� ����������� �������
    // @param commandtext - ����� �������
    // @param description - �������� �������
    // @param callbacksKeyWords - �������� �������� ���� � �������� ������� ��������� ��������
    // @param availabilityStatuses - �������� �������� ������������� ������� �������� �������
    void addCommand(Command command, std::wstring&& commandText, std::wstring&& description,
                    std::set<std::string>&& callbacksKeyWords,
                    const std::vector<AvailableStatus>& availabilityStatuses);

    // �������� ������ ������ � ��������� ��� ������������� ������������
    std::wstring getAvailableCommandsWithDescription(AvailableStatus userStatus) const;

    // �������� ��� ���� �������� �� ������� ������������
    // ���� false - � messageToUser ����� ����� ������������
    bool ensureNeedAnswerOnCommand(users::ITelegramUsersList* usersList, Command command,
                                   const MessagePtr& commandMessage, std::wstring& messageToUser) const;

    // �������� ��� ���� �������� �� ������, ���� ������������ ���������� ������ � ���� ��� ���������� ������
    // ���� false - � messageToUser ����� ����� ������������
    bool ensureNeedAnswerOnCallback(users::ITelegramUsersList* usersList,
                                    const std::string& callbackKeyWord,
                                    const MessagePtr& commandMessage) const;

    // ������� ������ ������
    void resetCommandList();

private:
    // �������� ������� �������� ����
    struct CommandDescription
    {
        CommandDescription(std::wstring&& text, std::wstring&& descr, std::set<std::string>&& callbacksKeyWord)
            : m_text(std::move(text)), m_description(std::move(descr)), m_callbacksKeywords(std::move(callbacksKeyWord))
        {}

        // ���� ������� ���� ��������� ��� ���������� �������
        std::wstring m_text;
        // �������� �������
        std::wstring m_description;
        // �������� �������� ���� � �������� �������
        std::set<std::string> m_callbacksKeywords;
        // ����������� ������� ��� ��������� ����� �������������
        std::bitset<AvailableStatus::eLastStatus> m_availabilityForUsers;
    };

    // �������� ������ ����
    //       ������� | �������� �������
    std::map<Command, CommandDescription> m_botCommands;
};

} // namespace telegram::command