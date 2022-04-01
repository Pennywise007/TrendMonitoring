#pragma once

#include <bitset>
#include <string>
#include <vector>

#include <include/ITelegramUsersList.h>
#include <TelegramThread.h>

#include <ext/core/singleton.h>
#include "TelegramCallbacks.h"

namespace telegram::command {

// executing bot command "restart"
void execute_restart_command(const std::int64_t& chatId, ITelegramThread* telegramThread);

// singleton for managing bot commands
class CommandsInfoService
{
    friend ext::Singleton<CommandsInfoService>;
public:
    // user status for checking command availability
    typedef users::ITelegramUsersList::UserStatus AvailableStatus;

    // Bot commands list
    enum class Command
    {
        eUnknown,           // Unknown bot command(not registered)
        eInfo,              // Get bot commands info
        eReport,            // Get channel report for period
        eRestart,           // Restarting monitoring
        eAlertingOn,        // Enable notifications about channel
        eAlertingOff,       // Disable notifications about channel
        eAlarmingValue,     // Change alarming value for channel
        // Last command, add commands before this line
        eLastCommand
    };

public:
    // Add command info
    // @param command - Command id
    // @param commandText - Command text which need send to bot for executing
    // @param description - Command description for user
    // @param callbacksKeyWords - callbacks for command list, all callbacks which command can use
    // @param availabilityStatuses - availability of command for users
    void AddCommand(Command command, std::wstring&& commandText, std::wstring&& description,
                    std::set<std::string>&& callbacksKeyWords,
                    const std::vector<AvailableStatus>& availabilityStatuses);

    // Get all available commands for user with given status
    EXT_NODISCARD std::wstring GetAvailableCommandsWithDescription(AvailableStatus userStatus) const;

    // Check if we need to process on command from user
    // Returns false if we don`t need to process command and add information to @messageToUser
    bool EnsureNeedAnswerOnCommand(users::ITelegramUsersList& usersList, Command command,
                                   const TgBot::User::Ptr& from, const std::int64_t& chatId, std::wstring& messageToUser) const;

    // Check if we need to process on callback from user(if user pressed callback button after user status changed)
    bool EnsureNeedAnswerOnCallback(users::ITelegramUsersList& usersList,
                                    const std::string& callbackKeyWord,
                                    const TgBot::CallbackQuery::Ptr& query) const;

    // Clean commands list
    void ResetCommandList();

private:
    struct CommandDescription
    {
        explicit CommandDescription(std::wstring&& text, std::wstring&& descr, std::set<std::string>&& callbacksKeyWord) EXT_NOEXCEPT
            : m_text(std::move(text)), m_description(std::move(descr)), m_callbacksKeywords(std::move(callbacksKeyWord))
        {}

        // command text which need send to bot for executing
        std::wstring m_text;
        // Command description for user
        std::wstring m_description;
        // callbacks for command list, all callbacks which command can use
        std::set<std::string> m_callbacksKeywords;
        // availability of command for users
        std::bitset<AvailableStatus::eLastStatus> m_availabilityForUsers;
    };

    std::map<Command, CommandDescription> m_botCommands;
};

} // namespace telegram::command