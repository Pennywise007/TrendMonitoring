#include "pch.h"
#include "KeyboardCallback.h"

#include <regex>

namespace telegram::callback {

////////////////////////////////////////////////////////////////////////////////
// Формирование колбэка
KeyboardCallback::KeyboardCallback(const std::string& keyWord) noexcept
    : m_reportStr(keyWord.c_str())
{}

//----------------------------------------------------------------------------//
KeyboardCallback& KeyboardCallback::addCallbackParam(const std::string& param, const CString& value)
{
    // колбэк должен быть вида
    // /%COMMAND% %PARAM_1%={'%VALUE_1%'} %PARAM_2%={'%VALUE_2%'} ...
    // Формируем пару %PARAM_N%={'%VALUE_N'}
    m_reportStr.AppendFormat(L" %s={\'%s\'}", getUNICODEString(param).c_str(), value.GetString());

    return *this;
}

//----------------------------------------------------------------------------//
std::string KeyboardCallback::buildReport() const
{
    // Список экранируемых символов, если не экранировать - ругается телеграм
    const std::wregex escapedCharacters(LR"(['])");
    const std::wstring rep(LR"(\$&)");

    // из-за того что TgTypeParser::appendToJson сам не добавляет '}'
    // если последним символом является '}' добавляем в конце пробел
    const std::wstring resultReport = std::regex_replace((m_reportStr + " ").GetString(), escapedCharacters, rep).c_str();

    // максимальное ограничение не размер запроса телеграма 64 байта
    /*constexpr int maxReportSize = 64;

    const auto result = getUtf8Str(resultReport);
    assert(result.length() <= maxReportSize);*/
    return getUtf8Str(resultReport);
}

// создание кнопки с колбэком в телеграме, text передается как юникод чтобы потом преобразовать в UTF-8, иначе телега не умеет
TgBot::InlineKeyboardButton::Ptr create_keyboard_button(const CString& text, const KeyboardCallback& callback)
{
    TgBot::InlineKeyboardButton::Ptr channelButton(new TgBot::InlineKeyboardButton);

    // текст должен быть в UTF-8
    channelButton->text = getUtf8Str(text.GetString());
    channelButton->callbackData = callback.buildReport();

    return channelButton;
}

} // namespace telegram::callback