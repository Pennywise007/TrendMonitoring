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

// идентификатор фиктивного пользователя телеграма
const int64_t kTelegramTestChatId = 1234;

////////////////////////////////////////////////////////////////////////////////
// создаем кнопки для ответа пользователем
TgBot::InlineKeyboardButton::Ptr generateKeyBoardButton(const CString& text,
                                                        const CString& callBack)
{
    TgBot::InlineKeyboardButton::Ptr button = std::make_shared<TgBot::InlineKeyboardButton>();
    button->text = getUtf8Str(text);
    button->callbackData = getUtf8Str(callBack);
    return button;
}

////////////////////////////////////////////////////////////////////////////////
// Проверка наличия обработчиков команд бота
TEST_F(TestTelegramBot, CheckCommandListeners)
{
    TgBot::EventBroadcaster& botEvents = m_pTelegramThread->getBotEvents();
    // проверяем наличие обработчиков
    auto& commandListeners = botEvents.getCommandListeners();
    for (const auto& command : m_commandsToUserStatus)
    {
        EXPECT_NE(commandListeners.find(CStringA(command.first).GetString()), commandListeners.end())
            << "У команды бота \"" + CStringA(command.first) + "\" отсутствует обработчик";
    }

    EXPECT_EQ(commandListeners.size(), 5) << "Количество обработчиков команд и самих команд не совпадает";
}

//----------------------------------------------------------------------------//
// проверяем реакцию на различных пользователей
TEST_F(TestTelegramBot, CheckCommandsAvailability)
{
    // ожидаемое сообщение телеграм боту
    CString expectedMessage;

    // класс для проверки ответов пользователю
    TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage);

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
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eOrdinaryUser)
            << "У пользователя не соответствует статус после авторизации";

        // повторно отправляем сообщение авторизации
        expectedMessage = L"Пользователь уже авторизован.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eOrdinaryUser)
            << "У пользователя не соответствует статус после авторизации";

        CString unknownMessageText = L"Неизвестная команда. Поддерживаемые команды бота:\n\n\n/info - Перечень команд бота.\n/report - Сформировать отчёт.\n\n\nДля того чтобы их использовать необходимо написать их этому боту(обязательно использовать слэш перед текстом команды(/)!). Или нажать в этом окне, они должны подсвечиваться.";

        // проверяем доступные обычному пользователю команды
        for (const auto& command : m_commandsToUserStatus)
        {
            if (command.second.find(ITelegramUsersList::UserStatus::eOrdinaryUser) == command.second.end())
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
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eAdmin)
            << "У пользователя не соответствует статус после авторизации";

        // повторно отправляем сообщение авторизации обычного пользователя
        expectedMessage = L"Пользователь является администратором системы. Авторизация не требуется.";
        emulateBroadcastMessage(L"MonitoringAuth");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eAdmin)
            << "У пользователя не соответствует статус после авторизации";

        // повторно отправляем сообщение авторизации админа
        expectedMessage = L"Пользователь уже авторизован как администратор.";
        emulateBroadcastMessage(L"MonitoringAuthAdmin");
        EXPECT_EQ(m_pUserList->getUserStatus(nullptr), ITelegramUsersList::UserStatus::eAdmin)
            << "У пользователя не соответствует статус после авторизации";

        // проверяем доступные обычному пользователю команды
        for (const auto& command : m_commandsToUserStatus)
        {
            EXPECT_TRUE(command.second.find(ITelegramUsersList::UserStatus::eAdmin) != command.second.end())
                << "У админа есть недоступная ему команда " << command.first;

            if (command.first == L"info")
                expectedMessage = L"Поддерживаемые команды бота:\n\n\n/info - Перечень команд бота.\n/report - Сформировать отчёт.\n/restart - Перезапустить систему мониторинга.\n/allertionOn - Включить оповещения о событиях.\n/allertionOff - Выключить оповещения о событиях.\n\n\nДля того чтобы их использовать необходимо написать их этому боту(обязательно использовать слэш перед текстом команды(/)!). Или нажать в этом окне, они должны подсвечиваться.";
            else if (command.first == L"allertionOn"     ||
                     command.first == L"allertionOff"    ||
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
        expectedMessage = L"Неизвестная команда. Поддерживаемые команды бота:\n\n\n/info - Перечень команд бота.\n/report - Сформировать отчёт.\n/restart - Перезапустить систему мониторинга.\n/allertionOn - Включить оповещения о событиях.\n/allertionOff - Выключить оповещения о событиях.\n\n\nДля того чтобы их использовать необходимо написать их этому боту(обязательно использовать слэш перед текстом команды(/)!). Или нажать в этом окне, они должны подсвечиваться.";;
        emulateBroadcastMessage(L"/123");
        emulateBroadcastMessage(L"123");
    }
}

