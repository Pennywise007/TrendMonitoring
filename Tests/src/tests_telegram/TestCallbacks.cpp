#include "pch.h"

#include <include/ITrendMonitoring.h>
#include <TelegramThread.h>

#include <src/Telegram/TelegramCallbacks.h>

#include "helpers/TestHelper.h"
#include "TestTelegramBot.h"

namespace telegram::bot {
using namespace callback;

////////////////////////////////////////////////////////////////////////////////
// создаем кнопки для ответа пользователем
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
// проверяем колбэк отчёта
TEST_F(TestTelegramBot, CheckReportCommandCallbacks)
{
    // ожидаемое сообщение телеграм боту
    std::wstring expectedMessage;
    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_telegramThread, &expectedMessage, &expectedReply);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.\n\n" + m_adminCommandsInfo;
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // задаем перечень каналов мониторинга чтобы с ними работать
    std::shared_ptr<ITrendMonitoring> trendMonitoring = ext::GetInterface<ITrendMonitoring>(m_serviceProvider);
    const auto allChannels = trendMonitoring->GetNamesOfAllChannels();
    for (const auto& chan : allChannels)
    {
        const size_t chanInd = trendMonitoring->AddMonitoringChannel();
        trendMonitoring->ChangeMonitoringChannelName(chanInd, chan);
    }

    using namespace report;

