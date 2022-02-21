#include "pch.h"

#include <include/ITrendMonitoring.h>
#include <TelegramDLL/TelegramThread.h>

#include "TestTelegramBot.h"

namespace telegram::bot {
////////////////////////////////////////////////////////////////////////////////
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
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eOrdinaryUser)
            << "� ������������ �� ������������� ������ ����� �����������";

        // �������� ���������� ��������� �����������
        expectedMessage = L"������������ ��� �����������.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eOrdinaryUser)
            << "� ������������ �� ������������� ������ ����� �����������";

        CString unknownMessageText = L"����������� �������. �������������� ������� ����:\n\n\n/info - �������� ������ ����.\n/report - ������������ �����.\n\n\n��� ���� ����� �� ������������ ���������� �������� �� ����� ����(����������� ������������ ���� ����� ������� �������(/)!). ��� ������ � ���� ����, ��� ������ ��������������.";

        // ��������� ��������� �������� ������������ �������
        for (const auto& command : m_commandsToUserStatus)
        {
            if (command.second.find(users::ITelegramUsersList::UserStatus::eOrdinaryUser) == command.second.end())
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
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eAdmin)
            << "� ������������ �� ������������� ������ ����� �����������";

        // �������� ���������� ��������� ����������� �������� ������������
        expectedMessage = L"������������ �������� ��������������� �������. ����������� �� ���������.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eAdmin)
            << "� ������������ �� ������������� ������ ����� �����������";

        // �������� ���������� ��������� ����������� ������
        expectedMessage = L"������������ ��� ����������� ��� �������������.";
        emulateBroadcastMessage(L"MonitoringAuthAdmin");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eAdmin)
            << "� ������������ �� ������������� ������ ����� �����������";

        // ��������� ��������� �������� ������������ �������
        for (const auto& command : m_commandsToUserStatus)
        {
            EXPECT_TRUE(command.second.find(users::ITelegramUsersList::UserStatus::eAdmin) != command.second.end())
                << "� ������ ���� ����������� ��� ������� " << command.first;

            if (command.first == L"info")
                expectedMessage = m_adminCommandsInfo;
            else if (command.first == L"alertingOn" ||
                     command.first == L"alertingOff" ||
                     command.first == L"alarmingValue" ||
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

TEST_F(TestTelegramBot, CheckUnauthorizedUser)
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

    // check no sending error text
    m_testTelegramBot->sendMessageToUsers(L"Random message");
    m_testTelegramBot->sendMessageToAdmins(L"Random message");

    auto errorMessage = std::make_shared<MonitoringErrorEventData>();
    errorMessage->errorTextForAllChannels = L"Error";
    get_service<CMassages>().sendMessage(onMonitoringErrorEvent, 0,
                                         std::static_pointer_cast<IEventData>(errorMessage));

    auto reportMessage = std::make_shared<MessageTextData>();
    reportMessage->messageText = L"Test report";
    get_service<CMassages>().postMessage(onReportPreparedEvent, 0,
                                         std::static_pointer_cast<IEventData>(reportMessage));

}
} // namespace telegram::bot