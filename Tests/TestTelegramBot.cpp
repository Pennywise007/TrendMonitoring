#include "pch.h"

#include <fstream>
#include <regex>

#include <boost/algorithm/string.hpp>

#include <tgbot/tgbot.h>

#include <TelegramDLL/TelegramThread.h>

#include <DirsService.h>
#include <ITrendMonitoring.h>
#include <Messages.h>

#include "TestHelper.h"
#include "TestTelegramBot.h"

// ������������� ���������� ������������ ���������
const int64_t kTelegramTestChatId = 1234;

////////////////////////////////////////////////////////////////////////////////
// ������� ������ ��� ������ �������������
TgBot::InlineKeyboardButton::Ptr generateKeyBoardButton(const std::wstring& text,
                                                        const std::wstring& callBack)
{
    TgBot::InlineKeyboardButton::Ptr button = std::make_shared<TgBot::InlineKeyboardButton>();
    button->text = getUtf8Str(text);
    button->callbackData = getUtf8Str(callBack);
    return button;
}

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
                expectedMessage = m_adminCommands;
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
        expectedMessage = L"����������� �������. " + m_adminCommands;
        emulateBroadcastMessage(L"/123");
        emulateBroadcastMessage(L"123");
    }
}

//----------------------------------------------------------------------------//
// ��������� ������ ������
TEST_F(TestTelegramBot, CheckReportCommandCallbacks)
{
    // ��������� ��������� �������� ����
    CString expectedMessage;
    // ����� � ����������
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // ����� ��� �������� ������� ������������
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply);

    // ������� �������� ������ �������, ������ ������
    expectedMessage = L"������������ ������� ����������� ��� �������������.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // ������ �������� ������� ����������� ����� � ���� ��������
    ITrendMonitoring* trendMonitoring = get_monitoring_service();
    std::set<CString> allChannels = trendMonitoring->getNamesOfAllChannels();
    for (auto& chan : allChannels)
    {
        size_t chanInd = trendMonitoring->addMonitoringChannel();
        trendMonitoring->changeMonitoringChannelName(chanInd, chan);
    }

    // ��������� ������������ �������
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(�����������) kParamInterval={'1000000'}
    expectedMessage = L"�� ����� ������� ������������ �����?";
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    expectedKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"��� ������", L"/report type={\\\'0\\\'}") },
                                         { generateKeyBoardButton(L"������������ �����", L"/report type={\\\'1\\\'}") } };
    expectedReply = expectedKeyboard;
    emulateBroadcastMessage(L"/report");

    // ��������� ����� �� ���� �������
    {
        expectedMessage = L"�������� �������� ������� �� ������� ����� �������� �����";
        expectedKeyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            CString callBackText;
            callBackText.Format(L"/report type={\\\'0\\\'} interval={\\\'%d\\\'}", i);

            expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)).GetString(), callBackText.GetString()) };
        }
        // ��������� ����� �� ���� �������
        // kKeyWord kParamType={'0'} kParamInterval={'1000000'}
        emulateBroadcastCallbackQuery(L"/report type={'0'}");

        expectedMessage = L"����������� ������ ������, ��� ����� ������ ��������� �����.";
        expectedReply = std::make_shared<TgBot::GenericReply>();
        // ��������� ��� ���������
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            CString text;
            text.Format(L"/report type={'0'} interval={'%d'}", i);

            // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(�����������) kParamInterval={'1000000'}
            emulateBroadcastCallbackQuery(text.GetString());
        }
    }

    // ��������� ����� �� ���������� ������
    {
        expectedMessage = L"�������� �����";
        expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        expectedReply = expectedKeyboard;
        for (auto& chan : allChannels)
        {
            CString callBackText;
            callBackText.Format(L"/report type={\\\'1\\\'} chan={\\\'%s\\\'}", chan.GetString());

            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.GetString(), callBackText.GetString()) });
        }
        // ��������� ����� �� ������������� ������
        emulateBroadcastCallbackQuery(L"/report type={'1'}");

        expectedKeyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);
        // ��������� ��� ��� ������� ������ ����� ����������� �����
        for (const auto& chan : allChannels)
        {
            expectedMessage = L"�������� �������� ������� �� ������� ����� �������� �����";
            expectedReply = expectedKeyboard;

            // ��� ������ ��� ������� ��������� �����������
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                CString callBackText;
                callBackText.Format(L"/report type={\\\'1\\\'} chan={\\\'%s\\\'} interval={\\\'%d\\\'}", chan.GetString(), i);

                expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)).GetString(), callBackText.GetString()) };
            }

            CString text;
            text.Format(L"/report type={'1'} chan={'%s'}", chan.GetString());
            // ����������� ���� �� ������ chan
            emulateBroadcastCallbackQuery(text.GetString());

            // ����������� ������ ��� ������� ���������
            expectedMessage = L"����������� ������ ������, ��� ����� ������ ��������� �����.";
            expectedReply = std::make_shared<TgBot::GenericReply>();
            // ��������� ��� ���������
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                CString text;
                text.Format(L"/report type={'1'} chan={'%s'} interval={'%d'}", chan.GetString(), i);
                emulateBroadcastCallbackQuery(text.GetString());
            }
        }
    }
}