//----------------------------------------------------------------------------//
// проверяем колбэк отчёта
TEST_F(TestTelegramBot, CheckReportCommandCallbacks)
{
    // ожидаемое сообщение телеграм боту
    CString expectedMessage;
    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
    TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // задаем перечень каналов мониторинга чтобы с ними работать
    ITrendMonitoring* trendMonitoring = get_monitoing_service();
    std::set<CString> allChannels = trendMonitoring->getNamesOfAllChannels();
    for (auto& chan : allChannels)
    {
        size_t chanInd = trendMonitoring->addMonitoingChannel();
        trendMonitoring->changeMonitoingChannelName(chanInd, chan);
    }

    // тестируем формирование отчётов
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}
    expectedMessage = L"По каким каналам сформировать отчёт?";
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    expectedKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"Все каналы", L"/report reportType={\\\'0\\\'}") },
                                         { generateKeyBoardButton(L"Определенный канал", L"/report reportType={\\\'1\\\'}") } };
    expectedReply = expectedKeyboard;
    emulateBroadcastMessage(L"/report");

    // тестируем отчёт по всем каналам
    {
        expectedMessage = L"Выберите интервал времени за который нужно показать отчёт";
        expectedKeyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            CString callBackText;
            callBackText.Format(L"/report reportType={\\\'0\\\'} interval={\\\'%d\\\'}", i);

            expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)), callBackText) };
        }
        // проверяем отчёт по всем каналам
        // kKeyWord kParamType={'0'} kParamInterval={'1000000'}
        emulateBroadcastCallbackQuery(L"/report reportType={'0'}");

        expectedMessage = L"Выполняется расчёт данных, это может занять некоторое время.";
        expectedReply = std::make_shared<TgBot::GenericReply>();
        // проверяем все интервалы
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            CString text;
            text.Format(L"/report reportType={'0'} interval={'%d'}", i);

            // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}
            emulateBroadcastCallbackQuery(CStringA(text).GetString());
        }
    }

    // тестируем отчёт по выбранному каналу
    {
        expectedMessage = L"Выберите канал";
        expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        expectedReply = expectedKeyboard;
        for (auto& chan : allChannels)
        {
            CString callBackText;
            callBackText.Format(L"/report reportType={\\\'1\\\'} chan={\\\'%s\\\'}", chan.GetString());

            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan, callBackText) });
        }
        // проверяем отчёт по определенному каналу
        emulateBroadcastCallbackQuery(L"/report reportType={'1'}");

        expectedKeyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);
        // проверяем что для каждого канала будет сформирован отчёт
        for (auto& chan : allChannels)
        {
            expectedMessage = L"Выберите интервал времени за который нужно показать отчёт";
            expectedReply = expectedKeyboard;

            // для кнопка для каждого интервала мониторинга
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                CString callBackText;
                callBackText.Format(L"/report reportType={\\\'1\\\'} chan={\\\'%s\\\'} interval={\\\'%d\\\'}", chan.GetString(), i);

                expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)), callBackText) };
            }

            CString text;
            text.Format(L"/report reportType={'1'} chan={'%s'}", chan.GetString());
            // запрашиваем очтёт по каналу chan
            emulateBroadcastCallbackQuery(text);

            // запрашиваем данные для каждого интервала
            expectedMessage = L"Выполняется расчёт данных, это может занять некоторое время.";
            expectedReply = std::make_shared<TgBot::GenericReply>();
            // проверяем все интервалы
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                CString text;
                text.Format(L"/report reportType={'1'} chan={'%s'} interval={'%d'}", chan.GetString(), i);
                emulateBroadcastCallbackQuery(text);
            }
        }
    }
}

