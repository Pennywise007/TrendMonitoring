#include "pch.h"

#include <atlstr.h>
#include <filesystem>

#include "CommandsInfoService.h"

#include <boost/scope_exit.hpp>

#include "DirsService.h"
#include "../Utils.h"

namespace telegram::command {

////////////////////////////////////////////////////////////////////////////////
// выполнение команды на рестарт
void execute_restart_command(const TgBot::Message::Ptr& message, ITelegramThread* telegramThread)
{
    // перезапуск системы делаем через батник так как надо много всего сделать для разных систем
    CString batFullPath = get_service<DirsService>().getExeDir() + kRestartSystemFileName;

    // сообщение пользователю
    CString messageToUser;
    if (std::filesystem::is_regular_file(batFullPath.GetString()))
    {
        telegramThread->sendMessage(message->chat->id, L"Перезапуск системы осуществляется.");

        // запускаем батник
        STARTUPINFO cif = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION m_ProcInfo = { nullptr };

        BOOST_SCOPE_EXIT(&batFullPath)
        {
            batFullPath.ReleaseBuffer();
        } BOOST_SCOPE_EXIT_END;

        if (FALSE != CreateProcess(batFullPath.GetBuffer(),     // имя исполняемого модуля
            nullptr,	                    // Командная строка
            NULL,                        // Указатель на структуру SECURITY_ATTRIBUTES
            NULL,                        // Указатель на структуру SECURITY_ATTRIBUTES
            0,                           // Флаг наследования текущего процесса
            NULL,                        // Флаги способов создания процесса
            NULL,                        // Указатель на блок среды
            NULL,                        // Текущий диск или каталог
            &cif,                        // Указатель на структуру STARTUPINFO
            &m_ProcInfo))                // Указатель на структуру PROCESS_INFORMATION)
        {	// идентификатор потока не нужен
            CloseHandle(m_ProcInfo.hThread);
            CloseHandle(m_ProcInfo.hProcess);
        }
    }
    else
        telegramThread->sendMessage(message->chat->id, L"Файл для перезапуска не найден.");
}

////////////////////////////////////////////////////////////////////////////////
// вспомогательный клас для работы с командами телеграма
void CommandsInfoService::addCommand(Command command, std::wstring&& commandText, std::wstring&& description,
                                     std::set<std::string>&& callbacksKeyWords,
                                     const std::vector<AvailableStatus>& availabilityStatuses)
{
    const auto commandDescrIt = m_botCommands.try_emplace(command, std::move(commandText),
                                                          std::move(description), std::move(callbacksKeyWords));
    assert(commandDescrIt.second && "Команда уже добавлена.");

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
        message = L"Поддерживаемые команды бота:\n\n\n" + message + L"\n\nДля того чтобы их использовать необходимо написать их этому боту(обязательно использовать слэш перед текстом команды(/)!). Или нажать в этом окне, они должны подсвечиваться.";
    return message;
}

//----------------------------------------------------------------------------//
bool CommandsInfoService::ensureNeedAnswerOnCommand(users::ITelegramUsersList* usersList, Command command,
                                                    const MessagePtr& commandMessage, std::wstring& messageToUser) const
{
    // пользователь отправивший сообщение
    const TgBot::User::Ptr& pUser = commandMessage->from;

    // убеждаемся что есть такой пользователь
    usersList->ensureExist(pUser, commandMessage->chat->id);
    // получаем статус пользователя
    const auto userStatus = usersList->getUserStatus(pUser);
    // проверяем что пользователь может использовать команду
    const auto commandIt = m_botCommands.find(command);
    if (commandIt == m_botCommands.end() ||
        !commandIt->second.m_availabilityForUsers[userStatus])
    {
        // формируем ответ пользователю со списком доступных ему команд
        std::wstring availableCommands = getAvailableCommandsWithDescription(userStatus);
        if (availableCommands.empty())
        {
            if (userStatus == users::ITelegramUsersList::eNotAuthorized)
                messageToUser = L"Для работы бота вам необходимо авторизоваться.";
            else
                messageToUser = L"Неизвестная команда. У вас нет доступных команд, обратитесь к администратору";
        }
        else
            messageToUser = L"Неизвестная команда. " + std::move(availableCommands);
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
    // пользователь отправивший сообщение
    const TgBot::User::Ptr& pUser = commandMessage->from;

    for (auto&&[command, commandInfo] : m_botCommands)
    {
        if (auto callbackKeywordIt = commandInfo.m_callbacksKeywords.find(callbackKeyWord);
            callbackKeywordIt != commandInfo.m_callbacksKeywords.end())
        {
            // убеждаемся что есть такой пользователь
            usersList->ensureExist(pUser, commandMessage->chat->id);
            // убеждаемся что пользователю можно отправлять эту команду/обрабатывать колбэки
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