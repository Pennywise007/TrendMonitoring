#include "pch.h"

#include <TelegramDLL/TelegramThread.h>

#include "ApplicationConfiguration.h"
#include "Messages.h"

namespace
{
//----------------------------------------------------------------------------//
// создание константного объекта обёртки над полями TgBot::User
// который можно в дальнейшем использовать для сравнения объектов
template <class T>
auto wrap_user_fields(T& pUser)
{
    return std::tie(pUser.id,        pUser.isBot,
                    pUser.firstName, pUser.lastName,
                    pUser.username,  pUser.languageCode);
}
};

//****************************************************************************//
ChannelParameters::ChannelParameters(const CString& initChannelName)
{
    channelName = initChannelName;
}

//----------------------------------------------------------------------------//
const MonitoringChannelData& ChannelParameters::getMonitoringData() const
{
    return *this;
}

//----------------------------------------------------------------------------//
void ChannelParameters::setTrendChannelData(const TrendChannelData& data)
{
    trendData = data;
}

//----------------------------------------------------------------------------//
bool ChannelParameters::changeName(const CString& newName)
{
    if (channelName == newName)
        return false;

    channelName = newName;
    resetChannelData();
    return true;
}

//----------------------------------------------------------------------------//
bool ChannelParameters::changeNotification(const bool state)
{
    if (bNotify == state)
        return false;

    bNotify = state;
    return true;
}

//----------------------------------------------------------------------------//
bool ChannelParameters::changeInterval(const MonitoringInterval newInterval)
{
    if (monitoringInterval == newInterval)
        return false;

    monitoringInterval = newInterval;
    resetChannelData();
    return true;
}

//----------------------------------------------------------------------------//
bool ChannelParameters::changeAlarmingValue(const float newvalue)
{
    if (alarmingValue == newvalue)
        return false;

    alarmingValue = newvalue;
    return true;
}

//----------------------------------------------------------------------------//
void ChannelParameters::resetChannelData()
{
    // сбрасываем состояние загруженности данных по каналу
    channelState.dataLoaded = false;
    channelState.loadingDataError = false;

    m_loadingParametersIntervalEnd.reset();
}

