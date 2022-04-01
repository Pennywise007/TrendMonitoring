#include "pch.h"

#include <include/ITrendMonitoring.h>
#include <TelegramDLL/TelegramThread.h>

#include <src/Telegram/TelegramCallbacks.h>

#include "TestHelper.h"
#include "TestTelegramBot.h"

namespace telegram::bot {
using namespace callback;

////////////////////////////////////////////////////////////////////////////////
// ������� ������ ��� ������ �������������
TgBot::InlineKeyboardButton::Ptr generateKeyBoardButton(const std::wstring& text, LPCWSTR commandFormat, ...)
{
    va_list args;
    va_start(args, commandFormat);
    CString callbackText;
    callbackText.FormatV(commandFormat, args);
    va_end(args);

    TgBot::InlineKeyboardButton::Ptr button = std::make_shared<TgBot::InlineKeyboardButton>();
    button->text = getUtf8Str(text);
    button->callbackData = getUtf8Str(callbackText.GetString());
    return button;
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
    ITrendMonitoring* trendMonitoring = GetInterface<ITrendMonitoring>();
    const auto allChannels = trendMonitoring->GetNamesOfAllChannels();
    for (const auto& chan : allChannels)
    {
        const size_t chanInd = trendMonitoring->AddMonitoringChannel();
        trendMonitoring->ChangeMonitoringChannelNotify(chanInd, chan);
    }

    using namespace report;

    // ��������� ������������ �������
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(�����������) kParamInterval={'1000000'}
    expectedMessage = L"�� ����� ������� ������������ �����?";
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    expectedKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"��� ������",
                                                                  L"%hs %hs={\\\'0\\\'}", kKeyWord.c_str(), kParamType.c_str()) },
                                         { generateKeyBoardButton(L"������������ �����",
                                                                  L"%hs %hs={\\\'1\\\'}", kKeyWord.c_str(), kParamType.c_str()) } };
    expectedReply = expectedKeyboard;
    emulateBroadcastMessage(L"/report");

    // ��������� ����� �� ���� �������
    {
        expectedMessage = L"�������� �������� ������� �� ������� ����� �������� �����";
        expectedKeyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            CString callBackText;
            // /report type={\\\'0\\\'} interval={\\\'i\\\'}
            callBackText = std::string_swprintf(L"%hs %hs={\\\'0\\\'} %hs={\\\'%d\\\'}", kKeyWord.c_str(), kParamType.c_str(), kParamInterval.c_str(), i);

            expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)).GetString(), callBackText.GetString()) };
        }
        // ��������� ����� �� ���� �������
        // kKeyWord kParamType={'0'} kParamInterval={'1000000'}

        // /report type={'0'}
        emulateBroadcastCallbackQuery(L"%hs %hs={'0'}", kKeyWord.c_str(), kParamType.c_str());

        expectedMessage = L"����������� ������ ������, ��� ����� ������ ��������� �����.";
        expectedReply = std::make_shared<TgBot::GenericReply>();
        // ��������� ��� ���������
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            CString text;
            // /report type={'0'} interval={'i'}
            text = std::string_swprintf(L"%hs %hs={'0'} %hs={'%d'}", kKeyWord.c_str(), kParamType.c_str(), kParamInterval.c_str(), i);

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
            // /report type={\\\'1\\\'} chan={\\\'chan\\\'}
            callBackText = std::string_swprintf(L"%hs %hs={\\\'1\\\'} %hs={\\\'%s\\\'}", kKeyWord.c_str(), kParamType.c_str(), kParamChan.c_str(), chan.GetString());

            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.GetString(), callBackText.GetString()) });
        }
        // ��������� ��� ��������� ������� ����� �� ������� �� �������
        // /report type={'1'}");
        emulateBroadcastCallbackQuery(L"%hs %hs={'1'}", kKeyWord.c_str(), kParamType.c_str());

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
                // /report type={\\\'1\\\'} chan={\\\'chan\\\'} interval={\\\'i\\\'}
                callBackText = std::string_swprintf(L"%hs %hs={\\\'1\\\'} %hs={\\\'%s\\\'} %hs={\\\'%d\\\'}",
                                    kKeyWord.c_str(), kParamType.c_str(),
                                    kParamChan.c_str(), chan.GetString(),
                                    kParamInterval.c_str(), i);

                expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)).GetString(), callBackText.GetString()) };
            }

            CString text;
            // /report type={'1'} chan={'chan'}
            text = std::string_swprintf(L"%hs %hs={'1'} %hs={'%s'}", kKeyWord.c_str(), kParamType.c_str(), kParamChan.c_str(), chan.GetString());
            // ����������� ����� �� ������ chan, ������� ��� ��������� ��� �������� ����������
            emulateBroadcastCallbackQuery(text.GetString());

            // ������ �� �� �������� ������ ��� ������� ���� ������
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                CString callBackText;
                // /report chan={\\\'chan\\\'} interval={\\\'i\\\'}
                callBackText = std::string_swprintf(L"%hs %hs={\\\'%s\\\'} %hs={\\\'%d\\\'}",
                                    kKeyWord.c_str(), kParamChan.c_str(), chan.GetString(),
                                    kParamInterval.c_str(), i);

                expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)).GetString(), callBackText.GetString()) };
            }

            // ����������� ������ ��� ������� ���������
            expectedMessage = L"����������� ������ ������, ��� ����� ������ ��������� �����.";
            expectedReply = std::make_shared<TgBot::GenericReply>();
            // ��������� ��� ���������
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                // /report type={'1'} chan={'chan'} interval={'i'}
                text = std::string_swprintf(L"%hs %hs={'1'} %hs={'%s'} %hs={'%d'}",
                            kKeyWord.c_str(), kParamType.c_str(),
                            kParamChan.c_str(), chan.GetString(),
                            kParamInterval.c_str(), i);
                emulateBroadcastCallbackQuery(text.GetString());
            }

            // ��������� ��� ��� ������� type ���� ��������
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                // /report chan={'chan'} interval={'i'}
                text = std::string_swprintf(L"%hs  %hs={'%s'} %hs={'%d'}",
                            kKeyWord.c_str(), kParamChan.c_str(), chan.GetString(),
                            kParamInterval.c_str(), i);
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
    emulateBroadcastCallbackQuery(L"%hs", restart::kKeyWord.c_str());
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
    ITrendMonitoring* trendMonitoring = GetInterface<ITrendMonitoring>();
    const auto allChannels = trendMonitoring->GetNamesOfAllChannels();
    for (const auto& chan : allChannels)
    {
        const size_t chanInd = trendMonitoring->AddMonitoringChannel();
        trendMonitoring->ChangeMonitoringChannelNotify(chanInd, chan);
    }

    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    auto checkAlert = [&](bool alertOn)
    {
        using namespace alertEnabling;

        expectedReply = expectedKeyboard;

        // ��������� ���������/���������� ����������
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        expectedMessage = std::string_swprintf(L"�������� ����� ��� %s ����������.", alertOn ? L"���������" : L"����������");

        CString defCallBackText;
        // /alert enable={\\\'true\\\'}
        defCallBackText = std::string_swprintf(L"%hs %hs={\\\'%s\\\'}", kKeyWord.c_str(), kParamEnable.c_str(), alertOn ? L"true" : L"false");

        // ��������� ��� ������
        expectedKeyboard->inlineKeyboard.clear();
        for (const auto& chan : allChannels)
        {
            CString callBack;
            callBack = std::string_swprintf(L"%s %hs={\\\'%s\\\'}", defCallBackText.GetString(), kParamChan.c_str(), chan.GetString());
            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.GetString(), callBack.GetString()) });
        }
        // ��������� ������ ���� �������
        // chan={\\\'allChannels\\\'}
        defCallBackText.AppendFormat(L" %hs={\\\'%hs\\\'}", kParamChan.c_str(), kValueAllChannels.c_str());
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
                for (size_t i = 0, channelsCount = allChannels.size(); i < channelsCount; ++i)
                {
                    trendMonitoring->ChangeMonitoringChannelNotify(i, !alertOn);
                }

                expectedMessage = std::string_swprintf(L"���������� ��� ���� ������� %s",
                                       alertOn ? L"��������" : L"���������");
                emulateBroadcastCallbackQuery(getUNICODEString(callBackStr).c_str());

                // ��������� ��� ��������� ���������� ����������
                for (size_t i = 0, channelsCount = allChannels.size(); i < channelsCount; ++i)
                {
                    EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(i).bNotify, alertOn)
                        << "����� ���������� ���������� ������������ ��������� ���������� ����������!";
                }
            }
            else
            {
                // ������ �������� ���������������� ��������� ���������� � ������
                trendMonitoring->ChangeMonitoringChannelNotify(ind, !alertOn);

                expectedMessage = std::string_swprintf(L"���������� ��� ������ %s %s",
                                       std::next(allChannels.begin(), ind)->GetString(),
                                       alertOn ? L"��������" : L"���������");
                emulateBroadcastCallbackQuery(getUNICODEString(callBackStr).c_str());

                // ��������� ��� ��������� ���������� ����������
                EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(ind).bNotify, alertOn)
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
    ITrendMonitoring* trendMonitoring = GetInterface<ITrendMonitoring>();
    auto allChannels = trendMonitoring->GetNamesOfAllChannels();
    for (const auto& chan : allChannels)
    {
        trendMonitoring->ChangeMonitoringChannelNotify(trendMonitoring->AddMonitoringChannel(), chan);
    }

    using namespace alarmingValue;

    expectedMessage = L"�������� ����� ��� ��������� ������ ����������.";
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    for (const auto& chan : allChannels)
    {
        CString callBack;
        // /alarmV chan={\\\'chan\\\'}
        callBack = std::string_swprintf(L"%hs %hs={\\\'%s\\\'}", kKeyWord.c_str(), kParamChan.c_str(), chan.GetString());
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

            expectedMessage = std::string_swprintf(L"��� ���� ����� �������� ���������� ������� �������� � ������ '%s' ��������� ����� ������� �������� ����������, ��������� NAN ����� ��������� ���������� ������.",
                                   channelName.GetString());
            emulateBroadcastCallbackQuery(callBackData.c_str());
        };

        // ��������� ����������� ��������
        {
            initCommand();

            const float newValue = 3.09f;
            std::wstringstream newLevelText;
            newLevelText << newValue;

            //  val={\\'3.09\\'}
            CString appendText;
            appendText = std::string_swprintf(L" %hs={\\'%s\\'}", kParamValue.c_str(), newLevelText.str().c_str());

            callBackData += appendText;
            expectedMessage = std::string_swprintf(L"���������� �������� ���������� ��� ������ '%s' ��� %s?", channelName.GetString(), newLevelText.str().c_str());

            TgBot::InlineKeyboardMarkup::Ptr acceptKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            acceptKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"����������", callBackData.c_str()) } };
            expectedReply = acceptKeyboard;

            emulateBroadcastMessage(newLevelText.str());

            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage = std::string_swprintf(L"�������� %s ����������� ��� ������ '%s' �������", newLevelText.str().c_str(), channelName.GetString());
            // ���� �������� ��� \\' �� ������ \'
            emulateBroadcastCallbackQuery(callBackData.c_str());

            // ��������� ��� ����� ����������
            EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(ind).alarmingValue, newValue)
                << "����� ��������� ������ ���������� ����� ���� ������� �� ������������� ���������!";
        }

        {
            // ��������� ���������� ������
            initCommand();
            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage = L"����������� �������. " + m_adminCommandsInfo;
            emulateBroadcastMessage(L"�������");

            // ��������� NAN
            std::wstring newLevelText = L"NAN";

            //  val={\\'NAN\\'}
            CString appendText;
            appendText = std::string_swprintf(L" %hs={\\'%s\\'}", kParamValue.c_str(), newLevelText.c_str());

            expectedMessage = std::string_swprintf(L"��������� ���������� ��� ������ '%s'?", channelName.GetString());
            callBackData += appendText;

            TgBot::InlineKeyboardMarkup::Ptr acceptKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            acceptKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"����������", callBackData.c_str()) } };
            expectedReply = acceptKeyboard;

            emulateBroadcastMessage(newLevelText);

            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage = std::string_swprintf(L"���������� ��� ������ '%s' ���������", channelName.GetString());
            // ���� �������� ��� \\' �� ������ \'
            emulateBroadcastCallbackQuery(callBackData.c_str());

            // ��������� ��� ����� ����������
            EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(ind).bNotify, false)
                << "����� ���������� ���������� ��� �� ���������!";

            // ��������� ��������� NAN
            initCommand();

            expectedMessage = std::string_swprintf(L"���������� � ������ '%s' ��� ���������.", channelName.GetString());
            expectedReply = std::make_shared<TgBot::GenericReply>();
            emulateBroadcastMessage(newLevelText);
        }
    }
}

