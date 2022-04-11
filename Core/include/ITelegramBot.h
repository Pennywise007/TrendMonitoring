#pragma once

#include <string>
#include <ext/core/dispatcher.h>
#include <ext/serialization/iserializable.h>

#include "ITelegramUsersList.h"

namespace telegram::bot {

struct ITelegramBotSettings : ext::serializable::ISerializableCollection
{
    virtual void SetSettings(const bool botEnable, const std::wstring& botToken) = 0;
    virtual void GetSettings(bool& botEnable, std::wstring& botToken) const = 0;

    EXT_NODISCARD virtual std::shared_ptr<users::ITelegramUsersList>& GetUsersList() = 0;
};

struct ISettingsChangedEvent : ext::events::IBaseEvent
{
    virtual void OnBotSettingsChanged(const bool newEnableValue, const std::wstring& newBotToken) = 0;
};

struct ITelegramBot
{
    virtual ~ITelegramBot() = default;

    // send message to all users with status admin
    virtual void SendMessageToAdmins(const std::wstring& message) const  = 0;
    // send message to all users with status ordinary user(authorized, but not admin)
    virtual void SendMessageToUsers(const std::wstring& message) const = 0;
};

} // namespace telegram::bot