    // тестируем формирование отчётов
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}
    expectedMessage = L"По каким каналам сформировать отчёт?";
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    expectedKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"Все каналы",
                                                                  L"%hs %hs={\\\'0\\\'}", kKeyWord.c_str(), kParamType.c_str()) },
                                         { generateKeyBoardButton(L"Определенный канал",
                                                                  L"%hs %hs={\\\'1\\\'}", kKeyWord.c_str(), kParamType.c_str()) } };
    expectedReply = expectedKeyboard;
    emulateBroadcastMessage(L"/report");

    // тестируем отчёт по всем каналам
    {
        expectedMessage = L"Выберите интервал времени за который нужно показать отчёт";
        expectedKeyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            // /report type={\\\'0\\\'} interval={\\\'i\\\'}
            auto callBackText = std::string_swprintf(L"%hs %hs={\\\'0\\\'} %hs={\\\'%d\\\'}", kKeyWord.c_str(), kParamType.c_str(), kParamInterval.c_str(), i);

            expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)), callBackText.c_str()) };
        }
        // проверяем отчёт по всем каналам
        // kKeyWord kParamType={'0'} kParamInterval={'1000000'}

        // /report type={'0'}
        emulateBroadcastCallbackQuery(L"%hs %hs={'0'}", kKeyWord.c_str(), kParamType.c_str());

        expectedMessage = L"Выполняется расчёт данных, это может занять некоторое время.";
        expectedReply = std::make_shared<TgBot::GenericReply>();
        // проверяем все интервалы
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            // /report type={'0'} interval={'i'}
            auto text = std::string_swprintf(L"%hs %hs={'0'} %hs={'%d'}", kKeyWord.c_str(), kParamType.c_str(), kParamInterval.c_str(), i);

            // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}
            emulateBroadcastCallbackQuery(text.c_str());
        }
    }

    // тестируем отчёт по выбранному каналу
    {
        expectedMessage = L"Выберите канал";
        expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        expectedReply = expectedKeyboard;
        for (auto& chan : allChannels)
        {
            // /report type={\\\'1\\\'} chan={\\\'chan\\\'}
            auto callBackText = std::string_swprintf(L"%hs %hs={\\\'1\\\'} %hs={\\\'%s\\\'}", kKeyWord.c_str(), kParamType.c_str(), kParamChan.c_str(), chan.c_str());

            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan, callBackText.c_str()) });
        }
        // проверяем что предложит создать отчёт по каждому из каналов
        // /report type={'1'}");
        emulateBroadcastCallbackQuery(L"%hs %hs={'1'}", kKeyWord.c_str(), kParamType.c_str());

        expectedKeyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);
        // проверяем что для каждого канала будет сформирован отчёт
        for (const auto& chan : allChannels)
        {
            expectedMessage = L"Выберите интервал времени за который нужно показать отчёт";
            expectedReply = expectedKeyboard;

            // для кнопка для каждого интервала мониторинга
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                // /report type={\\\'1\\\'} chan={\\\'chan\\\'} interval={\\\'i\\\'}
                auto callBackText = std::string_swprintf(L"%hs %hs={\\\'1\\\'} %hs={\\\'%s\\\'} %hs={\\\'%d\\\'}",
                                    kKeyWord.c_str(), kParamType.c_str(),
                                    kParamChan.c_str(), chan.c_str(),
                                    kParamInterval.c_str(), i);

                expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)), callBackText.c_str()) };
            }

            // /report type={'1'} chan={'chan'}
            auto text = std::string_swprintf(L"%hs %hs={'1'} %hs={'%s'}", kKeyWord.c_str(), kParamType.c_str(), kParamChan.c_str(), chan.c_str());
            // запрашиваем отчёт по каналу chan, ожидаем что предложит все варианты интервалов
            emulateBroadcastCallbackQuery(text.c_str());

            // делаем ту же проверку только без задания типа канала
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                // /report chan={\\\'chan\\\'} interval={\\\'i\\\'}
                auto callBackText = std::string_swprintf(L"%hs %hs={\\\'%s\\\'} %hs={\\\'%d\\\'}",
                                    kKeyWord.c_str(), kParamChan.c_str(), chan.c_str(),
                                    kParamInterval.c_str(), i);

                expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)), callBackText.c_str()) };
            }

            // запрашиваем данные для каждого интервала
            expectedMessage = L"Выполняется расчёт данных, это может занять некоторое время.";
            expectedReply = std::make_shared<TgBot::GenericReply>();
            // проверяем все интервалы
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                // /report type={'1'} chan={'chan'} interval={'i'}
                text = std::string_swprintf(L"%hs %hs={'1'} %hs={'%s'} %hs={'%d'}",
                            kKeyWord.c_str(), kParamType.c_str(),
                            kParamChan.c_str(), chan.c_str(),
                            kParamInterval.c_str(), i);
                emulateBroadcastCallbackQuery(text.c_str());
            }

            // проверяем что без задания type тоже работает
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                // /report chan={'chan'} interval={'i'}
                text = std::string_swprintf(L"%hs  %hs={'%s'} %hs={'%d'}",
                            kKeyWord.c_str(), kParamChan.c_str(), chan.c_str(),
                            kParamInterval.c_str(), i);
                emulateBroadcastCallbackQuery(text.c_str());
            }
        }
    }
}

//----------------------------------------------------------------------------//
// проверяем колбэк перезапуска
TEST_F(TestTelegramBot, CheckRestartCommandCallbacks)
{
    // ожидаемое сообщение телеграм боту
    std::wstring expectedMessage;

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_telegramThread, &expectedMessage);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.\n\n" + m_adminCommandsInfo;;
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // создаем пустой файл батника для эмуляции рестарта
    std::ofstream ofs(ext::get_service<TestHelper>().getRestartFilePath());
    ofs.close();

    expectedMessage = L"Перезапуск системы осуществляется.";
    emulateBroadcastCallbackQuery(L"%hs", restart::kKeyWord.c_str());
}

