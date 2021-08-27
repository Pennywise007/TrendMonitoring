#pragma once

#include <include/ITelegramBot.h>

#include "gmock/gmock.h"

namespace telegram::bot {

struct TelegramBotMock : public ITelegramBot
{
    MOCK_METHOD(void, setBotSettings, (const TelegramBotSettings& botSettings), (override));
    MOCK_METHOD(void, sendMessageToAdmins, (const CString& message), (const, override));
    MOCK_METHOD(void, sendMessageToUsers, (const CString& message), (const, override));
};

} // namespace telegram::bot