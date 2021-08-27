#pragma once

#include <afx.h>
#include <memory>
#include <string>

#include <boost/core/noncopyable.hpp>

#include <include/ITelegramUsersList.h>
#include <include/ITelegramBot.h>

#include <TelegramDLL/TelegramThread.h>

#include "CommandsInfoService.h"
#include "TelegramCallbacks.h"

namespace telegram::bot {

////////////////////////////////////////////////////////////////////////////////
// класс управления телеграм ботом
class CTelegramBot
    : public ITelegramBot
    , boost::noncopyable
{
public:
    CTelegramBot(const users::ITelegramUsersListPtr& telegramUsers, ITelegramThread* pDefaultTelegramThread = nullptr);
    ~CTelegramBot();

// ITelegramBot
public:
    // установка настроек бота
    void setBotSettings(const TelegramBotSettings& botSettings) override;
    // функция отправки сообщения администраторам системы
    void sendMessageToAdmins(const CString& message) const override;
    // функция отправки сообщения пользователям системы
    void sendMessageToUsers(const CString& message) const override;

// Отработка команд бота
private:
    // добавить реакции на команды
    void fillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                             CommandFunction& onUnknownCommand,
                             CommandFunction& onNonCommandMessage);
    // отработка не зарегистрированного сообщения/команды
    void onNonCommandMessage(const MessagePtr& commandMessage);

    // Обработка команд которые приходят боту отдельным сообщением
    void onCommandMessage       (command::CommandsInfoService::Command command, const MessagePtr& message);
    void onCommandInfo          (const MessagePtr& commandMessage) const;
    void onCommandReport        (const MessagePtr& commandMessage) const;
    void onCommandRestart       (const MessagePtr& commandMessage) const;
    void onCommandAlert         (const MessagePtr& commandMessage, bool bEnable) const;
    void onCommandAlarmingValue (const MessagePtr& commandMessage) const;

private:
    // поток работающего телеграма
    ITelegramThreadPtr m_telegramThread;
    // использующийся по умолчанию поток телеграмма
    ITelegramThreadPtr m_defaultTelegramThread;
    // текущие настройки бота
    TelegramBotSettings m_botSettings;
    // данные о пользователях телеграмма
    users::ITelegramUsersListPtr m_telegramUsers;
    // обработчик колбэков от телеграма
    callback::TelegramCallbacks m_callbacksHandler;
};

} // namespace telegram::bot