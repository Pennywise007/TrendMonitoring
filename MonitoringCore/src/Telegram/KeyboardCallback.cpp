#include "pch.h"
#include "KeyboardCallback.h"

#include <regex>

namespace telegram::callback {

////////////////////////////////////////////////////////////////////////////////
// ������������ �������
KeyboardCallback::KeyboardCallback(const std::string& keyWord) noexcept
    : m_reportStr(keyWord.c_str())
{}

//----------------------------------------------------------------------------//
KeyboardCallback& KeyboardCallback::addCallbackParam(const std::string& param, const CString& value)
{
    // ������ ������ ���� ����
    // /%COMMAND% %PARAM_1%={'%VALUE_1%'} %PARAM_2%={'%VALUE_2%'} ...
    // ��������� ���� %PARAM_N%={'%VALUE_N'}
    m_reportStr.AppendFormat(L" %s={\'%s\'}", getUNICODEString(param).c_str(), value.GetString());

    return *this;
}

//----------------------------------------------------------------------------//
std::string KeyboardCallback::buildReport() const
{
    // ������ ������������ ��������, ���� �� ������������ - �������� ��������
    const std::wregex escapedCharacters(LR"(['])");
    const std::wstring rep(LR"(\$&)");

    // ��-�� ���� ��� TgTypeParser::appendToJson ��� �� ��������� '}'
    // ���� ��������� �������� �������� '}' ��������� � ����� ������
    const std::wstring resultReport = std::regex_replace((m_reportStr + " ").GetString(), escapedCharacters, rep).c_str();

    // ������������ ����������� �� ������ ������� ��������� 64 �����
    /*constexpr int maxReportSize = 64;

    const auto result = getUtf8Str(resultReport);
    assert(result.length() <= maxReportSize);*/
    return getUtf8Str(resultReport);
}

// �������� ������ � �������� � ���������, text ���������� ��� ������ ����� ����� ������������� � UTF-8, ����� ������ �� �����
TgBot::InlineKeyboardButton::Ptr create_keyboard_button(const CString& text, const KeyboardCallback& callback)
{
    TgBot::InlineKeyboardButton::Ptr channelButton(new TgBot::InlineKeyboardButton);

    // ����� ������ ���� � UTF-8
    channelButton->text = getUtf8Str(text.GetString());
    channelButton->callbackData = callback.buildReport();

    return channelButton;
}

} // namespace telegram::callback