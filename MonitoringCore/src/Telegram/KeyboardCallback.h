#pragma once

#include <afxstr.h>
#include <string>

#include <TelegramDLL/TelegramThread.h>

namespace telegram::callback {

////////////////////////////////////////////////////////////////////////////////
// ����� ��� ������������ ������� ��� ������ ���������, �� ������ UTF-8
class KeyboardCallback
{
public:
    // ����������� �����������
    explicit KeyboardCallback(const std::string& keyWord) noexcept;
    KeyboardCallback(const KeyboardCallback& callback) = default;

    // �������� ������ ��� ������� � ����� �������� - ��������
    KeyboardCallback& addCallbackParam(const std::string& param, const CString& value);
    template <typename CharType, typename Traits, typename Alloc>
    KeyboardCallback& addCallbackParam(const std::string& param, const std::basic_string<CharType, Traits, Alloc>& value)
    {
        if constexpr (std::is_same_v<CharType, char>)
            return addCallbackParam(param, CString(getUNICODEString(value).c_str()));
        else
            return addCallbackParam(param, CString(value.c_str()));
    }
    // ������������ ������(UTF-8)
    std::string buildReport() const;

private:
    // ������ ������
    CString m_reportStr;
};

// �������� ������ � �������
TgBot::InlineKeyboardButton::Ptr create_keyboard_button(const CString& text, const KeyboardCallback& callback);

} // namespace telegram::callback