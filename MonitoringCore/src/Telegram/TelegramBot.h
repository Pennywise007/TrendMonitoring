#pragma once

#include <memory>
#include <string>

#include <ext/core/noncopyable.h>
#include <ext/core/dispatcher.h>

#include <include/ITelegramUsersList.h>
#include <include/ITelegramBot.h>

#include <TelegramThread.h>

#include "CommandsInfoService.h"
#include "TelegramCallbacks.h"

namespace telegram::bot {

// telegram bot control class
class CTelegramBot final
    : public ITelegramBot
    , public ext::events::ScopeSubscription<telegram::bot::ISettingsChangedEvent>
    , ext::ServiceProviderHolder
{
public:
    CTelegramBot(ext::ServiceProvider::Ptr provider, std::shared_ptr<telegram::users::ITelegramUsersList>&& telegramUsers,
                 std::shared_ptr<ITelegramThread>&& thread);
    ~CTelegramBot();

// ITelegramBot
public:
    // send message to all users with status admin
    void SendMessageToAdmins(const std::wstring& message) const override;
    // send message to all users with status ordinary user(authorized, but not admin)
    void SendMessageToUsers(const std::wstring& message) const override;

// ISettingsChangedEvent
private:
    void OnBotSettingsChanged(const bool newEnableValue, const std::wstring& newBotToken) override;

// Bot command processing
private:
    // Add commands reactions
    void FillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                             CommandFunction& onUnknownCommand,
                             CommandFunction& OnNonCommandMessage);
    // handling non command messages
    void OnNonCommandMessage(const MessagePtr& commandMessage);

    // Handling commands from messages
    void OnCommandMessage       (command::CommandsInfoService::Command command, const MessagePtr& message);
    void OnCommandInfo          (const MessagePtr& commandMessage) const;
    void OnCommandReport        (const MessagePtr& commandMessage) const;
    void OnCommandRestart       (const MessagePtr& commandMessage) const;
    void OnCommandAlert         (const MessagePtr& commandMessage, bool bEnable) const;
    void OnCommandAlarmingValue (const MessagePtr& commandMessage) const;

private:
    // Telegram thread
    std::shared_ptr<ITelegramThread> m_telegramThread;
    // Telegram users list
    std::shared_ptr<users::ITelegramUsersList> m_telegramUsers;
    // callback handler from telegram
    std::shared_ptr<callback::TelegramCallbacks> m_callbacksHandler;
};

} // namespace telegram::bot