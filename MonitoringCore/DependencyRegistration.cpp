#include "pch.h"
#include "DependencyRegistration.h"

#include <tuple>
#include <shared_mutex>
#include <list>

#include <TelegramThread.h>

#include <ext/serialization/iserializable.h>

#include <ext/core/dependency_injection.h>
#include <ext/utils/call_once.h>

#include "include/IDirService.h"
#include "src/DirServiceImpl.h"

#include "include/IMonitoringTasksService.h"
#include "src/MonitoringTaskService/MonitoringTasksServiceImpl.h"

#include "include/ITrendMonitoring.h"
#include "src/TrendMonitoring.h"

#include "include/ITelegramBot.h"
#include "src/Telegram/TelegramBot.h"
#include "src/Telegram/TelegramThreadImpl.h"

#include "include/ITelegramUsersList.h"

namespace telegram {
namespace users {
class TelegramUsersList; // Telegram user information
} // namespace users
namespace bot {
struct TelegramParameters; // Telegram bot settings, get it from DI
} // namespace bot
} // namespace telegram

void DependencyRegistration::RegisterServices()
{
    CALL_ONCE(
    (
        ext::ServiceCollection& serviceCollection = ext::get_service<ext::ServiceCollection>();

        serviceCollection.RegisterSingleton<DirServiceImpl, IDirService>();

        serviceCollection.RegisterSingleton<TrendMonitoring, ITrendMonitoring>();
        serviceCollection.RegisterSingleton<MonitoringTasksServiceImpl, IMonitoringTasksService>();

        serviceCollection.RegisterSingleton<telegram::users::TelegramUsersList, telegram::users::ITelegramUsersList>();
        serviceCollection.RegisterSingleton<telegram::bot::TelegramParameters, telegram::bot::ITelegramBotSettings>();

        serviceCollection.RegisterSingleton<telegram::bot::CTelegramBot, telegram::bot::ITelegramBot>();
        serviceCollection.RegisterSingleton<telegram::thread::TelegramThreadImpl, ITelegramThread>();
    ));
}

