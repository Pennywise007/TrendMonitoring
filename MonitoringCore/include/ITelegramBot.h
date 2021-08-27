#pragma once

#include <afx.h>

namespace telegram::bot {

////////////////////////////////////////////////////////////////////////////////
// настройки телеграм бота
struct TelegramBotSettings
{
    // состояние работы бота
    bool bEnable = false;
    // токен бота
    CString sToken;
};

////////////////////////////////////////////////////////////////////////////////
interface ITelegramBot
{
    virtual ~ITelegramBot() = default;

    // установка настроек бота
    virtual void setBotSettings(const TelegramBotSettings& botSettings) = 0;
    // функция отправки сообщения администраторам системы
    virtual void sendMessageToAdmins(const CString& message) const  = 0;
    // функция отправки сообщения пользователям системы
    virtual void sendMessageToUsers(const CString& message) const = 0;
};

} // namespace telegram::bot