//----------------------------------------------------------------------------//
// ��������� ������ �����������
TEST_F(TestTelegramBot, CheckRestartCommandCallbacks)
{
    // ��������� ��������� �������� ����
    CString expectedMessage;

    // ����� ��� �������� ������� ������������
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage);

    // ������� �������� ������ �������, ������ ������
    expectedMessage = L"������������ ������� ����������� ��� �������������.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // ������� ������ ���� ������� ��� �������� ��������
    std::ofstream ofs(get_service<TestHelper>().getRestartFilePath());
    ofs.close();

    expectedMessage = L"���������� ������� ��������������.";
    emulateBroadcastCallbackQuery(L"/restart");
}

//----------------------------------------------------------------------------//
// ��������� ������ �����������
TEST_F(TestTelegramBot, CheckAlertCommandCallbacks)
{
    // ��������� ��������� �������� ����
    CString expectedMessage;
    // ����� � ����������
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // ����� ��� �������� ������� ������������
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply);

    // ������� �������� ������ �������, ������ ������
    expectedMessage = L"������������ ������� ����������� ��� �������������.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // ������ �������� ������� ����������� ����� �� ��������� ����������
    ITrendMonitoring* trendMonitoring = get_monitoring_service();
    std::set<CString> allChannels = trendMonitoring->getNamesOfAllChannels();
    for (auto& chan : allChannels)
    {
        size_t chanInd = trendMonitoring->addMonitoringChannel();
        trendMonitoring->changeMonitoringChannelName(chanInd, chan);
    }

    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    auto checkAlert = [&](bool alertOn)
    {
        expectedReply = expectedKeyboard;

        // ��������� ���������/���������� ����������
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        expectedMessage.Format(L"�������� ����� ��� %s ����������.", alertOn ? L"���������" : L"����������");

        CString defCallBackText;
        defCallBackText.Format(L"/alert enable={\\\'%s\\\'}", alertOn ? L"true" : L"false");

        // ��������� ��� ������
        expectedKeyboard->inlineKeyboard.clear();
        for (auto& chan : allChannels)
        {
            CString callBack;
            callBack.Format(L"%s chan={\\\'%s\\\'}", defCallBackText.GetString(), chan.GetString());
            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.GetString(), callBack.GetString()) });
        }
        // ��������� ������ ���� �������
        defCallBackText.Append(L" chan={\\\'allChannels\\\'}");
        expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(L"��� ������", defCallBackText.GetString()) });

        // ��������� ������� ���������/����������
        emulateBroadcastMessage(alertOn ? L"/alertingOn" : L"/alertingOff");

        expectedReply = std::make_shared<TgBot::GenericReply>();
        // ��������� ��� ������ ������ ��� ����
        for (size_t ind = 0, count = expectedKeyboard->inlineKeyboard.size(); ind < count; ++ind)
        {
            const std::string& callBackStr = expectedKeyboard->inlineKeyboard[ind].front()->callbackData;

            // ��������� ��� ��� ��������� ������ �� ����� ��������
            if (ind == count - 1)
            {
                // ������ �������� ��������� � ����������
                for (size_t i = 0, count = allChannels.size(); i < count; ++i)
                {
                    trendMonitoring->changeMonitoringChannelNotify(i, !alertOn);
                }

                expectedMessage.Format(L"���������� ��� ���� ������� %s",
                                       alertOn ? L"��������" : L"���������");
                emulateBroadcastCallbackQuery(getUNICODEString(callBackStr));

                // ��������� ��� ��������� ���������� ����������
                for (size_t i = 0, count = allChannels.size(); i < count; ++i)
                {
                    EXPECT_EQ(trendMonitoring->getMonitoringChannelData(i).bNotify, alertOn)
                        << "����� ���������� ���������� ������������ ��������� ���������� ����������!";
                }
            }
            else
            {
                // ������ �������� ���������������� ��������� ���������� � ������
                trendMonitoring->changeMonitoringChannelNotify(ind, !alertOn);

                expectedMessage.Format(L"���������� ��� ������ %s %s",
                                       std::next(allChannels.begin(), ind)->GetString(),
                                       alertOn ? L"��������" : L"���������");
                emulateBroadcastCallbackQuery(getUNICODEString(callBackStr));

                // ��������� ��� ��������� ���������� ����������
                EXPECT_EQ(trendMonitoring->getMonitoringChannelData(ind).bNotify, alertOn)
                    << "����� ���������� ���������� ������������ ��������� ���������� ����������!";
            }
        }
    };

    // ��������� ��������� ����������
    checkAlert(true);

    // ��������� ���������� ����������
    checkAlert(false);
}