namespace telegram {
namespace users {

// Telegram user information
class ATL_NO_VTABLE TelegramUser
    : public ext::serializable::SerializableObject<TelegramUser>
    , public TgBot::User
{
public:
    typedef std::shared_ptr<TelegramUser> Ptr;

    TelegramUser()
    {
        // TgBot::User params
        REGISTER_SERIALIZABLE_OBJECT(id);
        REGISTER_SERIALIZABLE_OBJECT(isBot);
        REGISTER_SERIALIZABLE_OBJECT(firstName);
        REGISTER_SERIALIZABLE_OBJECT(lastName);
        REGISTER_SERIALIZABLE_OBJECT(username);
        REGISTER_SERIALIZABLE_OBJECT(languageCode);
    }
    TelegramUser(const TgBot::User& pUser);

    TelegramUser& operator=(const TgBot::User& pUser);
    bool operator!=(const TgBot::User& pUser) const;
    // TODO C++20
    //auto operator<=>(const TgBot::User::Ptr&) const = default;

public:
    // user chat id
    DECLARE_SERIALIZABLE((int64_t) m_chatId, 0);
    // user status
    DECLARE_SERIALIZABLE((ITelegramUsersList::UserStatus) m_userStatus, ITelegramUsersList::UserStatus::eNotAuthorized);
    // last sended by user command to bot
    DECLARE_SERIALIZABLE((std::string) m_userLastCommand);
    // note about user, not used now, but can be added in configuration file
    DECLARE_SERIALIZABLE((std::wstring) m_userNote);
};

// Telegram users handler, synchronised
class TelegramUsersList
    : public ext::serializable::SerializableObject<TelegramUsersList, nullptr, ITelegramUsersList>
    , ext::NonCopyable
{
public:
    typedef std::shared_ptr<TelegramUsersList> Ptr;

// ISerializableCollection
public:
    void OnSerializationStart() override;
    void OnSerializationEnd() override;

    void OnDeserializationStart() override;
    void OnDeserializationEnd() override;

// ITelegramUsersList
public:
    // ensure that user exists
    void EnsureExist(const TgBot::User::Ptr& pUser, const int64_t chatId) override;
    // set/get user status
    void SetUserStatus(const TgBot::User::Ptr& pUser, const UserStatus newStatus) override;
    UserStatus GetUserStatus(const TgBot::User::Ptr& pUser) override;
    // set/get user status last user command
    void SetUserLastCommand(const TgBot::User::Ptr& pUser, const std::string& command) override;
    std::string GetUserLastCommand(const TgBot::User::Ptr& pUser) override;
    // gel all user chats identifiers for all users with given status
    std::list<int64_t> GetAllChatIdsByStatus(const UserStatus userStatus) override;

private:
    // allow to get user iterator from collection
    EXT_NODISCARD std::list<TelegramUser::Ptr>::iterator GetUserIterator(const TgBot::User::Ptr& pUser);
    // get user iterator from collection or create it
    std::list<TelegramUser::Ptr>::iterator GetOrCreateUsertIterator(const TgBot::User::Ptr& pUser);
    // notification about changes inside user list
    void OnUsersListChanged() const;

private:
    // telegram users list
    DECLARE_SERIALIZABLE((std::list<TelegramUser::Ptr>) m_telegramUsers);

    // telegram users list mutex
    std::shared_mutex m_usersMutex;
};

} // namespace users

namespace bot {

// Telegram bot settings, get it from DI
struct TelegramParameters
    : ext::serializable::SerializableObject<TelegramParameters, nullptr, ITelegramBotSettings>
    , private ext::NonCopyable
{
public:
    typedef std::shared_ptr<TelegramParameters> Ptr;

    TelegramParameters(ext::ServiceProvider::Ptr serviceProvider)
        : m_telegramUsers(ext::GetInterface<users::ITelegramUsersList>(serviceProvider))
    {
        REGISTER_SERIALIZABLE_OBJECT(m_telegramUsers);
    }

// ITelegramBotSettings
private:
    void SetSettings(const bool botEnable, const std::wstring& botToken) override;
    void GetSettings(bool& botEnable, std::wstring& botToken) const override;
    EXT_NODISCARD std::shared_ptr<users::ITelegramUsersList> GetUsersList() override;

private:
    // telegram users list
    std::shared_ptr<users::ITelegramUsersList> m_telegramUsers;
    DECLARE_SERIALIZABLE((bool) bEnable);
    DECLARE_SERIALIZABLE((std::wstring) sToken);
};

} // namespace bot
} // namespace telegram

namespace {
// creating a constant wrapper object over TgBot::User fields
// which can later be used to compare objects
template <class T>
auto wrap_user_fields(T& pUser)
{
    return std::tie(pUser.id, pUser.isBot,
        pUser.firstName, pUser.lastName,
        pUser.username, pUser.languageCode);
}
} // namespace

