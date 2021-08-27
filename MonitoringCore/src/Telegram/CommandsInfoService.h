#pragma once

#include <bitset>
#include <string>
#include <vector>

#include <include/ITelegramUsersList.h>
#include <TelegramDLL/TelegramThread.h>

#include "Singleton.h"
#include "TelegramCallbacks.h"

namespace telegram::command {

// выполнение команды на рестарт
void execute_restart_command(const TgBot::Message::Ptr& message, ITelegramThread* telegramThread);

////////////////////////////////////////////////////////////////////////////////
// вспомогательный клас дл€ работы с командами телеграма
class CommandsInfoService
{
    friend class CSingleton<CommandsInfoService>;
public:
    // статус пользователей которым доступно использование команды
    typedef users::ITelegramUsersList::UserStatus AvailableStatus;

    // ѕеречень команд
    enum class Command
    {
        eUnknown,           // Ќеизвестна€ комнада бота
        eInfo,              // «апрос информации бота
        eReport,            // «апрос отчЄта за период
        eRestart,           // перезапуск мониторинга
        eAlertingOn,        // оповещени€ о событи€х канала включить
        eAlertingOff,       // оповещени€ о событи€х канала выключить
        eAlarmingValue,     // изменение допустимого уровн€ значений дл€ канала
        // ѕоследн€€ команда
        eLastCommand
    };

public:
    // ƒобавление описани€ команды
    // @param command - идентификатор исполн€емой команды
    // @param commandtext - текст команды
    // @param description - описание команды
    // @param callbacksKeyWords - перечень ключевых слов у колбэков которые порождены командой
    // @param availabilityStatuses - перечень статусов пользователей которым доступна команда
    void addCommand(Command command, std::wstring&& commandText, std::wstring&& description,
                    std::set<std::string>&& callbacksKeyWords,
                    const std::vector<AvailableStatus>& availabilityStatuses);

    // ѕолучить список команд с описанием дл€ определенного пользовател€
    std::wstring getAvailableCommandsWithDescription(AvailableStatus userStatus) const;

    // ѕроверка что надо ответить на команду пользователю
    // если false - в messageToUser будет ответ пользователю
    bool ensureNeedAnswerOnCommand(users::ITelegramUsersList* usersList, Command command,
                                   const MessagePtr& commandMessage, std::wstring& messageToUser) const;

    // ѕроверка что надо ответить на колбэк, пока пользователю показывали кнопку у него мог помен€тьс€ статус
    // если false - в messageToUser будет ответ пользователю
    bool ensureNeedAnswerOnCallback(users::ITelegramUsersList* usersList,
                                    const std::string& callbackKeyWord,
                                    const MessagePtr& commandMessage) const;

    // ќчищаем список команд
    void resetCommandList();

private:
    // ќписание команды телеграм бота
    struct CommandDescription
    {
        CommandDescription(std::wstring&& text, std::wstring&& descr, std::set<std::string>&& callbacksKeyWord)
            : m_text(std::move(text)), m_description(std::move(descr)), m_callbacksKeywords(std::move(callbacksKeyWord))
        {}

        // тект который надо отправить дл€ выполнени€ команды
        std::wstring m_text;
        // описание команды
        std::wstring m_description;
        // перечень ключевых слов у колбэков команды
        std::set<std::string> m_callbacksKeywords;
        // доступность команды дл€ различных типов пользователей
        std::bitset<AvailableStatus::eLastStatus> m_availabilityForUsers;
    };

    // перечень команд бота
    //       команда | описание команды
    std::map<Command, CommandDescription> m_botCommands;
};

} // namespace telegram::command