//----------------------------------------------------------------------------//
// ��������� ������ �����������
TEST_F(TestTelegramBot, CheckChangeAllarmingValueCallback)
{
    // ��������� ��������� �������� ����
    CString expectedMessage;
    // ����� � ����������
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // ����� ��� �������� ������� ������������
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply);

    // ������� �������� ������ �������, ������ ������
    expectedMessage = L"������������ ������� ����������� ��� �������������.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // ������ �������� ������� ����������� ����� �� ��������� ����������
    ITrendMonitoring* trendMonitoring = get_monitoring_service();
    std::set<CString> allChannels = trendMonitoring->getNamesOfAllChannels();
    for (auto& chan : allChannels)
    {
        trendMonitoring->changeMonitoringChannelName(trendMonitoring->addMonitoringChannel(), chan);
    }

    expectedMessage = L"�������� ����� ��� ��������� ������ ����������.";
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    for (const auto& chan : allChannels)
    {
        CString callBack;
        callBack.Format(L"/alarmV chan={\\\'%s\\\'}", chan.GetString());
        expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.GetString(), callBack.GetString()) });
    }
    expectedReply = expectedKeyboard;

    // ��������� ������� ��������� ������ ����������
    emulateBroadcastMessage(L"/alarmingValue");

    // ��������� ��� ������ ������ ��� ����
    for (size_t ind = 0, count = expectedKeyboard->inlineKeyboard.size(); ind < count; ++ind)
    {
        const CString channelName = *std::next(allChannels.begin(), ind);
        std::wstring callBackData;

        auto initCommand = [&]()
        {
            expectedReply = std::make_shared<TgBot::GenericReply>();
            callBackData = getUNICODEString(expectedKeyboard->inlineKeyboard[ind].front()->callbackData);

            expectedMessage.Format(L"��� ���� ����� �������� ���������� ������� �������� � ������ '%s' ��������� ����� ������� �������� ����������, ��������� NAN ����� ��������� ���������� ������.",
                                   channelName.GetString());
            emulateBroadcastCallbackQuery(callBackData);
        };

        // ��������� ����������� ��������
        {
            initCommand();

            const float newValue = 3.09f;
            std::wstringstream newLevelText;
            newLevelText << newValue;

            expectedMessage.Format(L"���������� �������� ���������� ��� ������ '%s' ��� %s?", channelName.GetString(), newLevelText.str().c_str());
            callBackData += L" val={\\'3.09\\'}";

            TgBot::InlineKeyboardMarkup::Ptr acceptKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            acceptKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"����������", callBackData) } };
            expectedReply = acceptKeyboard;

            emulateBroadcastMessage(newLevelText.str());

            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage.Format(L"�������� %s ����������� ��� ������ '%s' �������", newLevelText.str().c_str(), channelName.GetString());
            // ���� �������� ��� \\' �� ������ \'
            emulateBroadcastCallbackQuery(callBackData);

            // ��������� ��� ����� ����������
            EXPECT_EQ(trendMonitoring->getMonitoringChannelData(ind).alarmingValue, newValue)
                << "����� ��������� ������ ���������� ����� ���� ������� �� ������������� ���������!";
        }

        {
            // ��������� ���������� ������
            initCommand();
            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage = L"����������� �������. " + m_adminCommands;
            emulateBroadcastMessage(L"�������");

            // ��������� NAN
            const float newValue = NAN;
            std::wstring newLevelText = L"NAN";

            expectedMessage.Format(L"��������� ���������� ��� ������ '%s'?", channelName.GetString());
            callBackData += L" val={\\'NAN\\'}";

            TgBot::InlineKeyboardMarkup::Ptr acceptKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            acceptKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"����������", callBackData) } };
            expectedReply = acceptKeyboard;

            emulateBroadcastMessage(newLevelText);

            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage.Format(L"���������� ��� ������ '%s' ���������", channelName.GetString());
            // ���� �������� ��� \\' �� ������ \'
            emulateBroadcastCallbackQuery(callBackData);

            // ��������� ��� ����� ����������
            EXPECT_EQ(trendMonitoring->getMonitoringChannelData(ind).bNotify, false)
                << "����� ���������� ���������� ��� �� ���������!";

            // ��������� ��������� NAN
            initCommand();

            expectedMessage.Format(L"���������� � ������ '%s' ��� ���������.", channelName.GetString());
            expectedReply = std::make_shared<TgBot::GenericReply>();
            emulateBroadcastMessage(newLevelText);
        }
    }
}

