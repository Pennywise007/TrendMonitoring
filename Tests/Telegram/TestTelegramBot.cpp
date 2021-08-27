#include "pch.h"

#include <fstream>
#include <regex>

#include <boost/algorithm/string.hpp>

#include <tgbot/tgbot.h>

#include <TelegramDLL/TelegramThread.h>

#include <DirsService.h>
#include <include/ITrendMonitoring.h>
#include <Messages.h>

#include "TestHelper.h"
#include "TestTelegramBot.h"

#include <experimental/filesystem>

namespace telegram {

    // ������������� ���������� ������������ ���������
const int64_t kTelegramTestChatId = 1234;

namespace bot {

////////////////////////////////////////////////////////////////////////////////
void TestTelegramBot::SetUp()
{
    using namespace users;
    // �������������� ������ �������������
    m_pUserList = TestTelegramUsersList::create();

    m_pTelegramThread = new TestTelegramThread();

    // ������� �������� ����� ���������� � ���������� ��� ���������
    ITelegramThreadPtr pTelegramThread(m_pTelegramThread);
    // ������� ��������� ����
    TelegramBotSettings botSettings;
    botSettings.bEnable = true;
    botSettings.sToken = L"Testing";

    // �������������� ������ ������������� � ��������� ������ ��� ��������� ������ ���������, ��������� ��� � ��� unique_ptr
    m_testTelegramBot = std::make_unique<CTelegramBot>(m_pUserList, pTelegramThread.release());
    // ������� ���������
    m_testTelegramBot->setBotSettings(botSettings);

    // ��������� �������� ������ � ����������� � ��� ��������� �������������
    m_commandsToUserStatus[L"info"] = { ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };
    m_commandsToUserStatus[L"report"] = { ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };
    m_commandsToUserStatus[L"restart"] = { ITelegramUsersList::eAdmin };
    m_commandsToUserStatus[L"alertingOn"] = { ITelegramUsersList::eAdmin };
    m_commandsToUserStatus[L"alertingOff"] = { ITelegramUsersList::eAdmin };
    m_commandsToUserStatus[L"alarmingValue"] = { ITelegramUsersList::eAdmin };

    static_assert(ITelegramUsersList::eLastStatus == 3,
                  "������ ����������� ���������, �������� ����� ������������ ����������� ������");

    TestHelper& helper = get_service<TestHelper>();
    // ���������� ���������� ���������
    helper.resetMonitoringService();

    // ������� ���� �������� �������
    std::filesystem::remove(helper.getRestartFilePath());

    m_adminCommandsInfo =
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
void TestTelegramBot::emulateBroadcastCallbackQuery(LPCWSTR queryFormat, ...) const
{
    CString queryText;

    va_list args;
    va_start(args, queryFormat);
    queryText.FormatV(queryFormat, args);
    va_end(args);

    const TgBot::Message::Ptr pMessage = generateMessage(queryText.GetString());

    TgBot::Update::Ptr pUpdate = std::make_shared<TgBot::Update>();
    pUpdate->callbackQuery = std::make_shared<TgBot::CallbackQuery>();
    pUpdate->callbackQuery->from = pMessage->from;
    pUpdate->callbackQuery->message = pMessage;
    // ���� �������� ��� \\' �� ������ '
    pUpdate->callbackQuery->data = getUtf8Str(std::regex_replace(queryText.GetString(),
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

} // namespace bot

////////////////////////////////////////////////////////////////////////////////
TelegramUserMessagesChecker::TelegramUserMessagesChecker(bot::TestTelegramThread* pTelegramThread,
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

} // namespace telegram