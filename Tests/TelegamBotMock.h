#pragma once

#include <include/ITelegramBot.h>

#include "gmock/gmock.h"

namespace telegram::bot {

struct TelegramBotMock : public ITelegramBot
{
    MOCK_METHOD(void, SetBotSettings, (const TelegramBotSettings& botSettings), (override));
    MOCK_METHOD(void, SendMessageToAdmins, (const CString& message), (const, override));
    MOCK_METHOD(void, SendMessageToUsers, (const CString& message), (const, override));
};

} // namespace telegram::bot