//----------------------------------------------------------------------------//
// ��������� ������ �����������
TEST_F(TestTelegramBot, TestProcessingMonitoringError)
{
    const CString errorMsg = L"�������� ��������� 111235�������1��asd 41234%$#@$$%6sfda";

    // ��������� ��������� �������� ����, �������� ��������� �� ������
    CString expectedMessage = errorMsg;
    CString expectedMessageToRecipients = errorMsg;

    // ����� � ����������
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();
    // ��������� ������ �����������
    std::list<int64_t>* expectedRecipientsChats = nullptr;

    // ����� ��� �������� ������� ������������
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply, &expectedRecipientsChats, &expectedMessageToRecipients);

    // �������� ����� ������� ������������� � �������
    std::list<int64_t> userChats;
    std::list<int64_t> adminChats;
    // ������������ ������ ����� � ��������
    {
        auto registerChat = [&](const int64_t chatId, const ITelegramUsersList::UserStatus userStatus)
        {
            switch (userStatus)
            {
            case ITelegramUsersList::UserStatus::eAdmin:
                adminChats.push_back(chatId);
                break;
            case ITelegramUsersList::UserStatus::eOrdinaryUser:
                userChats.push_back(chatId);
                break;
            default:
                ASSERT_FALSE("����������� ������������ � �� ������������ ��������");
                break;
            }

            m_pUserList->addUserChatidStatus(chatId, userStatus);
        };

        for (int i = 0; i < 5; ++i)
        {
            registerChat(std::rand() % 15, ITelegramUsersList::UserStatus::eAdmin);
            registerChat(std::rand() % 15, ITelegramUsersList::UserStatus::eOrdinaryUser);
        }

        // ������������� ������ ���� ������������� ������ ������ ��� �����
        registerChat(std::rand() % 15, ITelegramUsersList::UserStatus::eOrdinaryUser);
        registerChat(std::rand() % 15, ITelegramUsersList::UserStatus::eOrdinaryUser);
    }

    // ������� �������� ������
    auto errorMessage = std::make_shared<MonitoringErrorEventData>();
    errorMessage->errorText = errorMsg;
    // ������� ������������� ������
    if (!SUCCEEDED(CoCreateGuid(&errorMessage->errorGUID)))
        assert(!"�� ������� ������� ����!");

    // ���������� ��������� ������������
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(L"������������� �������", L"/restart"),
                                                 generateKeyBoardButton(L"���������� ������� �������������",
                                                                        std::wstring(L"/resend errorId={\\\'") + CString(CComBSTR(errorMessage->errorGUID)).GetString() + L"\\\'}") });

    expectedReply = expectedKeyboard;
    expectedRecipientsChats = &adminChats;

    // ��������� ������������� ������
    get_service<CMassages>().sendMessage(onMonitoringErrorEvent, 0,
                                         std::static_pointer_cast<IEventData>(errorMessage));

    // ������ ��� �������
    m_pUserList->setUserStatus(nullptr, ITelegramUsersList::UserStatus::eAdmin);

    // ��������� �������

    // ������� ������ ���� ������� ��� �������� ��������
    std::ofstream ofs(get_service<TestHelper>().getRestartFilePath());
    ofs.close();

    expectedReply = std::make_shared<TgBot::GenericReply>();
    expectedMessage = L"���������� ������� ��������������.";
    emulateBroadcastCallbackQuery(L"/restart");

    // ��������� ��������� ���������
    expectedRecipientsChats = &userChats;

    // ���������� ������ ���������
    expectedMessage = L"������������ ������ ��� � ������, �������� ������ �������� ���������� (�������� ��������� 200 ������) ��� ��������� ���� ������������.";
    const GUID testGuid = { 123 };
    emulateBroadcastCallbackQuery(std::wstring(L"/resend errorId={'") + CString(CComBSTR(testGuid)).GetString() + L"'}");

    // ���������� ��������
    expectedMessage = L"������ ���� ������� ��������� ������� �������������.";
    expectedMessageToRecipients = errorMsg;
    emulateBroadcastCallbackQuery(std::wstring(L"/resend errorId={'") + CString(CComBSTR(errorMessage->errorGUID)).GetString() + L"'}");

    // ��������� �� ������� ���������
    expectedMessage = L"������ ��� ���� ���������.";
    emulateBroadcastCallbackQuery(std::wstring(L"/resend errorId={'") + CString(CComBSTR(errorMessage->errorGUID)).GetString() + L"'}");
}