//----------------------------------------------------------------------------//
// проверяем колбэк перезапуска
TEST_F(TestTelegramBot, CheckAlertCommandCallbacks)
{
    // ожидаемое сообщение телеграм боту
    std::wstring expectedMessage;
    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_telegramThread, &expectedMessage, &expectedReply);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.\n\n" + m_adminCommandsInfo;
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // задаем перечень каналов мониторинга чтобы им отключать оповещения
    auto trendMonitoring = ext::GetInterface<ITrendMonitoring>(m_serviceProvider);
    const auto allChannels = trendMonitoring->GetNamesOfAllChannels();
    for (const auto& chan : allChannels)
    {
        const size_t chanInd = trendMonitoring->AddMonitoringChannel();
        trendMonitoring->ChangeMonitoringChannelName(chanInd, chan);
    }

    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    auto checkAlert = [&](bool alertOn)
    {
        using namespace alertEnabling;

        expectedReply = expectedKeyboard;

        // тестируем включение/выключение оповещения
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        expectedMessage = std::string_swprintf(L"Выберите канал для %s оповещений.", alertOn ? L"включения" : L"выключения");

        // /alert enable={\\\'true\\\'}
        auto defCallBackText = std::string_swprintf(L"%hs %hs={\\\'%s\\\'}", kKeyWord.c_str(), kParamEnable.c_str(), alertOn ? L"true" : L"false");

        // добавляем все кнопки
        expectedKeyboard->inlineKeyboard.clear();
        for (const auto& chan : allChannels)
        {
            auto callBack = std::string_swprintf(L"%s %hs={\\\'%s\\\'}", defCallBackText.c_str(), kParamChan.c_str(), chan.c_str());
            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.c_str(), callBack.c_str()) });
        }
        // Добавляем кнопку всех каналов
        // chan={\\\'allChannels\\\'}
        defCallBackText += std::string_swprintf(L" %hs={\\\'%hs\\\'}", kParamChan.c_str(), kValueAllChannels.c_str());
        expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(L"Все каналы", defCallBackText.c_str()) });

        // эмулируем команду включения/выключения
        emulateBroadcastMessage(alertOn ? L"/alertingOn" : L"/alertingOff");

        expectedReply = std::make_shared<TgBot::GenericReply>();
        // проверяем что кнопки делают своё дело
        for (size_t ind = 0, count = expectedKeyboard->inlineKeyboard.size(); ind < count; ++ind)
        {
            const std::string& callBackStr = expectedKeyboard->inlineKeyboard[ind].front()->callbackData;

            // проверяем что это последняя кнопка со всеми каналами
            if (ind == count - 1)
            {
                // делаем обратное состояние у оповещений
                for (size_t i = 0, channelsCount = allChannels.size(); i < channelsCount; ++i)
                {
                    trendMonitoring->ChangeMonitoringChannelNotify(i, !alertOn);
                }

                expectedMessage = std::string_swprintf(L"Оповещения для всех каналов %s",
                                       alertOn ? L"включены" : L"выключены");
                emulateBroadcastCallbackQuery(getUNICODEString(callBackStr).c_str());

                // проверяем что состояние оповещения поменялось
                for (size_t i = 0, channelsCount = allChannels.size(); i < channelsCount; ++i)
                {
                    EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(i).bNotify, alertOn)
                        << "После выполнения управления мониторингов состояние оповещения отличаются!";
                }
            }
            else
            {
                // делаем обратное устанавливаемому состояние оповещений у канала
                trendMonitoring->ChangeMonitoringChannelNotify(ind, !alertOn);

                expectedMessage = std::string_swprintf(L"Оповещения для канала %s %s",
                                       std::next(allChannels.begin(), ind)->c_str(),
                                       alertOn ? L"включены" : L"выключены");
                emulateBroadcastCallbackQuery(getUNICODEString(callBackStr).c_str());

                // проверяем что состояние оповещения поменялось
                EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(ind).bNotify, alertOn)
                    << "После выполнения управления мониторингов состояние оповещения отличаются!";
            }
        }
    };

    // тестируем включение оповещений
    checkAlert(true);

    // тестируем выключение оповещений
    checkAlert(false);
}