//----------------------------------------------------------------------------//
// проверяем колбэк перезапуска
TEST_F(TestTelegramBot, CheckRestartCommandCallbacks)
{
    // ожидаемое сообщение телеграм боту
    CString expectedMessage;

    // класс для проверки ответов пользователю
    TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // создаем пустой файл батника для эмуляции рестарта
    std::ofstream ofs(get_service<TestHelper>().getRestartFilePath());
    ofs.close();

    expectedMessage = L"Перезапуск системы осуществляется.";
    emulateBroadcastCallbackQuery(L"/restart");
}

//----------------------------------------------------------------------------//
// проверяем колбэк перезапуска
TEST_F(TestTelegramBot, CheckAllertCommandCallbacks)
{
    // ожидаемое сообщение телеграм боту
    CString expectedMessage;
    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
    TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // задаем перечень каналов мониторинга чтобы им отключать оповещения
    ITrendMonitoring* trendMonitoring = get_monitoing_service();
    std::set<CString> allChannels = trendMonitoring->getNamesOfAllChannels();
    for (auto& chan : allChannels)
    {
        size_t chanInd = trendMonitoring->addMonitoingChannel();
        trendMonitoring->changeMonitoingChannelName(chanInd, chan);
    }

    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    auto checkAllert = [&](bool allertOn)
    {
        expectedReply = expectedKeyboard;

        // тестируем включение/выключение оповещения
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        expectedMessage.Format(L"Выберите канал для %s оповещений.", allertOn ? L"включения" : L"выключения");

        CString defCallBackText;
        defCallBackText.Format(L"/allert enable={\\\'%s\\\'}", allertOn ? L"true" : L"false");

        // добавляем все кнопки
        expectedKeyboard->inlineKeyboard.clear();
        for (auto& chan : allChannels)
        {
            CString callBack;
            callBack.Format(L"%s chan={\\\'%s\\\'}", defCallBackText.GetString(), chan.GetString());
            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan, callBack) });
        }
        // Добавляем кнопку всех каналов
        defCallBackText.Append(L" chan={\\\'allChannels\\\'}");
        expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(L"Все каналы", defCallBackText) });

        // эмулируем команду включения/выключения
        emulateBroadcastMessage(allertOn ? L"/allertionOn" : L"/allertionOff");

        expectedReply = std::make_shared<TgBot::GenericReply>();
        // проверяем что кнопки делают своё дело
        for (size_t ind = 0, count = expectedKeyboard->inlineKeyboard.size(); ind < count; ++ind)
        {
            const std::string& callBackStr = expectedKeyboard->inlineKeyboard[ind].front()->callbackData;

            // Надо заменить все \\\' на просто '
            CString callBackData = std::regex_replace(getUNICODEString(callBackStr).GetString(),
                                                      std::wregex(LR"(\\')"),
                                                      L"'").c_str();

            // проверяем что это последняя кнопка со всеми каналами
            if (ind == count - 1)
            {
                // делаем обратное состояние у оповещений
                for (size_t i = 0, count = allChannels.size(); i < count; ++i)
                {
                    trendMonitoring->changeMonitoingChannelNotify(i, !allertOn);
                }

                expectedMessage.Format(L"Оповещения для всех каналов %s",
                                       allertOn ? L"включены" : L"выключены");
                emulateBroadcastCallbackQuery(callBackData);

                // проверяем что состояние оповещения поменялось
                for (size_t i = 0, count = allChannels.size(); i < count; ++i)
                {
                    EXPECT_EQ(trendMonitoring->getMonitoringChannelData(i).bNotify, allertOn)
                        << "После выполнения управления мониторингов состояние оповещения отличаются!";
                }
            }
            else
            {
                // делаем обратное устанавливаемому состояние оповещений у канала
                trendMonitoring->changeMonitoingChannelNotify(ind, !allertOn);

                expectedMessage.Format(L"Оповещения для канала %s %s",
                                       std::next(allChannels.begin(), ind)->GetString(),
                                       allertOn ? L"включены" : L"выключены");
                emulateBroadcastCallbackQuery(callBackData);

                // проверяем что состояние оповещения поменялось
                EXPECT_EQ(trendMonitoring->getMonitoringChannelData(ind).bNotify, allertOn)
                    << "После выполнения управления мониторингов состояние оповещения отличаются!";
            }
        }
    };

    // тестируем включение оповещений
    checkAllert(true);

    // тестируем выключение оповещений
    checkAllert(false);
}

