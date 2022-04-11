#pragma once

#include <functional>
#include <map>
#include <memory>

#include <gtest/gtest.h>

#include <ext/core/dispatcher.h>
#include <ext/serialization/iserializable.h>

#include <TelegramThread.h>

#include <include/ITelegramUsersList.h>
#include <src/Telegram/TelegramBot.h>

#include <mocks/TelegramThreadMock.h>

namespace telegram {
namespace users {

////////////////////////////////////////////////////////////////////////////////
// Класс хранящий список пользователей телеграма
// позволяет работать со списком пользователей из нескольких потоков
class TestTelegramUsersList 
    : public ext::serializable::SerializableObject<TestTelegramUsersList, nullptr, ITelegramUsersList>
{
public:
    TestTelegramUsersList() = default;

public:
    // Добавить идентификатор чата с поьльзователем и задать статус
    void addUserChatidStatus(const int64_t chatId, const UserStatus userStatus)
    {
        m_chatIdsToUserStatusMap[userStatus].push_back(chatId);
    }

// ITelegramUsersList
public:
    // убедиться что пользователь существует
    void EnsureExist(const TgBot::User::Ptr& /*pUser*/, const int64_t /*chatId*/) override
    {
    }

    // получение статуса пользователя
    UserStatus GetUserStatus(const TgBot::User::Ptr& pUser) override
    {
        return m_curStatus;
    }
    // установка статуса пользователя
    void SetUserStatus(const TgBot::User::Ptr& pUser, const UserStatus newStatus) override
    {
        m_curStatus = newStatus;
    }

    // установка последней заданной пользователем команды боту
    void SetUserLastCommand(const TgBot::User::Ptr& pUser, const std::string& lastCommand) override
    {
        m_lastCommand = lastCommand;
    }
    // получение последней заданной пользователем команды боту
    std::string GetUserLastCommand(const TgBot::User::Ptr& pUser) override
    {
        return m_lastCommand;
    }

    // получить все идентификаторы чатов пользователей с определенным статусом
    std::list<int64_t> GetAllChatIdsByStatus(const UserStatus userStatus) override
    {
        return m_chatIdsToUserStatusMap[userStatus];
    }

    // remove all info about users
    void Clear() override
    {
        m_curStatus = UserStatus::eNotAuthorized;
        m_lastCommand.clear();
        m_chatIdsToUserStatusMap.clear();
    }

private:
    // текущий статус (общий для всех)
    UserStatus m_curStatus = UserStatus::eNotAuthorized;
    // последняя отправленная боту команда
    std::string m_lastCommand;
    // мапа соответствий идентификаторов чатов и статусов пользователей
    std::map<UserStatus, std::list<int64_t>> m_chatIdsToUserStatusMap;
};

} // namespace users

namespace bot {


////////////////////////////////////////////////////////////////////////////////
class TestTelegramThread
    : public ITelegramThread
{
public:
    // тестовый клиент для "подкладывания" в телеграм бота
    class TestClient
        : public TgBot::HttpClient
    {

    public:
        TestClient() = default;

        // Inherited via HttpClient
        std::string makeRequest(const TgBot::Url& url, const std::vector<TgBot::HttpReqArg>& args) const override
        {
            return std::string();
        }
    };

    TestTelegramThread()
        : m_bot(CreateTelegramBot("", m_testClient))
    {}

public:
    typedef std::function<void(int64_t, const std::wstring&, bool,
                               int32_t, TgBot::GenericReply::Ptr,
                               const std::string&, bool)> onSendMessageCallback;
    typedef std::function<void(const std::list<int64_t>&, const std::wstring&, bool,
                               int32_t, TgBot::GenericReply::Ptr,
                               const std::string&, bool)> onSendMessageToChatsCallback;

    // Колбэк на отправку сообщения
    void onSendMessage(const onSendMessageCallback& callback)
    {
        m_sendMessageCallback = callback;
    }
    // Колбэк на отправку сообщения
    void onSendMessageToChats(const onSendMessageToChatsCallback& callback)
    {
        m_sendMessageToChatsCallback = callback;
    }

// ITelegramThread
public:
    // запуск потока
    void StartTelegramThread(const std::unordered_map<std::string, CommandFunction>& commandsList,
                             const CommandFunction& onUnknownCommand = nullptr,
                             const CommandFunction& OnNonCommandMessage = nullptr) override
    {
        TgBot::EventBroadcaster& eventBroadCaster = GetBotEvents();
        for (auto&&[command, function] : commandsList)
        {
            eventBroadCaster.onCommand(command, function);
        }
        eventBroadCaster.onUnknownCommand(onUnknownCommand);
        eventBroadCaster.onNonCommandMessage(OnNonCommandMessage);
    }

    // остановка потока
    void StopTelegramThread() override
    {}

    // функция отправки сообщений в чаты
    void SendMessage(const std::list<int64_t>& chatIds, const std::wstring& msg,
                     bool disableWebPagePreview = false, int32_t replyToMessageId = 0,
                     TgBot::GenericReply::Ptr replyMarkup = std::make_shared<TgBot::GenericReply>(),
                     const std::string& parseMode = "", bool disableNotification = false) override
    {
        ASSERT_TRUE(m_sendMessageToChatsCallback) << L"Отправилось сообщение без колбэка " + CStringA(msg.c_str());

        m_sendMessageToChatsCallback(chatIds, msg, disableWebPagePreview, replyToMessageId,
                                     replyMarkup, parseMode, disableNotification);
    }

    // функция отправки сообщения в чат
    void SendMessage(int64_t chatId, const std::wstring& msg, bool disableWebPagePreview = false,
                     int32_t replyToMessageId = 0,
                     TgBot::GenericReply::Ptr replyMarkup = std::make_shared<TgBot::GenericReply>(),
                     const std::string& parseMode = "", bool disableNotification = false) override
    {
        ASSERT_TRUE(m_sendMessageCallback) << L"Отправилось сообщение без колбэка " + CStringA(msg.c_str());

        m_sendMessageCallback(chatId, msg, disableWebPagePreview, replyToMessageId,
                              replyMarkup, parseMode, disableNotification);
    }

    // возвращает события бота чтобы самому все обрабатывать
    TgBot::EventBroadcaster& GetBotEvents() override
    {
        return m_bot->getEvents();
    }

    // получение апи бота
    const TgBot::Api& GetBotApi() override
    {
        return m_bot->getApi();
    }

public:
    // Апи бота
    std::unique_ptr<TgBot::Bot> m_bot;
    // тестовый клиент для сборки
    TestClient m_testClient;
    // Колбэк на отправку сообщения
    onSendMessageCallback m_sendMessageCallback;
    // Колбэк на отправку сообщения в несколько чатов
    onSendMessageToChatsCallback m_sendMessageToChatsCallback;
};

////////////////////////////////////////////////////////////////////////////////
// тестовый телеграм бот
class TestTelegramBot
    : public testing::Test
    , public ext::events::ScopeSubscription<IMonitoringListEvents>
{
protected:
    TestTelegramBot();
    ~TestTelegramBot();

    // Sets up the test fixture.
    void SetUp() override;
    void TearDown() override;

    // эмулятор команд от телеграма
protected:
    // эмуляция отправки сообщения
    void emulateBroadcastMessage(const std::wstring& text) const;
    // эмуляция запросов от телеграма (нажатие на кнопки)
    void emulateBroadcastCallbackQuery(LPCWSTR queryFormat, ...) const;

// IMonitoringListEvents
private:
    // avoid no subscribers assertion
    void OnChanged() override
    {}

private:
    // создаем сообщение телеграма
    TgBot::Message::Ptr generateMessage(const std::wstring& text) const;

protected:
    ext::ServiceProvider::Ptr m_serviceProvider;

    std::shared_ptr<telegram::users::TestTelegramUsersList> m_pUserList;

    // тестовый телеграм бот
    std::unique_ptr<ITelegramBot> m_testTelegramBot;

    // тестовый объект с фейковым потоком телеграма
    std::shared_ptr<TestTelegramThread> m_telegramThread;
    // need to create tested class
    std::shared_ptr<ITelegramBot> m_telegramBot;

    // список команд и доступности для различных пользователей
    std::map<std::wstring, std::set<users::ITelegramUsersList::UserStatus>> m_commandsToUserStatus;

    // текст со списком команд для админа
    CString m_adminCommandsInfo;
};

} // namespace bot

////////////////////////////////////////////////////////////////////////////////
// класс для проверки результа работы телеграм бота, проверяет какие сообщения
// отправляются пользователю и сравнивает их с эталоном
class TelegramUserMessagesChecker
{
public:
    // pTelegramThread - подложный поток телеграма который эмулирует отправку сообщений
    // pExpectedUserMessage - сообщение которое должно быть отправлено пользователю через onSendMessage
    // pExpectedReply - ответ который должен получить пользователь(как правило тут клавиатура с кнопками ответа)
    // pExpectedRecipientsChats - список чатов которым предназначается сообщение
    // pExpectedMessageToRecipients - сообщение которое должно быть отправлено на указанный список чатов
    TelegramUserMessagesChecker(std::shared_ptr<bot::TestTelegramThread> pTelegramThread,
                                std::wstring* pExpectedUserMessage,
                                TgBot::GenericReply::Ptr* pExpectedReply = nullptr,
                                std::list<int64_t>* pExpectedRecipientsChats = nullptr,
                                std::wstring* pExpectedMessageToRecipients = nullptr);
};

} // namespace telegram