//----------------------------------------------------------------------------//
// проверяем колбэк перезапуска
TEST_F(TestTelegramBot, CheckChangeAllarmingValueCallback)
{
    // ожидаемое сообщение телеграм боту
    std::wstring expectedMessage;
    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_telegramThread, &expectedMessage, &expectedReply);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.\n\n" + m_adminCommandsInfo;
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // задаем перечень каналов мониторинга чтобы им отключать оповещения
    auto trendMonitoring = ext::GetInterface<ITrendMonitoring>(m_serviceProvider);
    auto allChannels = trendMonitoring->GetNamesOfAllChannels();
    for (const auto& chan : allChannels)
    {
        trendMonitoring->ChangeMonitoringChannelName(trendMonitoring->AddMonitoringChannel(), chan);
    }

    using namespace alarmingValue;

    expectedMessage = L"Выберите канал для изменения уровня оповещений.";
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    for (const auto& chan : allChannels)
    {
        // /alarmV chan={\\\'chan\\\'}
        std::wstring callBack = std::string_swprintf(L"%hs %hs={\\\'%s\\\'}", kKeyWord.c_str(), kParamChan.c_str(), chan.c_str());
        expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.c_str(), callBack.c_str()) });
    }
    expectedReply = expectedKeyboard;

    // эмулируем команду изменения уровня оповещений
    emulateBroadcastMessage(L"/alarmingValue");

    // проверяем что кнопки делают своё дело
    for (size_t ind = 0, count = expectedKeyboard->inlineKeyboard.size(); ind < count; ++ind)
    {
        const std::wstring channelName = *std::next(allChannels.begin(), ind);
        std::wstring callBackData;

        auto initCommand = [&]()
        {
            expectedReply = std::make_shared<TgBot::GenericReply>();
            callBackData = getUNICODEString(expectedKeyboard->inlineKeyboard[ind].front()->callbackData);

            expectedMessage = std::string_swprintf(L"Для того чтобы изменить допустимый уровень значений у канала '%s' отправьте новый уровень ответным сообщением, отправьте NAN чтобы отключить оповещения совсем.",
                                                   channelName.c_str());
            emulateBroadcastCallbackQuery(callBackData.c_str());
        };

        // установка нормального значения
        {
            initCommand();

            const float newValue = 3.09f;
            std::wstringstream newLevelText;
            newLevelText << newValue;

            //  val={\\'3.09\\'}
            std::wstring appendText = std::string_swprintf(L" %hs={\\'%s\\'}", kParamValue.c_str(), newLevelText.str().c_str());

            callBackData += appendText;
            expectedMessage = std::string_swprintf(L"Установить значение оповещений для канала '%s' как %s?", channelName.c_str(), newLevelText.str().c_str());

            TgBot::InlineKeyboardMarkup::Ptr acceptKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            acceptKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"Установить", callBackData.c_str()) } };
            expectedReply = acceptKeyboard;

            emulateBroadcastMessage(newLevelText.str());

            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage = std::string_swprintf(L"Значение %s установлено для канала '%s' успешно", newLevelText.str().c_str(), channelName.c_str());
            // Надо заменить все \\' на просто \'
            emulateBroadcastCallbackQuery(callBackData.c_str());

            // проверяем что число поменялось
            EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(ind).alarmingValue, newValue)
                << "После изменения уровня оповещений через бота уровень не соответствует заданному!";

            // test if user can send non command messages
            expectedMessage = L"Неизвестная команда. " + m_adminCommandsInfo;
            emulateBroadcastMessage(L"12");
        }

        {
            // установка рандомного текста
            initCommand();
            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage = L"Неизвестная команда. " + m_adminCommandsInfo;
            emulateBroadcastMessage(L"фывфывф");

            // установка NAN
            std::wstring newLevelText = L"NAN";

            //  val={\\'NAN\\'}
            std::wstring appendText = std::string_swprintf(L" %hs={\\'%s\\'}", kParamValue.c_str(), newLevelText.c_str());

            expectedMessage = std::string_swprintf(L"Отключить оповещения для канала '%s'?", channelName.c_str());
            callBackData += appendText;

            TgBot::InlineKeyboardMarkup::Ptr acceptKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            acceptKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"Установить", callBackData.c_str()) } };
            expectedReply = acceptKeyboard;

            emulateBroadcastMessage(newLevelText);

            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage = std::string_swprintf(L"Оповещения для канала '%s' выключены", channelName.c_str());
            // Надо заменить все \\' на просто \'
            emulateBroadcastCallbackQuery(callBackData.c_str());

            // проверяем что число поменялось
            EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(ind).bNotify, false)
                << "После отключения оповещений они не выключены!";

            // повторная установка NAN
            initCommand();

            expectedMessage = std::string_swprintf(L"Оповещения у канала '%s' уже выключены.", channelName.c_str());
            expectedReply = std::make_shared<TgBot::GenericReply>();
            emulateBroadcastMessage(newLevelText);
        }
    }
}

