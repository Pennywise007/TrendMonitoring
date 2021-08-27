#pragma once

#include <afx.h>

namespace telegram::bot {

////////////////////////////////////////////////////////////////////////////////
// ��������� �������� ����
struct TelegramBotSettings
{
    // ��������� ������ ����
    bool bEnable = false;
    // ����� ����
    CString sToken;
};

////////////////////////////////////////////////////////////////////////////////
interface ITelegramBot
{
    virtual ~ITelegramBot() = default;

    // ��������� �������� ����
    virtual void setBotSettings(const TelegramBotSettings& botSettings) = 0;
    // ������� �������� ��������� ��������������� �������
    virtual void sendMessageToAdmins(const CString& message) const  = 0;
    // ������� �������� ��������� ������������� �������
    virtual void sendMessageToUsers(const CString& message) const = 0;
};

} // namespace telegram::bot