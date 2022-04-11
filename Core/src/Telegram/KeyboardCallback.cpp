#include "pch.h"
#include "KeyboardCallback.h"

#include <ext/std/string.h>

#include <regex>

namespace telegram::callback {

KeyboardCallback::KeyboardCallback(const std::string& keyWord) noexcept
    : m_callbackString(std::widen(keyWord.c_str()))
{}

KeyboardCallback& KeyboardCallback::AddCallbackParam(const std::string& param, const std::wstring& value)
{
    // callback format
    // /%COMMAND% %PARAM_1%={'%VALUE_1%'} %PARAM_2%={'%VALUE_2%'} ...
    // build pair %PARAM_N%={'%VALUE_N'}
    m_callbackString.append(std::string_swprintf(L" %s={\'%s\'}", getUNICODEString(param).c_str(), value.c_str()));

    return *this;
}

std::string KeyboardCallback::BuildCallback() const
{
    // Escaping characters list, not accepting by telegram
    const std::wregex escapedCharacters(LR"(['])");
    const std::wstring rep(LR"(\$&)");

    // because TgTypeParser::appendToJson don`t add '}'
    // if last symbol already precented as '}' - add manually space at the end
    const std::wstring resultCallback = std::regex_replace(m_callbackString + L" ", escapedCharacters, rep);

    // Maximum telegram callback size = 64 bytes
    /*constexpr int maxCallbackSize = 64;

    const auto result = getUtf8Str(resultReport);
    EXT_ASSERT(result.length() <= maxReportSize);*/
    return getUtf8Str(resultCallback);
}

TgBot::InlineKeyboardButton::Ptr create_keyboard_button(const std::wstring& text, const KeyboardCallback& callback)
{
    TgBot::InlineKeyboardButton::Ptr channelButton(new TgBot::InlineKeyboardButton);

    // text must be in UTF-8
    channelButton->text = getUtf8Str(text);
    channelButton->callbackData = callback.BuildCallback();

    return channelButton;
}

} // namespace telegram::callback