//----------------------------------------------------------------------------//
// проверяем колбэк перезапуска
TEST_F(TestTelegramBot, TestProcessingMonitoringError)
{
    const CString errorMsg = L"Тестовое сообщение 111235апывафф1Фвasd 41234%$#@$$%6sfda";

    // ожидаемое сообщение телеграм боту, тестовое сообщение об ошибке
    CString expectedMessage = errorMsg;
    CString expectedMessageToReciepients = errorMsg;

    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();
    // ожидаемые список получателей
    std::list<int64_t>* expectedReciepientsChats = nullptr;

    // класс для проверки ответов пользователю
    TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply, &expectedReciepientsChats, &expectedMessageToReciepients);

    // перечень чатов обычных пользователей и админов
    std::list<int64_t> userChats;
    std::list<int64_t> adminChats;
    // регистрируем список чатов и статусов
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
                ASSERT_FALSE("Регистрация пользователя с не обработанным статусом");
                break;
            }

            m_pUserList->addUserChatidStatus(chatId, userStatus);
        };

        for (int i = 0; i < 5; ++i)
        {
            registerChat(std::rand() % 15, ITelegramUsersList::UserStatus::eAdmin);
            registerChat(std::rand() % 15, ITelegramUsersList::UserStatus::eOrdinaryUser);
        }

        // дополнительно регаем пару пользователей просто потому что можем
        registerChat(std::rand() % 15, ITelegramUsersList::UserStatus::eOrdinaryUser);
        registerChat(std::rand() % 15, ITelegramUsersList::UserStatus::eOrdinaryUser);
    }

    // создаем тестовую ошибку
    auto errorMessage = std::make_shared<MonitoringErrorEventData>();
    errorMessage->errorText = errorMsg;
    // генерим идентификатор ошибки
    if (!SUCCEEDED(CoCreateGuid(&errorMessage->errorGUID)))
        assert(!"Не удалось создать гуид!");

    // Клавиатура доступная пользователю
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(L"Перезапустить систему", L"/restart"),
                                                 generateKeyBoardButton(L"Оповестить обычных пользователей",
                                                                        L"/resend errorId={\\\'" + CString(CComBSTR(errorMessage->errorGUID)) + L"\\\'}") });

    expectedReply = expectedKeyboard;
    expectedReciepientsChats = &adminChats;

    // эмулируем возникновение ошибки
    get_service<CMassages>().sendMessage(onMonitoringErrorEvent, 0,
                                         std::static_pointer_cast<IEventData>(errorMessage));

    // делаем нас админом
    m_pUserList->setUserStatus(nullptr, ITelegramUsersList::UserStatus::eAdmin);

    // проверяем рестарт

    // создаем пустой файл батника для эмуляции рестарта
    std::ofstream ofs(get_service<TestHelper>().getRestartFilePath());
    ofs.close();

    expectedReply = std::make_shared<TgBot::GenericReply>();
    expectedMessage = L"Перезапуск системы осуществляется.";
    emulateBroadcastCallbackQuery(L"/restart");

    // проверяем пересылку сообщения
    expectedReciepientsChats = &userChats;

    // пересылаем ошибку рандомную
    expectedMessage = L"Пересылаемой ошибки нет в списке, возможно ошибка является устаревшей (хранятся последние 200 ошибок) или программа была перезапущена.";
    GUID testGuid = { 123 };
    emulateBroadcastCallbackQuery(L"/resend errorId={'" + CString(CComBSTR(testGuid)) + L"'}");

    // пересылаем реальную
    expectedMessage = L"Ошибка была успешно переслана обычным пользователям.";
    expectedMessageToReciepients = errorMsg;
    emulateBroadcastCallbackQuery(L"/resend errorId={'" + CString(CComBSTR(errorMessage->errorGUID)) + L"'}");

    // проверяем на двойную пересылку
    expectedMessage = L"Ошибка уже была переслана.";
    emulateBroadcastCallbackQuery(L"/resend errorId={'" + CString(CComBSTR(errorMessage->errorGUID)) + L"'}");
}

