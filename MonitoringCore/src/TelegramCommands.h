#pragma once

#include "TelegramBot.h"

////////////////////////////////////////////////////////////////////////////////
// вспомогательный клас для работы с командами телеграма
class CTelegramBot::CommandsHelper
{
public:
    // статус пользователей которым доступно использование команды
    typedef ITelegramUsersList::UserStatus AvailableStatus;
    CommandsHelper() = default;

public:
    // Добавление описания команды
    // @param command - идентификатор исполняемой команды
    // @param commandtext - текст команды
    // @param descr - описание команды
    // @param availabilityStatuses - перечень статусов пользователей которым доступна команда
    void addCommand(const CTelegramBot::Command command,
                    const std::wstring& commandText, const std::wstring& descr,
                    const std::vector<AvailableStatus>& availabilityStatuses);

    // Получить список команд с описанием для определенного пользователя
    std::wstring getAvailableCommandsWithDescr(const AvailableStatus userStatus) const;

    // Проверка что надо ответить на команду пользователю
    // если false - в messageToUser будет ответ пользователю
    bool ensureNeedAnswerOnCommand(ITelegramUsersList* usersList,
                                   const CTelegramBot::Command command,
                                   const MessagePtr commandMessage,
                                   std::wstring& messageToUser) const;

private:
    // Описание команды телеграм бота
    struct CommandDescription
    {
        CommandDescription(std::wstring text, std::wstring descr)
            : m_text(std::move(text)) , m_description(std::move(descr))
        {}

        // тект который надо отправить для выполнения команды
        std::wstring m_text;
        // описание команды
        std::wstring m_description;
        // доступность команды для различных типов пользователей
        std::bitset<AvailableStatus::eLastStatus> m_availabilityForUsers;
    };

    // перечень команд бота
    //          команда            |  описание команды
    std::map<CTelegramBot::Command, CommandDescription> m_botCommands;
};

