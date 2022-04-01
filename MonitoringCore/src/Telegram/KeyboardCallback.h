#pragma once

#include <string>

#include <TelegramThread.h>

#include <ext/core/defines.h>

namespace telegram::callback {

// Class for forming a callback for telegram buttons, UTF-8 output
class KeyboardCallback
{
public:
    explicit KeyboardCallback(const std::string& keyWord) noexcept;
    KeyboardCallback(const KeyboardCallback& callback) = default;

    // add a line for the callback with a parameter - value pair
    KeyboardCallback& AddCallbackParam(const std::string& param, const std::wstring& value);
    template <typename CharType, typename Traits, typename Alloc>
    KeyboardCallback& AddCallbackParam(const std::string& param, const std::basic_string<CharType, Traits, Alloc>& value)
    {
        if constexpr (std::is_same_v<CharType, char>)
            return AddCallbackParam(param, getUNICODEString(value));
        else
            return AddCallbackParam(param, std::wstring(value));
    }
    // build callback(UTF-8)
    EXT_NODISCARD std::string BuildCallback() const;

private:
    // callback string
    std::wstring m_callbackString;
};

// create button with callbacks
TgBot::InlineKeyboardButton::Ptr create_keyboard_button(const std::wstring& text, const KeyboardCallback& callback);

} // namespace telegram::callback