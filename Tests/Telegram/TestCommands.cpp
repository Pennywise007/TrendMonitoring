#include "pch.h"

#include <include/ITrendMonitoring.h>
#include <TelegramDLL/TelegramThread.h>

#include "TestTelegramBot.h"

////////////////////////////////////////////////////////////////////////////////
// �������� ������� ������������ ������ ����
TEST_F(TestTelegramBot, CheckCommandListeners)
{
    TgBot::EventBroadcaster& botEvents = m_pTelegramThread->getBotEvents();
    // ��������� ������� ������������
    auto& commandListeners = botEvents.getCommandListeners();
    for (const auto& command : m_commandsToUserStatus)
    {
        EXPECT_NE(commandListeners.find(CStringA(command.first.c_str()).GetString()), commandListeners.end())
            << "� ������� ���� \"" + CStringA(command.first.c_str()) + "\" ����������� ����������";
    }

    EXPECT_EQ(commandListeners.size(), 6) << "���������� ������������ ������ � ����� ������ �� ���������";
}

//----------------------------------------------------------------------------//
// ��������� ������� �� ��������� �������������
TEST_F(TestTelegramBot, CheckCommandsAvailability)
{
    // ��������� ��������� �������� ����
    CString expectedMessage;

    // ����� ��� �������� ������� ������������
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage);

    // �� ��������� � ������������ ������ eNotAuthorized � � ���� ��� ��������� ������ ���� �� �� ������������
    expectedMessage = L"��� ������ ���� ��� ���������� ��������������.";
    for (const auto& command : m_commandsToUserStatus)
    {
        emulateBroadcastMessage(L"/" + command.first);
    }
    // ��� ����� ��������� ��������� ����� ��������� � ������������ ���� ������ ��������
    emulateBroadcastMessage(L"132123");
    emulateBroadcastMessage(L"/22");

    {
        // ���������� ������������ ��� �������� eOrdinaryUser
        expectedMessage = L"������������ ������� �����������.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eOrdinaryUser)
            << "� ������������ �� ������������� ������ ����� �����������";

        // �������� ���������� ��������� �����������
        expectedMessage = L"������������ ��� �����������.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eOrdinaryUser)
            << "� ������������ �� ������������� ������ ����� �����������";

        CString unknownMessageText = L"����������� �������. �������������� ������� ����:\n\n\n/info - �������� ������ ����.\n/report - ������������ �����.\n\n\n��� ���� ����� �� ������������ ���������� �������� �� ����� ����(����������� ������������ ���� ����� ������� �������(/)!). ��� ������ � ���� ����, ��� ������ ��������������.";

        // ��������� ��������� �������� ������������ �������
        for (const auto& command : m_commandsToUserStatus)
        {
            if (command.second.find(ITelegramUsersList::UserStatus::eOrdinaryUser) == command.second.end())
            {
                // ����������� �������
                expectedMessage = unknownMessageText;
            }
            else
            {
                // ��������� �������
                if (command.first == L"info")
                    expectedMessage = L"�������������� ������� ����:\n\n\n/info - �������� ������ ����.\n/report - ������������ �����.\n\n\n��� ���� ����� �� ������������ ���������� �������� �� ����� ����(����������� ������������ ���� ����� ������� �������(/)!). ��� ������ � ���� ����, ��� ������ ��������������.";
                else
                {
                    EXPECT_TRUE(command.first == L"report") << "����������� ����� �������: " << command.first;
                    expectedMessage = L"������ ��� ����������� �� �������";
                }
            }
            // ��������� ��������
            emulateBroadcastMessage(L"/" + command.first);
        }

        // ��������� ��������� �����
        expectedMessage = unknownMessageText;
        emulateBroadcastMessage(L"/123");
        emulateBroadcastMessage(L"123");
    }

    {
        // ���������� ������������ ��� ������ eAdmin
        expectedMessage = L"������������ ������� ����������� ��� �������������.";
        emulateBroadcastMessage(L"MonitoringAuthAdmin");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eAdmin)
            << "� ������������ �� ������������� ������ ����� �����������";

        // �������� ���������� ��������� ����������� �������� ������������
        expectedMessage = L"������������ �������� ��������������� �������. ����������� �� ���������.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eAdmin)
            << "� ������������ �� ������������� ������ ����� �����������";

        // �������� ���������� ��������� ����������� ������
        expectedMessage = L"������������ ��� ����������� ��� �������������.";
        emulateBroadcastMessage(L"MonitoringAuthAdmin");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eAdmin)
            << "� ������������ �� ������������� ������ ����� �����������";

        // ��������� ��������� �������� ������������ �������
        for (const auto& command : m_commandsToUserStatus)
        {
            EXPECT_TRUE(command.second.find(ITelegramUsersList::UserStatus::eAdmin) != command.second.end())
                << "� ������ ���� ����������� ��� ������� " << command.first;

            if (command.first == L"info")
                expectedMessage = m_adminCommandsInfo;
            else if (command.first == L"alertingOn"     ||
                     command.first == L"alertingOff"    ||
                     command.first == L"alarmingValue"  ||
                     command.first == L"report")
                expectedMessage = L"������ ��� ����������� �� �������";
            else
            {
                EXPECT_TRUE(command.first == L"restart") << "����������� ����� �������: " << command.first;
                expectedMessage = L"���� ��� ����������� �� ������.";
            }

            // ��������� ��������
            emulateBroadcastMessage(L"/" + command.first);
        }

        // ��������� �������� �� ��������� �����
        expectedMessage = L"����������� �������. " + m_adminCommandsInfo;
        emulateBroadcastMessage(L"/123");
        emulateBroadcastMessage(L"123");
    }
}
