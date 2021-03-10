#include "pch.h"
#include "TelegramCommands.h"

#include <regex>

////////////////////////////////////////////////////////////////////////////////
// Формирование колбэка
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
    // колбэк должен быть вида
    // /%COMMAND% %PARAM_1%={'%VALUE_1%'} %PARAM_2%={'%VALUE_2%'} ...
    // Формируем пару %PARAM_N%={'%VALUE_N'}
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
    // Список экранируемых символов, если не экранировать - ругается телеграм
    const std::wregex escapedCharacters(LR"(['])");
    const std::wstring rep(LR"(\$&)");

    // из-за того что TgTypeParser::appendToJson сам не добавляет '}'
    // если последним символом является '}' добавляем в конце пробел
    const std::wstring resultReport = std::regex_replace((m_reportStr + " ").GetString(), escapedCharacters, rep).c_str();

    // максимальное ограничение не размер запроса телеграма 64 байта
    /*constexpr int maxReportSize = 64;

    const auto result = getUtf8Str(resultReport);
    assert(result.length() <= maxReportSize);*/
    return getUtf8Str(resultReport);
}

////////////////////////////////////////////////////////////////////////////////
// вспомогательный клас для работы с командами телеграма
void CTelegramBot::CommandsHelper::addCommand(const CTelegramBot::Command command,
                                              const std::wstring& commandText, const std::wstring& descr,
                                              const std::vector<AvailableStatus>& availabilityStatuses)
{
    auto commandDescrIt = m_botCommands.try_emplace(command, commandText, descr);
    assert(commandDescrIt.second && "Команда уже добавлена.");

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
        message = L"Поддерживаемые команды бота:\n\n\n" + message + L"\n\nДля того чтобы их использовать необходимо написать их этому боту(обязательно использовать слэш перед текстом команды(/)!). Или нажать в этом окне, они должны подсвечиваться.";
    return message;
}

//----------------------------------------------------------------------------//
bool CTelegramBot::CommandsHelper::ensureNeedAnswerOnCommand(ITelegramUsersList* usersList,
                                                             const CTelegramBot::Command command,
                                                             const MessagePtr commandMessage,
                                                             std::wstring& messageToUser) const
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
        std::wstring availableCommands = getAvailableCommandsWithDescr(userStatus);
        if (availableCommands.empty())
        {
            if (userStatus == ITelegramUsersList::eNotAuthorized)
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