#include "pch.h"

#include <include/ITrendMonitoring.h>
#include <TelegramDLL/TelegramThread.h>

#include "TestTelegramBot.h"

namespace telegram::bot {
////////////////////////////////////////////////////////////////////////////////
// проверяем реакцию на различных пользователей
TEST_F(TestTelegramBot, CheckCommandsAvailability)
{
    // ожидаемое сообщение телеграм боту
    CString expectedMessage;

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage);

    // по умолчанию у пользователя статус eNotAuthorized и у него нет доступных команд пока он не авторизуется
    expectedMessage = L"Для работы бота вам необходимо авторизоваться.";
    for (const auto& command : m_commandsToUserStatus)
    {
        emulateBroadcastMessage(L"/" + command.first);
    }
    // или любое рандомное сообщение кроме сообщения с авторизацией тоже должны ругаться
    emulateBroadcastMessage(L"132123");
    emulateBroadcastMessage(L"/22");

    {
        // авторизуем пользователя как обычного eOrdinaryUser
        expectedMessage = L"Пользователь успешно авторизован.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->GetUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eOrdinaryUser)
            << "У пользователя не соответствует статус после авторизации";

        // повторно отправляем сообщение авторизации
        expectedMessage = L"Пользователь уже авторизован.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->GetUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eOrdinaryUser)
            << "У пользователя не соответствует статус после авторизации";

        CString unknownMessageText = L"Неизвестная команда. Поддерживаемые команды бота:\n\n\n/info - Перечень команд бота.\n/report - Сформировать отчёт.\n\n\nДля того чтобы их использовать необходимо написать их этому боту(обязательно использовать слэш перед текстом команды(/)!). Или нажать в этом окне, они должны подсвечиваться.";

        // проверяем доступные обычному пользователю команды
        for (const auto& command : m_commandsToUserStatus)
        {
            if (command.second.find(users::ITelegramUsersList::UserStatus::eOrdinaryUser) == command.second.end())
            {
                // недоступная команда
                expectedMessage = unknownMessageText;
            }
            else
            {
                // доступная команда
                if (command.first == L"info")
                    expectedMessage = L"Поддерживаемые команды бота:\n\n\n/info - Перечень команд бота.\n/report - Сформировать отчёт.\n\n\nДля того чтобы их использовать необходимо написать их этому боту(обязательно использовать слэш перед текстом команды(/)!). Или нажать в этом окне, они должны подсвечиваться.";
                else
                {
                    EXPECT_TRUE(command.first == L"report") << "Неизвестный текст команды: " << command.first;
                    expectedMessage = L"Каналы для мониторинга не выбраны";
                }
            }
            // эмулируем отправку
            emulateBroadcastMessage(L"/" + command.first);
        }

        // проверяем рандомный текст
        expectedMessage = unknownMessageText;
        emulateBroadcastMessage(L"/123");
        emulateBroadcastMessage(L"123");
    }

    {
        // авторизуем пользователя как админа eAdmin
        expectedMessage = L"Пользователь успешно авторизован как администратор.";
        emulateBroadcastMessage(L"MonitoringAuthAdmin");
        EXPECT_EQ(m_pUserList->GetUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eAdmin)
            << "У пользователя не соответствует статус после авторизации";

        // повторно отправляем сообщение авторизации обычного пользователя
        expectedMessage = L"Пользователь является администратором системы. Авторизация не требуется.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->GetUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eAdmin)
            << "У пользователя не соответствует статус после авторизации";

        // повторно отправляем сообщение авторизации админа
        expectedMessage = L"Пользователь уже авторизован как администратор.";
        emulateBroadcastMessage(L"MonitoringAuthAdmin");
        EXPECT_EQ(m_pUserList->GetUserStatus(nullptr), users::ITelegramUsersList::UserStatus::eAdmin)
            << "У пользователя не соответствует статус после авторизации";

        // проверяем доступные обычному пользователю команды
        for (const auto& command : m_commandsToUserStatus)
        {
            EXPECT_TRUE(command.second.find(users::ITelegramUsersList::UserStatus::eAdmin) != command.second.end())
                << "У админа есть недоступная ему команда " << command.first;

            if (command.first == L"info")
                expectedMessage = m_adminCommandsInfo;
            else if (command.first == L"alertingOn" ||
                     command.first == L"alertingOff" ||
                     command.first == L"alarmingValue" ||
                     command.first == L"report")
                expectedMessage = L"Каналы для мониторинга не выбраны";
            else
            {
                EXPECT_TRUE(command.first == L"restart") << "Неизвестный текст команды: " << command.first;
                expectedMessage = L"Файл для перезапуска не найден.";
            }

            // эмулируем отправку
            emulateBroadcastMessage(L"/" + command.first);
        }

        // проверяем реакциюю на рандомный текст
        expectedMessage = L"Неизвестная команда. " + m_adminCommandsInfo;
        emulateBroadcastMessage(L"/123");
        emulateBroadcastMessage(L"123");
    }
}

TEST_F(TestTelegramBot, CheckUnauthorizedUser)
{
    // ожидаемое сообщение телеграм боту
    CString expectedMessage;

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage);

    // по умолчанию у пользователя статус eNotAuthorized и у него нет доступных команд пока он не авторизуется
    expectedMessage = L"Для работы бота вам необходимо авторизоваться.";
    for (const auto& command : m_commandsToUserStatus)
    {
        emulateBroadcastMessage(L"/" + command.first);
    }
    // или любое рандомное сообщение кроме сообщения с авторизацией тоже должны ругаться
    emulateBroadcastMessage(L"132123");
    emulateBroadcastMessage(L"/22");

    // check no sending error text
    m_testTelegramBot->SendMessageToUsers(L"Random message");
    m_testTelegramBot->SendMessageToAdmins(L"Random message");

    auto errorMessage = std::make_shared<IMonitoringErrorEvents::EventData>();
    errorMessage->errorTextForAllChannels = L"Error";
    ext::send_event(&IMonitoringErrorEvents::OnError, errorMessage);

    ext::send_event_async(&IMonitoringErrorEvents::OnError, L"Test report");
}
} // namespace telegram::bot