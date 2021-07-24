#pragma once

#include <afx.h>
#include <memory>
#include <string>

#include <boost/core/noncopyable.hpp>

#include <include/ITelegramUsersList.h>
#include <include/ITrendMonitoring.h>

#include <TelegramDLL/TelegramThread.h>

#include "CommandsInfoService.h"
#include "TelegramCallbacks.h"

////////////////////////////////////////////////////////////////////////////////
// класс управления телеграм ботом
class CTelegramBot
    : boost::noncopyable
{
public:
    CTelegramBot(const ITelegramUsersListPtr& telegramUsers, ITelegramThread* pDefaultTelegramThread = nullptr);
    ~CTelegramBot();

public:
    // установка настроек бота
    void setBotSettings(const TelegramBotSettings& botSettings);

    // функция отправки сообщения администраторам системы
    void sendMessageToAdmins(const CString& message) const;
    // функция отправки сообщения пользователям системы
    void sendMessageToUsers(const CString& message) const;

// Отработка команд бота
private:
    // добавить реакции на команды
    void fillCommandHandlers(std::unordered_map<std::string, CommandFunction>& commandsList,
                             CommandFunction& onUnknownCommand,
                             CommandFunction& onNonCommandMessage);
    // отработка не зарегистрированного сообщения/команды
    void onNonCommandMessage(const MessagePtr& commandMessage);

    // Обработка команд которые приходят боту отдельным сообщением
    void onCommandMessage       (CommandsInfoService::Command command, const MessagePtr& message);
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
    ITelegramUsersListPtr m_telegramUsers;
    // обработчик колбэков от телеграма
    callback::TelegramCallbacks m_callbacksHandler;
};
