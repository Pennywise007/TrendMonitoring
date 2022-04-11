#include "pch.h"

#include <atlstr.h>
#include <filesystem>

#include "CommandsInfoService.h"

#include <ext/scope/on_exit.h>
#include <ext/std/filesystem.h>

#include "../Utils.h"

namespace telegram::command {

// execute command to restart
void execute_restart_command(const std::int64_t& chatId, ITelegramThread* telegramThread)
{
    // restarting the system is done through a batch file, since there is a lot to do for different systems
    CString batFullPath = std::filesystem::get_exe_directory().append(kRestartSystemFileName).c_str();

    // message to the user
    std::wstring messageToUser;
    if (std::filesystem::is_regular_file(batFullPath.GetString()))
    {
        telegramThread->SendMessage(chatId, L"Перезапуск системы осуществляется.");

        // run batch file
        STARTUPINFO cif = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION m_ProcInfo = { nullptr };

        EXT_SCOPE_ON_EXIT_F((&batFullPath),
        {
            batFullPath.ReleaseBuffer();
        });

        if (FALSE != CreateProcess(batFullPath.GetBuffer(), // executable module name
            nullptr,                                        // Command line
            NULL,                                           // Pointer to the SECURITY_ATTRIBUTES structure
            NULL,                                           // Pointer to the SECURITY_ATTRIBUTES structure
            0,                                              // Current process inheritance flag
            NULL,                                           // Flags of process creation methods
            NULL,                                           // Pointer to environment block
            NULL,                                           // Current drive or directory
            &cif,                                           // Pointer to STARTUPINFO structure
            &m_ProcInfo))                                   // Pointer to PROCESS_INFORMATION structure)
        {	// идентификатор потока не нужен
            CloseHandle(m_ProcInfo.hThread);
            CloseHandle(m_ProcInfo.hProcess);
        }
    }
    else
        telegramThread->SendMessage(chatId, L"Файл для перезапуска не найден.");
}

////////////////////////////////////////////////////////////////////////////////
// вспомогательный клас для работы с командами телеграма
void CommandsInfoService::AddCommand(Command command, std::wstring&& commandText, std::wstring&& description,
                                     std::set<std::string>&& callbacksKeyWords,
                                     const std::vector<AvailableStatus>& availabilityStatuses)
{
    const auto commandDescrIt = m_botCommands.try_emplace(command, std::move(commandText),
                                                          std::move(description), std::move(callbacksKeyWords));
    EXT_ASSERT(commandDescrIt.second && "Команда уже добавлена.");

    CommandDescription& commandDescr = commandDescrIt.first->second;
    for (const auto& status : availabilityStatuses)
        commandDescr.m_availabilityForUsers[status] = true;
}

//----------------------------------------------------------------------------//
std::wstring CommandsInfoService::GetAvailableCommandsWithDescription(AvailableStatus userStatus) const
{
    std::wstring message;
    for (const auto& command : m_botCommands)
    {
        if (command.second.m_availabilityForUsers[userStatus])
            message += L"/" + command.second.m_text + L" - " + command.second.m_description + L"\n";
    }

    if (!message.empty())
        message = L"Поддерживаемые команды бота:\n\n\n" + message + L"\n\nДля того чтобы их использовать необходимо написать их этому боту(обязательно использовать слэш перед текстом команды(/)!). Или нажать в этом окне, они должны подсвечиваться.";
    return message;
}

//----------------------------------------------------------------------------//
bool CommandsInfoService::EnsureNeedAnswerOnCommand(users::ITelegramUsersList& usersList, Command command,
                                                    const TgBot::User::Ptr& from, const std::int64_t& chatId, std::wstring& messageToUser) const
{
    // make sure there is such a user
    usersList.EnsureExist(from, chatId);
    // get user status
    const auto userStatus = usersList.GetUserStatus(from);
    // check if the user can use the command
    const auto commandIt = m_botCommands.find(command);
    if (commandIt == m_botCommands.end() ||
        !commandIt->second.m_availabilityForUsers[userStatus])
    {
        // form a response to the user with a list of commands available to him
        std::wstring availableCommands = GetAvailableCommandsWithDescription(userStatus);
        if (availableCommands.empty())
        {
            if (userStatus == users::ITelegramUsersList::eNotAuthorized)
                messageToUser = L"Для работы бота вам необходимо авторизоваться.";
            else
                messageToUser = L"Неизвестная команда. У вас нет доступных команд, обратитесь к администратору";
        }
        else
            messageToUser = L"Неизвестная команда. " + std::move(availableCommands);
        EXT_ASSERT(!messageToUser.empty());

        return false;
    }

    return true;
}

//----------------------------------------------------------------------------//
bool CommandsInfoService::EnsureNeedAnswerOnCallback(users::ITelegramUsersList& usersList,
                                                     const std::string& callbackKeyWord,
                                                     const TgBot::CallbackQuery::Ptr& query) const
{
    // user who sent the message
    const TgBot::User::Ptr& pUser = query->from;

    for (auto&&[command, commandInfo] : m_botCommands)
    {
        if (auto callbackKeywordIt = commandInfo.m_callbacksKeywords.find(callbackKeyWord);
            callbackKeywordIt != commandInfo.m_callbacksKeywords.end())
        {
            // make sure there is such a user
            usersList.EnsureExist(pUser, query->message->chat->id);
            // make sure the user can send this command/handle callbacks
            if (commandInfo.m_availabilityForUsers[usersList.GetUserStatus(pUser)])
                return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------//
void CommandsInfoService::ResetCommandList()
{
    m_botCommands.clear();
}

} // namespace telegram::command