namespace telegram {
namespace users {

////////////////////////////////////////////////////////////////////////////////
// Реализация TelegramUser
TelegramUser::TelegramUser(const TgBot::User& pUser)
{
    *this = pUser;
}
//----------------------------------------------------------------------------//
TelegramUser& TelegramUser::operator=(const TgBot::User& pUser)
{
    wrap_user_fields(*this) = wrap_user_fields(pUser);
    return *this;
}

//----------------------------------------------------------------------------//
bool TelegramUser::operator!=(const TgBot::User& pUser) const
{
    return wrap_user_fields(*this) != wrap_user_fields(pUser);
}

////////////////////////////////////////////////////////////////////////////////
// Реализация TelegramUsersList
bool TelegramUsersList::onStartSerializing()
{
    m_usersMutex.lock();

    if (!SerializableObjectsCollection::onStartSerializing())
    {
        m_usersMutex.unlock();
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------//
void TelegramUsersList::onEndSerializing()
{
    SerializableObjectsCollection::onEndSerializing();

    m_usersMutex.unlock();
}

//----------------------------------------------------------------------------//
bool TelegramUsersList::onStartDeserializing(const std::list<CString>& objNames)
{
    m_usersMutex.lock();

    if (!SerializableObjectsCollection::onStartDeserializing(objNames))
    {
        m_usersMutex.unlock();
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------//
void TelegramUsersList::onEndDeserializing()
{
    SerializableObjectsCollection::onEndDeserializing();

    m_usersMutex.unlock();
}

//----------------------------------------------------------------------------//
void TelegramUsersList::ensureExist(const TgBot::User::Ptr& pUser, const int64_t chatId)
{
    std::lock_guard lock(m_usersMutex);

    bool bListChanged = false;
    if (auto userIt = getUserIterator(pUser);
        userIt == m_telegramUsers.end())
    {
        // вставляем пользователя с новым идентификатором
        userIt = m_telegramUsers.insert(m_telegramUsers.end(), TelegramUser::make(*pUser));
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
        onUsersListChanged();
}

//----------------------------------------------------------------------------//
void TelegramUsersList::setUserStatus(const TgBot::User::Ptr& _pUser, const UserStatus newStatus)
{
    std::lock_guard lock(m_usersMutex);

    auto pUser = *getOrCreateUsertIterator(_pUser);
    if (pUser->m_userStatus != newStatus)
    {
        pUser->m_userStatus = newStatus;
        onUsersListChanged();
    }
}

//----------------------------------------------------------------------------//
ITelegramUsersList::UserStatus TelegramUsersList::getUserStatus(const TgBot::User::Ptr& pUser)
{
    std::lock_guard lock(m_usersMutex);

    return (*getOrCreateUsertIterator(pUser))->m_userStatus;
}

//----------------------------------------------------------------------------//
void TelegramUsersList::setUserLastCommand(const TgBot::User::Ptr& _pUser, const std::string& command)
{
    std::lock_guard lock(m_usersMutex);

    auto pUser = *getOrCreateUsertIterator(_pUser);
    if (pUser->m_userLastCommand != command)
    {
        pUser->m_userLastCommand = command;
        onUsersListChanged();
    }
}

//----------------------------------------------------------------------------//
std::string TelegramUsersList::getUserLastCommand(const TgBot::User::Ptr& pUser)
{
    std::lock_guard lock(m_usersMutex);

    return (*getOrCreateUsertIterator(pUser))->m_userLastCommand;
}

//----------------------------------------------------------------------------//
std::list<int64_t> TelegramUsersList::getAllChatIdsByStatus(const UserStatus userStatus)
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

//----------------------------------------------------------------------------//
std::list<TelegramUser::Ptr>::iterator TelegramUsersList::getUserIterator(const TgBot::User::Ptr& pUser)
{
    // проверяем что мьютекс на данные уже заблокирован
    assert(!m_usersMutex.try_lock());

    return std::find_if(m_telegramUsers.begin(), m_telegramUsers.end(),
                        [&userId = pUser->id](const TelegramUser::Ptr& telegramUser)
    {
        return telegramUser->id == userId;
    });
}

//----------------------------------------------------------------------------//
std::list<TelegramUser::Ptr>::iterator
TelegramUsersList::getOrCreateUsertIterator(const TgBot::User::Ptr& pUser)
{
    auto userIt = getUserIterator(pUser);
    if (userIt == m_telegramUsers.end())
    {
        assert(!"Пользователя ещё не создали!");

        // Создаем пользователя с идентификатором чата равным идентификатору пользователя
        ensureExist(pUser, (int64_t)pUser->id);
        // Получаем его ещё раз
        userIt = getUserIterator(pUser);
        assert(userIt != m_telegramUsers.end());
    }

    return userIt;
}

//----------------------------------------------------------------------------//
void TelegramUsersList::onUsersListChanged() const
{
    get_service<CMassages>().postMessage(onUsersListChangedEvent);
}

} // namespace users

namespace bot {
////////////////////////////////////////////////////////////////////////////////
// Реализация функций класса с настрйками телеграм бота
TelegramParameters& TelegramParameters::operator=(const TelegramBotSettings& botSettings)
{
    auto createTie = [](auto& botSettings)
    {
        return std::tie(botSettings.bEnable, botSettings.sToken);
    };

    createTie(*this) = createTie(botSettings);

    return *this;
}
} // namespace bot
} // namespace telegram

////////////////////////////////////////////////////////////////////////////////
// Реализация функций класса с настрйками приложения
telegram::users::ITelegramUsersListPtr ApplicationConfiguration::getTelegramUsers() const
{
    return m_telegramParameters->m_telegramUsers;
}

//----------------------------------------------------------------------------//
const telegram::bot::TelegramBotSettings& ApplicationConfiguration::getTelegramSettings() const
{
    return *m_telegramParameters;
}

//----------------------------------------------------------------------------//
void ApplicationConfiguration::setTelegramSettings(const telegram::bot::TelegramBotSettings& newSettings)
{
    *m_telegramParameters = newSettings;
}