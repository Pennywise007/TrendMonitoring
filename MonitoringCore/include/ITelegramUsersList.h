#pragma once

#include <ext/core/dispatcher.h>
#include <ext/serialization/iserializable.h>

#pragma warning( push )
#pragma warning( disable: 4996 ) // boost deprecated objects usage
#include <tgbot/tgbot.h>
#pragma warning( pop )

namespace telegram::users {

// Notification about telegram user list changes
struct ITelegramUserChangedEvent : ext::events::IBaseEvent
{
    virtual void OnChanged() = 0;
};

// interface for managing telegram user list
struct ITelegramUsersList : ext::serializable::ISerializableCollection
{
    virtual ~ITelegramUsersList() = default;

    enum UserStatus
    {
        eNotAuthorized,
        eOrdinaryUser,
        eAdmin,
        // Add status before this line
        eLastStatus
    };

    // ensure that user exists
    virtual void EnsureExist(const TgBot::User::Ptr& pUser, const int64_t chatId) = 0;

    // set/get user status
    virtual void SetUserStatus(const TgBot::User::Ptr& pUser, const UserStatus newStatus) = 0;
    virtual UserStatus GetUserStatus(const TgBot::User::Ptr& pUser) = 0;

    // set/get user status last user command
    virtual void SetUserLastCommand(const TgBot::User::Ptr& pUser, const std::string& command) = 0;
    virtual std::string GetUserLastCommand(const TgBot::User::Ptr& pUser) = 0;

    // gel all user chats identifiers for all users with given status
    virtual std::list<int64_t> GetAllChatIdsByStatus(const UserStatus userStatus) = 0;
};

typedef std::shared_ptr<ITelegramUsersList> ITelegramUsersListPtr;

} // namespace telegram::users