//----------------------------------------------------------------------------//
// проверяем колбэк перезапуска
TEST_F(TestTelegramBot, TestProcessingMonitoringError)
{
    using namespace users;

    const std::wstring errorMsg = L"Тестовое сообщение 111235апывафф1Фвasd 41234%$#@$$%6sfda";

    // ожидаемое сообщение телеграм боту, тестовое сообщение об ошибке
    std::wstring expectedMessage = errorMsg;
    std::wstring expectedMessageToRecipients = errorMsg;

    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();
    // ожидаемые список получателей
    std::list<int64_t> expectedRecipientsChats;

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_telegramThread, &expectedMessage, &expectedReply, &expectedRecipientsChats, &expectedMessageToRecipients);

    // перечень чатов обычных пользователей и админов
    std::list<int64_t> userChats;
    std::list<int64_t> adminChats;
    // регистрируем список чатов и статусов
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
                ASSERT_FALSE(false) << "Регистрация пользователя с не обработанным статусом";
                break;
            }

            m_pUserList->addUserChatidStatus(currentChatNumber++, userStatus);
        };

        for (int i = 0; i < 5; ++i)
        {
            registerChat(ITelegramUsersList::UserStatus::eAdmin);
            registerChat(ITelegramUsersList::UserStatus::eOrdinaryUser);
        }

        // дополнительно регаем пару пользователей просто потому что можем
        registerChat(ITelegramUsersList::UserStatus::eOrdinaryUser);
        registerChat(ITelegramUsersList::UserStatus::eOrdinaryUser);
    }

    // создаем тестовую ошибку
    auto eventData = std::make_shared<IMonitoringErrorEvents::EventData>();
    eventData->errorTextForAllChannels = errorMsg;
    // генерим идентификатор ошибки
    if (!SUCCEEDED(CoCreateGuid(&eventData->errorGUID)))
        EXT_ASSERT(false) << L"Не удалось создать гуид!";

    // Клавиатура доступная пользователю
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(L"Перезапустить систему", CString(restart::kKeyWord.c_str()).GetString()),
                                                 generateKeyBoardButton(L"Оповестить обычных пользователей",
                                                                        L"%hs %hs={\\\'%s\\\'}", resend::kKeyWord.c_str(), resend::kParamId.c_str(), CString(CComBSTR(eventData->errorGUID)).GetString()) });

    expectedReply = expectedKeyboard;
    expectedRecipientsChats = adminChats;

    // эмулируем возникновение ошибки
    ext::send_event(&IMonitoringErrorEvents::OnError, eventData);

    // делаем нас админом
    m_pUserList->SetUserStatus(nullptr, ITelegramUsersList::UserStatus::eAdmin);

    // проверяем рестарт

    // создаем пустой файл батника для эмуляции рестарта
    std::ofstream ofs(ext::get_service<TestHelper>().getRestartFilePath());
    ofs.close();

    expectedReply = std::make_shared<TgBot::GenericReply>();
    expectedMessage = L"Перезапуск системы осуществляется.";
    emulateBroadcastCallbackQuery(CString(restart::kKeyWord.c_str()).GetString());

    // удаляем файл рестарта системы
    std::filesystem::remove(ext::get_service<TestHelper>().getRestartFilePath());

    // проверяем пересылку сообщения
    expectedRecipientsChats = userChats;

    // пересылаем ошибку рандомную
    expectedMessage = L"Пересылаемой ошибки нет в списке, возможно ошибка является устаревшей (хранятся последние 200 ошибок) или программа была перезапущена.";
    const GUID testGuid = { 123 };
    // /resend errorId={'testGuid'}
    emulateBroadcastCallbackQuery(L"%hs %hs={'%s'}", resend::kKeyWord.c_str(), resend::kParamId.c_str(), CString(CComBSTR(testGuid)).GetString());

    // пересылаем реальную
    expectedMessage = L"Ошибка была успешно переслана обычным пользователям.";
    expectedMessageToRecipients = errorMsg;

    // /resend errorId={'errorGUID'}
    auto resendString = std::string_swprintf(L"%hs %hs={'%s'}", resend::kKeyWord.c_str(), resend::kParamId.c_str(), CString(CComBSTR(eventData->errorGUID)).GetString());
    emulateBroadcastCallbackQuery(resendString.c_str());

    // проверяем на двойную пересылку
    expectedMessage = L"Ошибка уже была переслана.";
    emulateBroadcastCallbackQuery(resendString.c_str());
}

