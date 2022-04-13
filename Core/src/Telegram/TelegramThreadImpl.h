#pragma once

#include <TelegramThread.h>
#include <include/ITelegramBot.h>
#include <include/ITrendMonitoring.h>

#include <ext/core/dispatcher.h>
#include <ext/core/dependency_injection.h>

namespace telegram::thread {

// Telegram bot implementation, proxy on CreateTelegramThread function only for dependency injection and tests
class TelegramThreadImpl final
    : public ITelegramThread
    , ext::events::ScopeSubscription<telegram::bot::ISettingsChangedEvent>
{
public:
    TelegramThreadImpl(std::shared_ptr<telegram::bot::ITelegramBotSettings>&& botSettings)
    {
        EXT_REQUIRE(botSettings);

        bool enable;
        std::wstring botToken;
        botSettings->GetSettings(enable, botToken);
        OnBotSettingsChanged(enable, botToken);

        // we must handle changing settings event the very first
        ScopeSubscription::SetFirstPriority<telegram::bot::ISettingsChangedEvent>();
    }

// ITelegramThread
private:
    // start thread
    void StartTelegramThread(const std::unordered_map<std::string, CommandFunction>& commandsList,
                             const CommandFunction& onUnknownCommand = nullptr,
                             const CommandFunction& onNonCommandMessage = nullptr) override
    {
        EXT_EXPECT(!!m_telegramThread);
        m_telegramThread->StartTelegramThread(commandsList, onUnknownCommand, onNonCommandMessage);
    }

    // stop thread
    void StopTelegramThread() override
    {
        if (m_telegramThread)
            m_telegramThread->StopTelegramThread();
    }

    // function for sending messages to chats
    void SendMessage(const std::list<int64_t>& chatIds, const std::wstring& msg,
                     bool disableWebPagePreview = false, int32_t replyToMessageId = 0,
                     TgBot::GenericReply::Ptr replyMarkup = std::make_shared<TgBot::GenericReply>(),
                     const std::string& parseMode = "", bool disableNotification = false) override
    {
        if (m_telegramThread)
            m_telegramThread->SendMessage(chatIds, msg, disableWebPagePreview, replyToMessageId, replyMarkup, parseMode, disableNotification);
    }

    // function to send a message to the chat
    void SendMessage(int64_t chatId, const std::wstring& msg, bool disableWebPagePreview = false,
                     int32_t replyToMessageId = 0,
                     TgBot::GenericReply::Ptr replyMarkup = std::make_shared<TgBot::GenericReply>(),
                     const std::string& parseMode = "", bool disableNotification = false) override
    {
        if (m_telegramThread)
            m_telegramThread->SendMessage(chatId, msg, disableWebPagePreview, replyToMessageId, replyMarkup, parseMode, disableNotification);
    }

    // returns bot events to handle everything itself
    TgBot::EventBroadcaster& GetBotEvents() override
    {
        EXT_EXPECT(!!m_telegramThread);
        return m_telegramThread->GetBotEvents();
    }

    // get api bot
    const TgBot::Api& GetBotApi() override
    {
        EXT_EXPECT(!!m_telegramThread);
        return m_telegramThread->GetBotApi();
    }

// ISettingsChangedEvent
private:
    void OnBotSettingsChanged(const bool newEnableValue, const std::wstring& newBotToken) override
    {
        if (m_telegramThread)
        {
            m_telegramThread->StopTelegramThread();
            m_telegramThread = nullptr;
            // currently we can`t stop it immediatly
            // TODO rework on BoostHttpOnlySslClient in dll
            //m_telegramThread.reset();
        }

        if (!newEnableValue || newBotToken.empty())
            return;

        // start monitoring thread
        m_telegramThread = CreateTelegramThread(std::narrow(newBotToken.c_str()),
            [](const std::wstring& alertMessage)
            {
                send_message_to_log(ILogEvents::LogMessageData::MessageType::eError, std::wstring(alertMessage.c_str()));
            });
    }

private:
    ITelegramThreadPtr m_telegramThread;
};

} // namespace telegram::thread