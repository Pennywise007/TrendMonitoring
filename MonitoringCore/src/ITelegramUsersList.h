#pragma once

// —писок всопомогательных объектов дл€ работы с телеграмом

#include "COM.h"
#include "Messages.h"

#include <tgbot/tgbot.h>

// оповещение об изменение в списке пользователей телеграма
// {491F3E83-3A8B-48BF-9235-33E5CE2E9FEC}
constexpr EventId onUsersListChangedEvent =
{ 0x491f3e83, 0x3a8b, 0x48bf, { 0x92, 0x35, 0x33, 0xe5, 0xce, 0x2e, 0x9f, 0xec } };

////////////////////////////////////////////////////////////////////////////////
// интерфейс дл€ управлени€ списком пользователей телеграма
DECLARE_COM_INTERFACE(ITelegramUsersList, "EFB00D0B-3B37-4AEC-B63A-9ECEA34C0802")
{
    // статус пользовател€
    enum UserStatus
    {
        eNotAuthorized,     // Ќе авторизованный
        eOrdinaryUser,      // ќбычный пользователь системы
        eAdmin,             // јдминистратор системы
        // ѕоследний статус
        eLastStatus
    };

    // убедитьс€ что пользователь существует
    virtual void ensureExist(const TgBot::User::Ptr& pUser, const int64_t chatId) = 0;

    // получение статуса пользовател€
    virtual UserStatus getUserStatus(const TgBot::User::Ptr& pUser) = 0;
    // установка статуса пользовател€
    virtual void setUserStatus(const TgBot::User::Ptr& pUser,
                               const UserStatus newStatus) = 0;
    // получить все идентификаторы чатов пользователей с определенным статусом
    virtual std::list<int64_t> getAllChatIdsByStatus(const UserStatus usertatus) = 0;
};