//----------------------------------------------------------------------------//
// ��������� ������ �����������
TEST_F(TestTelegramBot, TestProcessingMonitoringError)
{
    using namespace users;

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
        int64_t currentChatNumber = 1;
        auto registerChat = [&](const ITelegramUsersList::UserStatus userStatus)
        {
            switch (userStatus)
            {
            case ITelegramUsersList::UserStatus::eAdmin:
                adminChats.push_back(currentChatNumber);
                break;
            case ITelegramUsersList::UserStatus::eOrdinaryUser:
                userChats.push_back(currentChatNumber);
                break;
            default:
                ASSERT_FALSE("����������� ������������ � �� ������������ ��������");
                break;
            }

            m_pUserList->addUserChatidStatus(currentChatNumber++, userStatus);
        };

        for (int i = 0; i < 5; ++i)
        {
            registerChat(ITelegramUsersList::UserStatus::eAdmin);
            registerChat(ITelegramUsersList::UserStatus::eOrdinaryUser);
        }

        // ������������� ������ ���� ������������� ������ ������ ��� �����
        registerChat(ITelegramUsersList::UserStatus::eOrdinaryUser);
        registerChat(ITelegramUsersList::UserStatus::eOrdinaryUser);
    }

    // ������� �������� ������
    auto errorMessage = std::make_shared<MonitoringErrorEventData>();
    errorMessage->errorTextForAllChannels = errorMsg;
    // ������� ������������� ������
    if (!SUCCEEDED(CoCreateGuid(&errorMessage->errorGUID)))
        EXT_ASSERT(!"�� ������� ������� ����!");

    // ���������� ��������� ������������
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(L"������������� �������", CString(restart::kKeyWord.c_str()).GetString()),
                                                 generateKeyBoardButton(L"���������� ������� �������������",
                                                                        L"%hs %hs={\\\'%s\\\'}", resend::kKeyWord.c_str(), resend::kParamId.c_str(), CString(CComBSTR(errorMessage->errorGUID)).GetString()) });

    expectedReply = expectedKeyboard;
    expectedRecipientsChats = &adminChats;

    // ��������� ������������� ������
    get_service<CMassages>().SendMessage(onMonitoringErrorEvent, 0,
                                         std::static_pointer_cast<IEventData>(errorMessage));

    // ������ ��� �������
    m_pUserList->SetUserStatus(nullptr, ITelegramUsersList::UserStatus::eAdmin);

    // ��������� �������

    // ������� ������ ���� ������� ��� �������� ��������
    std::ofstream ofs(get_service<TestHelper>().getRestartFilePath());
    ofs.close();

    expectedReply = std::make_shared<TgBot::GenericReply>();
    expectedMessage = L"���������� ������� ��������������.";
    emulateBroadcastCallbackQuery(CString(restart::kKeyWord.c_str()).GetString());

    // ������� ���� �������� �������
    std::filesystem::remove(get_service<TestHelper>().getRestartFilePath());

    // ��������� ��������� ���������
    expectedRecipientsChats = &userChats;

    // ���������� ������ ���������
    expectedMessage = L"������������ ������ ��� � ������, �������� ������ �������� ���������� (�������� ��������� 200 ������) ��� ��������� ���� ������������.";
    const GUID testGuid = { 123 };
    // /resend errorId={'testGuid'}
    emulateBroadcastCallbackQuery(L"%hs %hs={'%s'}", resend::kKeyWord.c_str(), resend::kParamId.c_str(), CString(CComBSTR(testGuid)).GetString());

    // ���������� ��������
    expectedMessage = L"������ ���� ������� ��������� ������� �������������.";
    expectedMessageToRecipients = errorMsg;

    // /resend errorId={'errorGUID'}
    CString resendString;
    resendString = std::string_swprintf(L"%hs %hs={'%s'}", resend::kKeyWord.c_str(), resend::kParamId.c_str(), CString(CComBSTR(errorMessage->errorGUID)).GetString());
    emulateBroadcastCallbackQuery(resendString.GetString());

    // ��������� �� ������� ���������
    expectedMessage = L"������ ��� ���� ���������.";
    emulateBroadcastCallbackQuery(resendString.GetString());
}