//----------------------------------------------------------------------------//
// проверяем что не авторизованный пользователь не имеет доступа к выполнению команд
TEST_F(TestTelegramBot, TestUsingCallbacksWithoutPermission)
{
    // ожидаемое сообщение телеграм боту, тестовое сообщение об ошибке
    std::wstring expectedMessage;

    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_telegramThread, &expectedMessage, &expectedReply);

    std::list<std::wstring> allCallbacksVariants;
    // /restart
    allCallbacksVariants.emplace_back(std::string_swprintf(L"%hs", restart::kKeyWord.c_str()));
    // /resend errorId={\\\'012312312312\\\'}
    allCallbacksVariants.emplace_back(std::string_swprintf(L"%hs %hs={\\\'012312312312\\\'}", resend::kKeyWord.c_str(), resend::kParamId.c_str()));
    // /report type={\\\'0\\\'} chan={\\\'RandomChannel\\\'}
    allCallbacksVariants.emplace_back(std::string_swprintf(L"%hs %hs={\\\'0\\\'} %hs={\\\'RandomChannel\\\'}", report::kKeyWord.c_str(), report::kParamType.c_str(), report::kParamChan.c_str()));
    // /alert enable={\\\'true\\\'} chan={\\\'allChannels\\\'}
    allCallbacksVariants.emplace_back(std::string_swprintf(L"%hs %hs={\\\'true\\\'} %hs={\\\'%hs\\\'}",
                                               alertEnabling::kKeyWord.c_str(), alertEnabling::kParamEnable.c_str(),
                                               alertEnabling::kParamChan.c_str(), alertEnabling::kValueAllChannels.c_str()));
    // /alarmV chan={\\\'RandomChannel\\\'} val={\\\'0.3\\\'}
    allCallbacksVariants.emplace_back() = std::string_swprintf(L"%hs %hs={\\\'RandomChannel\\\'} %hs={\\\'0.3\\\'}", alarmingValue::kKeyWord.c_str(), alarmingValue::kParamChan.c_str(), alarmingValue::kParamValue.c_str());

    // делаем нас никем
    m_pUserList->SetUserStatus(nullptr, users::ITelegramUsersList::UserStatus::eNotAuthorized);

    expectedMessage = L"Для работы бота вам необходимо авторизоваться.";
    for (const auto& callback : allCallbacksVariants)
    {
        emulateBroadcastCallbackQuery(callback.c_str());
    }
}

} // namespace telegram::bot