namespace telegram {
namespace users {

TelegramUser::TelegramUser(const TgBot::User& pUser)
{
    *this = pUser;
}

TelegramUser& TelegramUser::operator=(const TgBot::User& pUser)
{
    wrap_user_fields(*this) = wrap_user_fields(pUser);
    return *this;
}


bool TelegramUser::operator!=(const TgBot::User& pUser) const
{
    return wrap_user_fields(*this) != wrap_user_fields(pUser);
}

void TelegramUsersList::OnSerializationStart()
{
    SerializableObject::OnSerializationStart();
    m_usersMutex.lock();
}


void TelegramUsersList::OnSerializationEnd()
{
    SerializableObject::OnSerializationEnd();
    m_usersMutex.unlock();
}


void TelegramUsersList::OnDeserializationStart()
{
    SerializableObject::OnDeserializationStart();
    m_usersMutex.lock();
}


void TelegramUsersList::OnDeserializationEnd()
{
    SerializableObject::OnDeserializationEnd();

    m_usersMutex.unlock();
}


void TelegramUsersList::EnsureExist(const TgBot::User::Ptr& pUser, const int64_t chatId)
{
    std::lock_guard lock(m_usersMutex);

    bool bListChanged = false;
    if (auto userIt = GetUserIterator(pUser);
        userIt == m_telegramUsers.end())
    {
        // insert user with new id
        userIt = m_telegramUsers.insert(m_telegramUsers.end(), std::make_shared<TelegramUser>(*pUser));
        userIt->get()->m_chatId = chatId;
        userIt->get()->m_userNote = getUNICODEString(pUser->firstName).c_str();

        bListChanged = true;
    }
    else if (*userIt->get() != *pUser ||
        userIt->get()->m_chatId != chatId)
    {
        *userIt->get() = *pUser;
        userIt->get()->m_chatId = chatId;

        bListChanged = true;
    }

    if (bListChanged)
        OnUsersListChanged();
}

void TelegramUsersList::SetUserStatus(const TgBot::User::Ptr& _pUser, const UserStatus newStatus)
{
    std::lock_guard lock(m_usersMutex);

    auto pUser = *GetOrCreateUsertIterator(_pUser);
    if (pUser->m_userStatus != newStatus)
    {
        pUser->m_userStatus = newStatus;
        OnUsersListChanged();
    }
}

ITelegramUsersList::UserStatus TelegramUsersList::GetUserStatus(const TgBot::User::Ptr& pUser)
{
    std::lock_guard lock(m_usersMutex);

    return (*GetOrCreateUsertIterator(pUser))->m_userStatus;
}

void TelegramUsersList::SetUserLastCommand(const TgBot::User::Ptr& _pUser, const std::string& command)
{
    std::lock_guard lock(m_usersMutex);

    auto pUser = *GetOrCreateUsertIterator(_pUser);
    if (pUser->m_userLastCommand != command)
    {
        pUser->m_userLastCommand = command;
        OnUsersListChanged();
    }
}

std::string TelegramUsersList::GetUserLastCommand(const TgBot::User::Ptr& pUser)
{
    std::lock_guard lock(m_usersMutex);

    return (*GetOrCreateUsertIterator(pUser))->m_userLastCommand;
}

std::list<int64_t> TelegramUsersList::GetAllChatIdsByStatus(const UserStatus userStatus)
{
    std::lock_guard lock(m_usersMutex);

    std::list<int64_t> chats;
    for (auto& pUser : m_telegramUsers)
    {
        if (pUser->m_userStatus == userStatus)
            chats.push_back(pUser->m_chatId);
    }

    return chats;
}

std::list<TelegramUser::Ptr>::iterator TelegramUsersList::GetUserIterator(const TgBot::User::Ptr& pUser)
{
    // check if the data mutex is already locked
    EXT_ASSERT(!m_usersMutex.try_lock());

    return std::find_if(m_telegramUsers.begin(), m_telegramUsers.end(),
        [&userId = pUser->id](const TelegramUser::Ptr& telegramUser)
    {
        return telegramUser->id == userId;
    });
}


std::list<TelegramUser::Ptr>::iterator TelegramUsersList::GetOrCreateUsertIterator(const TgBot::User::Ptr& pUser)
{
    auto userIt = GetUserIterator(pUser);
    if (userIt == m_telegramUsers.end())
    {
        EXT_ASSERT("User has not been created yet!");

        // Create a user with a chat id equal to the user id
        EnsureExist(pUser, (int64_t)pUser->id);
        // Get it again
        userIt = GetUserIterator(pUser);
        EXT_ASSERT(userIt != m_telegramUsers.end());
    }

    return userIt;
}


void TelegramUsersList::OnUsersListChanged() const
{
    ext::send_event_async(&ITelegramUserChangedEvent::OnChanged);
}

} // namespace users

namespace bot {

void telegram::bot::TelegramParameters::SetSettings(const bool botEnable, const std::wstring& botToken)
{
    bEnable = botEnable;
    sToken = botToken;
    ext::send_event_async(&ISettingsChangedEvent::OnBotSettingsChanged, botEnable, botToken);
}

void TelegramParameters::GetSettings(bool& botEnable, std::wstring& botToken) const
{
    botEnable = bEnable;
    botToken = sToken;
}

EXT_NODISCARD std::shared_ptr<users::ITelegramUsersList> TelegramParameters::GetUsersList()
{
    return m_telegramUsers;
}

} // namespace bot
} // namespace telegram