////////////////////////////////////////////////////////////////////////////////
void TestTelegramBot::SetUp()
{
    // инициализируем список пользователей
    m_pUserList = TestTelegramUsersList::create();;

    m_pTelegramThread = new TestTelegramThread();

    // создаем фейковый поток телеграмма и запоминаем его указатель
    ITelegramThreadPtr pTelegramThread(m_pTelegramThread);
    // создаем настройки бота
    TelegramBotSettings botSettings;
    botSettings.bEnable = true;
    botSettings.sToken = L"Testing";

    // инициализируем список пользователей
    m_testTelegramBot.initBot(m_pUserList);
    // инициализация фейкового потока для имтитации работы телеграма, имитируем что у нас unique_ptr
    m_testTelegramBot.setDefaultTelegramThread(pTelegramThread);
    // передаём настройки
    m_testTelegramBot.setBotSettings(botSettings);

    // заполняем перечень команд и доступность её для различных пользователей
    m_commandsToUserStatus["info"]         = { ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };
    m_commandsToUserStatus["report"]       = { ITelegramUsersList::eAdmin, ITelegramUsersList::eOrdinaryUser };
    m_commandsToUserStatus["restart"]      = { ITelegramUsersList::eAdmin };
    m_commandsToUserStatus["allertionOn"]  = { ITelegramUsersList::eAdmin };
    m_commandsToUserStatus["allertionOff"] = { ITelegramUsersList::eAdmin };

    static_assert(ITelegramUsersList::eLastStatus == 3,
                  "Список пользовтеля изменился, возможно стоит пересмотреть доступность команд");

    get_service<TestHelper>().resetMonitoringService();
}

//----------------------------------------------------------------------------//
void TestTelegramBot::emulateBroadcastMessage(const CString& text) const
{
    TgBot::Update::Ptr pUpdate = std::make_shared<TgBot::Update>();
    pUpdate->message = generateMessage(text);

    HandleTgUpdate(m_pTelegramThread->m_botApi->getEventHandler(), pUpdate);
}

//----------------------------------------------------------------------------//
void TestTelegramBot::emulateBroadcastCallbackQuery(const CString& text) const
{
    TgBot::Message::Ptr pMessage = generateMessage(text);

    TgBot::Update::Ptr pUpdate = std::make_shared<TgBot::Update>();
    pUpdate->callbackQuery = std::make_shared<TgBot::CallbackQuery>();
    pUpdate->callbackQuery->from = pMessage->from;
    pUpdate->callbackQuery->message = pMessage;
    pUpdate->callbackQuery->data = getUtf8Str(text);

    HandleTgUpdate(m_pTelegramThread->m_botApi->getEventHandler(), pUpdate);
}

