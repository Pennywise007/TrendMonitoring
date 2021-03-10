#pragma once

#include "TelegramBot.h"

////////////////////////////////////////////////////////////////////////////////
// ��������������� ���� ��� ������ � ��������� ���������
class CTelegramBot::CommandsHelper
{
public:
    // ������ ������������� ������� �������� ������������� �������
    typedef ITelegramUsersList::UserStatus AvailableStatus;
    CommandsHelper() = default;

public:
    // ���������� �������� �������
    // @param command - ������������� ����������� �������
    // @param commandtext - ����� �������
    // @param descr - �������� �������
    // @param availabilityStatuses - �������� �������� ������������� ������� �������� �������
    void addCommand(const CTelegramBot::Command command,
                    const std::wstring& commandText, const std::wstring& descr,
                    const std::vector<AvailableStatus>& availabilityStatuses);

    // �������� ������ ������ � ��������� ��� ������������� ������������
    std::wstring getAvailableCommandsWithDescr(const AvailableStatus userStatus) const;

    // �������� ��� ���� �������� �� ������� ������������
    // ���� false - � messageToUser ����� ����� ������������
    bool ensureNeedAnswerOnCommand(ITelegramUsersList* usersList,
                                   const CTelegramBot::Command command,
                                   const MessagePtr commandMessage,
                                   std::wstring& messageToUser) const;

private:
    // �������� ������� �������� ����
    struct CommandDescription
    {
        CommandDescription(std::wstring text, std::wstring descr)
            : m_text(std::move(text)) , m_description(std::move(descr))
        {}

        // ���� ������� ���� ��������� ��� ���������� �������
        std::wstring m_text;
        // �������� �������
        std::wstring m_description;
        // ����������� ������� ��� ��������� ����� �������������
        std::bitset<AvailableStatus::eLastStatus> m_availabilityForUsers;
    };

    // �������� ������ ����
    //          �������            |  �������� �������
    std::map<CTelegramBot::Command, CommandDescription> m_botCommands;
};