////////////////////////////////////////////////////////////////////////////////
void TestTelegramBot::SetUp()
{
    // �������������� ������ �������������
    m_pUserList = TestTelegramUsersList::create();

    m_pTelegramThread = new TestTelegramThread();

    // ������� �������� ����� ���������� � ���������� ��� ���������
    ITelegramThreadPtr pTelegramThread(m_pTelegramThread);
    // ������� ��������� ����
    TelegramBotSettings botSettings;
    botSettings.bEnable = true;
    botSettings.sToken = L"Testing";

    // �������������� ������ �������������
    m_testTelegramBot.initBot(m_pUserList);
    // ������������� ��������� ������ ��� ��������� ������ ���������, ��������� ��� � ��� unique_ptr
    m_testTelegramBot.setDefaultTelegramThread(pTelegramThread);
    // ������� ���������
    m_testTelegramBot.setBotSettings(botSettings);

    // ��������� �������� ������ � ����������� � ��� ��������� �������������
    m_commandsToUserStatus[L"info"]             = { ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };
    m_commandsToUserStatus[L"report"]           = { ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };
    m_commandsToUserStatus[L"restart"]          = { ITelegramUsersList::eAdmin };
    m_commandsToUserStatus[L"alertingOn"]       = { ITelegramUsersList::eAdmin };
    m_commandsToUserStatus[L"alertingOff"]      = { ITelegramUsersList::eAdmin };
    m_commandsToUserStatus[L"alarmingValue"]    = { ITelegramUsersList::eAdmin };

    static_assert(ITelegramUsersList::eLastStatus == 3,
                  "������ ����������� ���������, �������� ����� ������������ ����������� ������");

    get_service<TestHelper>().resetMonitoringService();

    m_adminCommands =
        L"�������������� ������� ����:\n\n\n"
        L"/info - �������� ������ ����.\n"
        L"/report - ������������ �����.\n"
        L"/restart - ������������� ������� �����������.\n"
        L"/alertingOn - �������� ���������� � ��������.\n"
        L"/alertingOff - ��������� ���������� � ��������.\n"
        L"/alarmingValue - �������� ���������� ������� �������� � ������.\n\n\n"
        L"��� ���� ����� �� ������������ ���������� �������� �� ����� ����(����������� ������������ ���� ����� ������� �������(/)!). ��� ������ � ���� ����, ��� ������ ��������������.";
}