//----------------------------------------------------------------------------//
// ��������� ��� �� �������������� ������������ �� ����� ������� � ���������� ������
TEST_F(TestTelegramBot, TestUsingCallbacksWithoutPermission)
{
    // ��������� ��������� �������� ����, �������� ��������� �� ������
    CString expectedMessage;

    // ����� � ����������
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // ����� ��� �������� ������� ������������
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply);

    std::list<CString> allCallbacksVariants;
    // /restart
    allCallbacksVariants.emplace_back() = std::string_swprintf(L"%hs", restart::kKeyWord.c_str());
    // /resend errorId={\\\'012312312312\\\'}
    allCallbacksVariants.emplace_back() = std::string_swprintf(L"%hs %hs={\\\'012312312312\\\'}", resend::kKeyWord.c_str(), resend::kParamId.c_str());
    // /report type={\\\'0\\\'} chan={\\\'RandomChannel\\\'}
    allCallbacksVariants.emplace_back() = std::string_swprintf(L"%hs %hs={\\\'0\\\'} %hs={\\\'RandomChannel\\\'}", report::kKeyWord.c_str(), report::kParamType.c_str(), report::kParamChan.c_str());
    // /alert enable={\\\'true\\\'} chan={\\\'allChannels\\\'}
    allCallbacksVariants.emplace_back() = std::string_swprintf(L"%hs %hs={\\\'true\\\'} %hs={\\\'%hs\\\'}",
                                               alertEnabling::kKeyWord.c_str(), alertEnabling::kParamEnable.c_str(),
                                               alertEnabling::kParamChan.c_str(), alertEnabling::kValueAllChannels.c_str());
    // /alarmV chan={\\\'RandomChannel\\\'} val={\\\'0.3\\\'}
    allCallbacksVariants.emplace_back() = std::string_swprintf(L"%hs %hs={\\\'RandomChannel\\\'} %hs={\\\'0.3\\\'}", alarmingValue::kKeyWord.c_str(), alarmingValue::kParamChan.c_str(), alarmingValue::kParamValue.c_str());

    // ������ ��� �����
    m_pUserList->SetUserStatus(nullptr, users::ITelegramUsersList::UserStatus::eNotAuthorized);

    expectedMessage = L"��� ������ ���� ��� ���������� ��������������.";
    for (const auto& callback : allCallbacksVariants)
    {
        emulateBroadcastCallbackQuery(callback.GetString());
    }
}

} // namespace telegram::bot