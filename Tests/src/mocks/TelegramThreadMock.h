#pragma once

#include "gmock/gmock.h"

#include "TelegramThread.h"

struct TelegramThreadMock : public ITelegramThread
{
    MOCK_METHOD(void, StartTelegramThread, ((const std::unordered_map<std::string, CommandFunction>& commandsList),
                const CommandFunction& onUnknownCommand,
                const CommandFunction& OnNonCommandMessage), (override));
    MOCK_METHOD(void, StopTelegramThread, (), (override));
    MOCK_METHOD(void, SendMessage, (const std::list<int64_t>& chatIds, const std::wstring& msg,
                bool disableWebPagePreview, int32_t replyToMessageId,
                TgBot::GenericReply::Ptr replyMarkup,
                const std::string& parseMode, bool disableNotification), (override));
    MOCK_METHOD(void, SendMessage, (int64_t chatId, const std::wstring& msg, bool disableWebPagePreview,
                int32_t replyToMessageId, TgBot::GenericReply::Ptr replyMarkup,
                const std::string& parseMode, bool disableNotification), (override));
    MOCK_METHOD(TgBot::EventBroadcaster&, GetBotEvents, (), (override));
    MOCK_METHOD(const TgBot::Api&, GetBotApi, (), (override));
};