//----------------------------------------------------------------------------//
void TestTelegramBot::emulateBroadcastMessage(const std::wstring& text) const
{
    TgBot::Update::Ptr pUpdate = std::make_shared<TgBot::Update>();
    pUpdate->message = generateMessage(text);

    HandleTgUpdate(m_pTelegramThread->m_botApi->getEventHandler(), pUpdate);
}

//----------------------------------------------------------------------------//
void TestTelegramBot::emulateBroadcastCallbackQuery(const std::wstring& text) const
{
    const TgBot::Message::Ptr pMessage = generateMessage(text);

    TgBot::Update::Ptr pUpdate = std::make_shared<TgBot::Update>();
    pUpdate->callbackQuery = std::make_shared<TgBot::CallbackQuery>();
    pUpdate->callbackQuery->from = pMessage->from;
    pUpdate->callbackQuery->message = pMessage;
    // ���� �������� ��� \\' �� ������ '
    pUpdate->callbackQuery->data = getUtf8Str(std::regex_replace(text,
                                                                 std::wregex(LR"(\\')"),
                                                                 L"'"));

    HandleTgUpdate(m_pTelegramThread->m_botApi->getEventHandler(), pUpdate);
}

//----------------------------------------------------------------------------//
TgBot::Message::Ptr TestTelegramBot::generateMessage(const std::wstring& text) const
{
    TgBot::Message::Ptr pMessage = std::make_shared<TgBot::Message>();
    pMessage->from = std::make_shared<TgBot::User>();
    pMessage->from->id = kTelegramTestChatId;
    pMessage->from->firstName = "Bot";
    pMessage->from->lastName = "Test";
    pMessage->from->username = "TestBot";
    pMessage->from->languageCode = "Ru";

    pMessage->chat = std::make_shared<TgBot::Chat>();
    pMessage->chat->id = kTelegramTestChatId;
    pMessage->chat->type = TgBot::Chat::Type::Private;
    pMessage->chat->title = "Test conversation";
    pMessage->chat->firstName = "Bot";
    pMessage->chat->lastName = "Test";
    pMessage->chat->username = "TestBot";

    pMessage->text = getUtf8Str(text);

    return pMessage;
}

