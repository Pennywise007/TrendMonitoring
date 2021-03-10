#include "pch.h"
#include "TelegramCommands.h"

#include <regex>

////////////////////////////////////////////////////////////////////////////////
// ������������ �������
KeyboardCallback::KeyboardCallback(const std::string& keyWord)
    : m_reportStr(keyWord.c_str())
{}

//----------------------------------------------------------------------------//
KeyboardCallback::KeyboardCallback(const KeyboardCallback& reportCallback)
    : m_reportStr(reportCallback.m_reportStr)
{}

//----------------------------------------------------------------------------//
KeyboardCallback& KeyboardCallback::addCallbackParam(const std::string& param,
                                                     const CString& value)
{
    return addCallbackParam(param, std::wstring(value.GetString()));
}

//----------------------------------------------------------------------------//
KeyboardCallback& KeyboardCallback::addCallbackParam(const std::string& param,
                                                     const std::wstring& value)
{
    // ������ ������ ���� ����
    // /%COMMAND% %PARAM_1%={'%VALUE_1%'} %PARAM_2%={'%VALUE_2%'} ...
    // ��������� ���� %PARAM_N%={'%VALUE_N'}
    m_reportStr.AppendFormat(L" %s={\'%s\'}", getUNICODEString(param).c_str(), value.c_str());

    return *this;
}

//----------------------------------------------------------------------------//
KeyboardCallback& KeyboardCallback::addCallbackParam(const std::string& param,
                                                     const std::string& value)
{
    return addCallbackParam(param, getUNICODEString(value));
}

//----------------------------------------------------------------------------//
std::string KeyboardCallback::buildReport() const
{
    // ������ ������������ ��������, ���� �� ������������ - �������� ��������
    const std::wregex escapedCharacters(LR"(['])");
    const std::wstring rep(LR"(\$&)");

    // ��-�� ���� ��� TgTypeParser::appendToJson ��� �� ��������� '}'
    // ���� ��������� �������� �������� '}' ��������� � ����� ������
    const std::wstring resultReport = std::regex_replace((m_reportStr + " ").GetString(), escapedCharacters, rep).c_str();

    // ������������ ����������� �� ������ ������� ��������� 64 �����
    /*constexpr int maxReportSize = 64;

    const auto result = getUtf8Str(resultReport);
    assert(result.length() <= maxReportSize);*/
    return getUtf8Str(resultReport);
}

////////////////////////////////////////////////////////////////////////////////
// ��������������� ���� ��� ������ � ��������� ���������
void CTelegramBot::CommandsHelper::addCommand(const CTelegramBot::Command command,
                                              const std::wstring& commandText, const std::wstring& descr,
                                              const std::vector<AvailableStatus>& availabilityStatuses)
{
    auto commandDescrIt = m_botCommands.try_emplace(command, commandText, descr);
    assert(commandDescrIt.second && "������� ��� ���������.");

    CommandDescription& commandDescr = commandDescrIt.first->second;
    for (const auto& status : availabilityStatuses)
        commandDescr.m_availabilityForUsers[status] = true;
}

//----------------------------------------------------------------------------//
std::wstring CTelegramBot::CommandsHelper::getAvailableCommandsWithDescr(const AvailableStatus userStatus) const
{
    std::wstring message;
    for (auto& command : m_botCommands)
    {
        if (command.second.m_availabilityForUsers[userStatus])
            message += L"/" + command.second.m_text + L" - " + command.second.m_description + L"\n";
    }

    if (!message.empty())
        message = L"�������������� ������� ����:\n\n\n" + message + L"\n\n��� ���� ����� �� ������������ ���������� �������� �� ����� ����(����������� ������������ ���� ����� ������� �������(/)!). ��� ������ � ���� ����, ��� ������ ��������������.";
    return message;
}

//----------------------------------------------------------------------------//
bool CTelegramBot::CommandsHelper::ensureNeedAnswerOnCommand(ITelegramUsersList* usersList,
                                                             const CTelegramBot::Command command,
                                                             const MessagePtr commandMessage,
                                                             std::wstring& messageToUser) const
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
        std::wstring availableCommands = getAvailableCommandsWithDescr(userStatus);
        if (availableCommands.empty())
        {
            if (userStatus == ITelegramUsersList::eNotAuthorized)
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