//----------------------------------------------------------------------------//
TgBot::Message::Ptr TestTelegramBot::generateMessage(const CString& text) const
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
                                                         std::list<int64_t>** pExpectedReciepientsChats /*= nullptr*/,
                                                         CString* pExpectedMessageToReciepients /*= nullptr*/)
{
    // сравнение двух ответов(клавиатур пользователя)
    auto compareReply = [](const TgBot::GenericReply::Ptr first,
                           const TgBot::GenericReply::Ptr second) -> bool
    {
        if (first == second)
            return true;

        if (!first || !second)
            return false;

        TgBot::InlineKeyboardMarkup::Ptr keyBoardMarkupFirst =
            std::dynamic_pointer_cast<TgBot::InlineKeyboardMarkup>(first);
        TgBot::InlineKeyboardMarkup::Ptr keyBoardMarkupSecond =
            std::dynamic_pointer_cast<TgBot::InlineKeyboardMarkup>(second);

        if ((bool)keyBoardMarkupFirst != (bool)keyBoardMarkupSecond)
        {
            EXPECT_FALSE("Типы ответов не соответствуют друг другу.");
            return false;
        }

        if (keyBoardMarkupFirst && keyBoardMarkupSecond)
        {
            // сравниваем как InlineKeyboardMarkup
            std::vector<std::vector<TgBot::InlineKeyboardButton::Ptr>> keyboardFirst =
                keyBoardMarkupFirst->inlineKeyboard;
            std::vector<std::vector<TgBot::InlineKeyboardButton::Ptr>> keyboardSecond =
                keyBoardMarkupSecond->inlineKeyboard;

            if (keyboardFirst.size() != keyboardSecond.size())
            {
                EXPECT_FALSE("Клавиатура для ответа различается");
                return false;
            }

            for (size_t row = 0, countRows = keyboardFirst.size(); row < countRows; ++row)
            {
                if (keyboardFirst[row].size() != keyboardSecond[row].size())
                {
                    EXPECT_FALSE("Клавиатура для ответа различается");
                    return false;
                }

                for (size_t col = 0, countColumns = keyboardFirst[row].size(); col < countColumns; ++col)
                {
                    TgBot::InlineKeyboardButton::Ptr firstKey = keyboardFirst[row][col];
                    TgBot::InlineKeyboardButton::Ptr secondKey = keyboardSecond[row][col];

                    // сравниваем кнопки
                    if (firstKey->text != secondKey->text)
                    {
                        EXPECT_FALSE("Клавиатура для ответа различается");
                        return false;
                    }

                    boost::trim(firstKey->callbackData);
                    boost::trim(secondKey->callbackData);
                    if (firstKey->callbackData != secondKey->callbackData)
                    {
                        EXPECT_FALSE("Клавиатура для ответа различается");
                        return false;
                    }
                }
            }

            return true;
        }

        return true;
    };

    pTelegramThread->onSendMessage(
        [pExpectedMessage, pExpectedReply, &compareReply]
        (int64_t chatId, const CString& msg, bool disableWebPagePreview,
         int32_t replyToMessageId, TgBot::GenericReply::Ptr replyMarkup,
         const std::string& parseMode, bool disableNotification)
        {
            ASSERT_TRUE(pExpectedMessage) << "Не передали сообщение.";

            EXPECT_EQ(chatId, kTelegramTestChatId) << "Идентификатор чата с получателем сообщения не корректен";
            EXPECT_TRUE(msg == *pExpectedMessage) << "Пришло сообщение отличающееся от ожидаемого: " + CStringA(msg);

            if (pExpectedReply)
                compareReply(replyMarkup, *pExpectedReply);
        });

    pTelegramThread->onSendMessageToChats(
        [pExpectedReciepientsChats, pExpectedReply, pExpectedMessageToReciepients, &compareReply]
         (const std::list<int64_t>& chatIds, const CString& msg, bool disableWebPagePreview,
         int32_t replyToMessageId, TgBot::GenericReply::Ptr replyMarkup,
         const std::string& parseMode, bool disableNotification)
        {
            ASSERT_TRUE(pExpectedReciepientsChats) << "Не передали список чатов.";
            ASSERT_TRUE(pExpectedMessageToReciepients) << "Не передали сообщение для получателей.";

            EXPECT_EQ(chatIds.size(), (*pExpectedReciepientsChats)->size()) << "Ожидаемые чаты и фактические не совпали.";
            auto chatId = chatIds.begin();
            auto expectedId = (*pExpectedReciepientsChats)->begin();
            for (size_t i = 0, count = std::min<size_t>(chatIds.size(), (*pExpectedReciepientsChats)->size());
                 i < count; ++i, ++chatId, ++expectedId)
            {
                EXPECT_EQ(*chatId, *expectedId) << "Ожидаемые чаты и фактические не совпали.";;
            }

            EXPECT_TRUE(msg == *pExpectedMessageToReciepients) << "Пришло сообщение отличающееся от ожидаемого: " + CStringA(msg);

            if (pExpectedReply)
                compareReply(replyMarkup, *pExpectedReply);
        });
}