////////////////////////////////////////////////////////////////////////////////
TelegramUserMessagesChecker::TelegramUserMessagesChecker(TestTelegramThread* pTelegramThread,
                                                         CString* pExpectedMessage,
                                                         TgBot::GenericReply::Ptr* pExpectedReply /*= nullptr*/,
                                                         std::list<int64_t>** pExpectedRecipientsChats /*= nullptr*/,
                                                         CString* pExpectedMessageToRecipients /*= nullptr*/)
{
    // ��������� ���� �������(��������� ������������)
    auto compareReply = [](const TgBot::GenericReply::Ptr realReply,
                           const TgBot::GenericReply::Ptr expectedReply) -> bool
    {
        if (realReply == expectedReply)
            return true;

        if (!realReply || !expectedReply)
            return false;

        TgBot::InlineKeyboardMarkup::Ptr keyBoardMarkupReal =
            std::dynamic_pointer_cast<TgBot::InlineKeyboardMarkup>(realReply);
        TgBot::InlineKeyboardMarkup::Ptr keyBoardMarkupExpected =
            std::dynamic_pointer_cast<TgBot::InlineKeyboardMarkup>(expectedReply);

        EXPECT_EQ(!keyBoardMarkupReal, !keyBoardMarkupExpected) << "���� ������� �� ������������� ���� �����.";

        if (keyBoardMarkupReal && keyBoardMarkupExpected)
        {
            // ���������� ��� InlineKeyboardMarkup
            std::vector<std::vector<TgBot::InlineKeyboardButton::Ptr>> keyboardReal =
                keyBoardMarkupReal->inlineKeyboard;
            std::vector<std::vector<TgBot::InlineKeyboardButton::Ptr>> keyboardExpected =
                keyBoardMarkupExpected->inlineKeyboard;

            EXPECT_EQ(keyboardReal.size(), keyboardExpected.size()) << "������ ���������� ��� ������ �����������";

            for (size_t row = 0, countRows = keyboardReal.size(); row < countRows; ++row)
            {
                EXPECT_EQ(keyboardReal[row].size(), keyboardExpected[row].size()) << "���������� ������ � ������ " << row << " �����������";

                for (size_t col = 0, countColumns = keyboardReal[row].size(); col < countColumns; ++col)
                {
                    TgBot::InlineKeyboardButton::Ptr realKey = keyboardReal[row][col];
                    TgBot::InlineKeyboardButton::Ptr expectedKey = keyboardExpected[row][col];

                    // ���������� ������
                    EXPECT_TRUE(realKey->text == expectedKey->text) << "����� �� ������� ����������.\n�������� :" <<
                        CStringA(getUNICODEString(realKey->text).c_str()) << "\n���������:" << CStringA(getUNICODEString(expectedKey->text).c_str());

                    boost::trim(realKey->callbackData);
                    boost::trim(expectedKey->callbackData);

                    EXPECT_TRUE(realKey->callbackData == expectedKey->callbackData) << "������ � ������ ����������.\n�������� :" <<
                        CStringA(getUNICODEString(realKey->callbackData).c_str()) << "\n���������:" << CStringA(getUNICODEString(expectedKey->callbackData).c_str());
                }
            }
        }

        return true;
    };

    pTelegramThread->onSendMessage(
        [pExpectedMessage, pExpectedReply, &compareReply]
        (int64_t chatId, const std::wstring& msg, bool disableWebPagePreview,
         int32_t replyToMessageId, TgBot::GenericReply::Ptr replyMarkup,
         const std::string& parseMode, bool disableNotification)
        {
            ASSERT_TRUE(pExpectedMessage) << "�� �������� ���������.";

            EXPECT_EQ(chatId, kTelegramTestChatId) << "������������� ���� � ����������� ��������� �� ���������";
            EXPECT_TRUE(msg == pExpectedMessage->GetString()) << "������ ��������� ������������ �� ����������.\n\n���������:\n" <<
                CStringA(msg.c_str()) << "\n\n���������:\n" << CStringA(*pExpectedMessage);

            if (pExpectedReply)
                EXPECT_TRUE(compareReply(replyMarkup, *pExpectedReply));
        });

    pTelegramThread->onSendMessageToChats(
        [pExpectedRecipientsChats, pExpectedReply, pExpectedMessageToRecipients, &compareReply]
         (const std::list<int64_t>& chatIds, const std::wstring& msg, bool disableWebPagePreview,
         int32_t replyToMessageId, TgBot::GenericReply::Ptr replyMarkup,
         const std::string& parseMode, bool disableNotification)
        {
            ASSERT_TRUE(pExpectedRecipientsChats) << "�� �������� ������ �����.";
            ASSERT_TRUE(pExpectedMessageToRecipients) << "�� �������� ��������� ��� �����������.";

            EXPECT_EQ(chatIds.size(), (*pExpectedRecipientsChats)->size()) << "��������� ���� � ����������� �� �������.";
            auto chatId = chatIds.begin();
            auto expectedId = (*pExpectedRecipientsChats)->begin();
            for (size_t i = 0, count = std::min<size_t>(chatIds.size(), (*pExpectedRecipientsChats)->size());
                 i < count; ++i, ++chatId, ++expectedId)
            {
                EXPECT_EQ(*chatId, *expectedId) << "��������� ���� � ����������� �� �������.";;
            }

            EXPECT_TRUE(msg == pExpectedMessageToRecipients->GetString()) << "������ ��������� ������������ �� ����������.\n\n���������:\n" <<
                CStringA(msg.c_str()) << "\n\n���������:\n" << CStringA(*pExpectedMessageToRecipients);

            if (pExpectedReply)
                EXPECT_TRUE(compareReply(replyMarkup, *pExpectedReply));
        });
}