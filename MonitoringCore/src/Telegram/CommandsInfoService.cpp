#include "pch.h"

#include <atlstr.h>
#include <filesystem>

#include "CommandsInfoService.h"

#include <boost/scope_exit.hpp>

#include "DirsService.h"
#include "../Utils.h"

namespace telegram::command {

////////////////////////////////////////////////////////////////////////////////
// ���������� ������� �� �������
void execute_restart_command(const TgBot::Message::Ptr& message, ITelegramThread* telegramThread)
{
    // ���������� ������� ������ ����� ������ ��� ��� ���� ����� ����� ������� ��� ������ ������
    CString batFullPath = get_service<DirsService>().getExeDir() + kRestartSystemFileName;

    // ��������� ������������
    CString messageToUser;
    if (std::filesystem::is_regular_file(batFullPath.GetString()))
    {
        telegramThread->sendMessage(message->chat->id, L"���������� ������� ��������������.");

        // ��������� ������
        STARTUPINFO cif = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION m_ProcInfo = { nullptr };

        BOOST_SCOPE_EXIT(&batFullPath)
        {
            batFullPath.ReleaseBuffer();
        } BOOST_SCOPE_EXIT_END;

        if (FALSE != CreateProcess(batFullPath.GetBuffer(),     // ��� ������������ ������
            nullptr,	                    // ��������� ������
            NULL,                        // ��������� �� ��������� SECURITY_ATTRIBUTES
            NULL,                        // ��������� �� ��������� SECURITY_ATTRIBUTES
            0,                           // ���� ������������ �������� ��������
            NULL,                        // ����� �������� �������� ��������
            NULL,                        // ��������� �� ���� �����
            NULL,                        // ������� ���� ��� �������
            &cif,                        // ��������� �� ��������� STARTUPINFO
            &m_ProcInfo))                // ��������� �� ��������� PROCESS_INFORMATION)
        {	// ������������� ������ �� �����
            CloseHandle(m_ProcInfo.hThread);
            CloseHandle(m_ProcInfo.hProcess);
        }
    }
    else
        telegramThread->sendMessage(message->chat->id, L"���� ��� ����������� �� ������.");
}

////////////////////////////////////////////////////////////////////////////////
// ��������������� ���� ��� ������ � ��������� ���������
void CommandsInfoService::addCommand(Command command, std::wstring&& commandText, std::wstring&& description,
                                     std::set<std::string>&& callbacksKeyWords,
                                     const std::vector<AvailableStatus>& availabilityStatuses)
{
    const auto commandDescrIt = m_botCommands.try_emplace(command, std::move(commandText),
                                                          std::move(description), std::move(callbacksKeyWords));
    assert(commandDescrIt.second && "������� ��� ���������.");

    CommandDescription& commandDescr = commandDescrIt.first->second;
    for (const auto& status : availabilityStatuses)
        commandDescr.m_availabilityForUsers[status] = true;
}

//----------------------------------------------------------------------------//
std::wstring CommandsInfoService::getAvailableCommandsWithDescription(AvailableStatus userStatus) const
{
    std::wstring message;
    for (const auto& command : m_botCommands)
    {
        if (command.second.m_availabilityForUsers[userStatus])
            message += L"/" + command.second.m_text + L" - " + command.second.m_description + L"\n";
    }

    if (!message.empty())
        message = L"�������������� ������� ����:\n\n\n" + message + L"\n\n��� ���� ����� �� ������������ ���������� �������� �� ����� ����(����������� ������������ ���� ����� ������� �������(/)!). ��� ������ � ���� ����, ��� ������ ��������������.";
    return message;
}

//----------------------------------------------------------------------------//
bool CommandsInfoService::ensureNeedAnswerOnCommand(users::ITelegramUsersList* usersList, Command command,
                                                    const MessagePtr& commandMessage, std::wstring& messageToUser) const
{
    // ������������ ����������� ���������
    const TgBot::User::Ptr& pUser = commandMessage->from;

    // ���������� ��� ���� ����� ������������
    usersList->ensureExist(pUser, commandMessage->chat->id);
    // �������� ������ ������������
    const auto userStatus = usersList->getUserStatus(pUser);
    // ��������� ��� ������������ ����� ������������ �������
    const auto commandIt = m_botCommands.find(command);
    if (commandIt == m_botCommands.end() ||
        !commandIt->second.m_availabilityForUsers[userStatus])
    {
        // ��������� ����� ������������ �� ������� ��������� ��� ������
        std::wstring availableCommands = getAvailableCommandsWithDescription(userStatus);
        if (availableCommands.empty())
        {
            if (userStatus == users::ITelegramUsersList::eNotAuthorized)
                messageToUser = L"��� ������ ���� ��� ���������� ��������������.";
            else
                messageToUser = L"����������� �������. � ��� ��� ��������� ������, ���������� � ��������������";
        }
        else
            messageToUser = L"����������� �������. " + std::move(availableCommands);
        assert(!messageToUser.empty());

        return false;
    }

    return true;
}

//----------------------------------------------------------------------------//
bool CommandsInfoService::ensureNeedAnswerOnCallback(users::ITelegramUsersList* usersList,
                                                     const std::string& callbackKeyWord,
                                                     const MessagePtr& commandMessage) const
{
    // ������������ ����������� ���������
    const TgBot::User::Ptr& pUser = commandMessage->from;

    for (auto&&[command, commandInfo] : m_botCommands)
    {
        if (auto callbackKeywordIt = commandInfo.m_callbacksKeywords.find(callbackKeyWord);
            callbackKeywordIt != commandInfo.m_callbacksKeywords.end())
        {
            // ���������� ��� ���� ����� ������������
            usersList->ensureExist(pUser, commandMessage->chat->id);
            // ���������� ��� ������������ ����� ���������� ��� �������/������������ �������
            if (commandInfo.m_availabilityForUsers[usersList->getUserStatus(pUser)])
                return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------//
void CommandsInfoService::resetCommandList()
{
    m_botCommands.clear();
}

} // namespace telegram::command