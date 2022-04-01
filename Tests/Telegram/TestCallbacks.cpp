#include "pch.h"

#include <include/ITrendMonitoring.h>
#include <TelegramDLL/TelegramThread.h>

#include <src/Telegram/TelegramCallbacks.h>

#include "TestHelper.h"
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
    CString expectedMessage;
    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // задаем перечень каналов мониторинга чтобы с ними работать
    ITrendMonitoring* trendMonitoring = GetInterface<ITrendMonitoring>();
    const auto allChannels = trendMonitoring->GetNamesOfAllChannels();
    for (const auto& chan : allChannels)
    {
        const size_t chanInd = trendMonitoring->AddMonitoringChannel();
        trendMonitoring->ChangeMonitoringChannelNotify(chanInd, chan);
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
            CString callBackText;
            // /report type={\\\'0\\\'} interval={\\\'i\\\'}
            callBackText = std::string_swprintf(L"%hs %hs={\\\'0\\\'} %hs={\\\'%d\\\'}", kKeyWord.c_str(), kParamType.c_str(), kParamInterval.c_str(), i);

            expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)).GetString(), callBackText.GetString()) };
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
            CString text;
            // /report type={'0'} interval={'i'}
            text = std::string_swprintf(L"%hs %hs={'0'} %hs={'%d'}", kKeyWord.c_str(), kParamType.c_str(), kParamInterval.c_str(), i);

            // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}
            emulateBroadcastCallbackQuery(text.GetString());
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
            // /report type={\\\'1\\\'} chan={\\\'chan\\\'}
            callBackText = std::string_swprintf(L"%hs %hs={\\\'1\\\'} %hs={\\\'%s\\\'}", kKeyWord.c_str(), kParamType.c_str(), kParamChan.c_str(), chan.GetString());

            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.GetString(), callBackText.GetString()) });
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
            // запрашиваем отчёт по каналу chan, ожидаем что предложит все варианты интервалов
            emulateBroadcastCallbackQuery(text.GetString());

            // делаем ту же проверку только без задания типа канала
            for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
            {
                CString callBackText;
                // /report chan={\\\'chan\\\'} interval={\\\'i\\\'}
                callBackText = std::string_swprintf(L"%hs %hs={\\\'%s\\\'} %hs={\\\'%d\\\'}",
                                    kKeyWord.c_str(), kParamChan.c_str(), chan.GetString(),
                                    kParamInterval.c_str(), i);

                expectedKeyboard->inlineKeyboard[i] = { generateKeyBoardButton(monitoring_interval_to_string(MonitoringInterval(i)).GetString(), callBackText.GetString()) };
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
                            kParamChan.c_str(), chan.GetString(),
                            kParamInterval.c_str(), i);
                emulateBroadcastCallbackQuery(text.GetString());
            }

            // проверяем что без задания type тоже работает
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
// проверяем колбэк перезапуска
TEST_F(TestTelegramBot, CheckRestartCommandCallbacks)
{
    // ожидаемое сообщение телеграм боту
    CString expectedMessage;

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // создаем пустой файл батника для эмуляции рестарта
    std::ofstream ofs(get_service<TestHelper>().getRestartFilePath());
    ofs.close();

    expectedMessage = L"Перезапуск системы осуществляется.";
    emulateBroadcastCallbackQuery(L"%hs", restart::kKeyWord.c_str());
}

//----------------------------------------------------------------------------//
// проверяем колбэк перезапуска
TEST_F(TestTelegramBot, CheckAlertCommandCallbacks)
{
    // ожидаемое сообщение телеграм боту
    CString expectedMessage;
    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // задаем перечень каналов мониторинга чтобы им отключать оповещения
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

        // тестируем включение/выключение оповещения
        // kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
        expectedMessage = std::string_swprintf(L"Выберите канал для %s оповещений.", alertOn ? L"включения" : L"выключения");

        CString defCallBackText;
        // /alert enable={\\\'true\\\'}
        defCallBackText = std::string_swprintf(L"%hs %hs={\\\'%s\\\'}", kKeyWord.c_str(), kParamEnable.c_str(), alertOn ? L"true" : L"false");

        // добавляем все кнопки
        expectedKeyboard->inlineKeyboard.clear();
        for (const auto& chan : allChannels)
        {
            CString callBack;
            callBack = std::string_swprintf(L"%s %hs={\\\'%s\\\'}", defCallBackText.GetString(), kParamChan.c_str(), chan.GetString());
            expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.GetString(), callBack.GetString()) });
        }
        // Добавляем кнопку всех каналов
        // chan={\\\'allChannels\\\'}
        defCallBackText.AppendFormat(L" %hs={\\\'%hs\\\'}", kParamChan.c_str(), kValueAllChannels.c_str());
        expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(L"Все каналы", defCallBackText.GetString()) });

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
                                       std::next(allChannels.begin(), ind)->GetString(),
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
    CString expectedMessage;
    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply);

    // команда доступна только админам, делаем админа
    expectedMessage = L"Пользователь успешно авторизован как администратор.";
    emulateBroadcastMessage(L"MonitoringAuthAdmin");

    // задаем перечень каналов мониторинга чтобы им отключать оповещения
    ITrendMonitoring* trendMonitoring = GetInterface<ITrendMonitoring>();
    auto allChannels = trendMonitoring->GetNamesOfAllChannels();
    for (const auto& chan : allChannels)
    {
        trendMonitoring->ChangeMonitoringChannelNotify(trendMonitoring->AddMonitoringChannel(), chan);
    }

    using namespace alarmingValue;

    expectedMessage = L"Выберите канал для изменения уровня оповещений.";
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

    for (const auto& chan : allChannels)
    {
        CString callBack;
        // /alarmV chan={\\\'chan\\\'}
        callBack = std::string_swprintf(L"%hs %hs={\\\'%s\\\'}", kKeyWord.c_str(), kParamChan.c_str(), chan.GetString());
        expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(chan.GetString(), callBack.GetString()) });
    }
    expectedReply = expectedKeyboard;

    // эмулируем команду изменения уровня оповещений
    emulateBroadcastMessage(L"/alarmingValue");

    // проверяем что кнопки делают своё дело
    for (size_t ind = 0, count = expectedKeyboard->inlineKeyboard.size(); ind < count; ++ind)
    {
        const CString channelName = *std::next(allChannels.begin(), ind);
        std::wstring callBackData;

        auto initCommand = [&]()
        {
            expectedReply = std::make_shared<TgBot::GenericReply>();
            callBackData = getUNICODEString(expectedKeyboard->inlineKeyboard[ind].front()->callbackData);

            expectedMessage = std::string_swprintf(L"Для того чтобы изменить допустимый уровень значений у канала '%s' отправьте новый уровень ответным сообщением, отправьте NAN чтобы отключить оповещения совсем.",
                                   channelName.GetString());
            emulateBroadcastCallbackQuery(callBackData.c_str());
        };

        // установка нормального значения
        {
            initCommand();

            const float newValue = 3.09f;
            std::wstringstream newLevelText;
            newLevelText << newValue;

            //  val={\\'3.09\\'}
            CString appendText;
            appendText = std::string_swprintf(L" %hs={\\'%s\\'}", kParamValue.c_str(), newLevelText.str().c_str());

            callBackData += appendText;
            expectedMessage = std::string_swprintf(L"Установить значение оповещений для канала '%s' как %s?", channelName.GetString(), newLevelText.str().c_str());

            TgBot::InlineKeyboardMarkup::Ptr acceptKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            acceptKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"Установить", callBackData.c_str()) } };
            expectedReply = acceptKeyboard;

            emulateBroadcastMessage(newLevelText.str());

            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage = std::string_swprintf(L"Значение %s установлено для канала '%s' успешно", newLevelText.str().c_str(), channelName.GetString());
            // Надо заменить все \\' на просто \'
            emulateBroadcastCallbackQuery(callBackData.c_str());

            // проверяем что число поменялось
            EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(ind).alarmingValue, newValue)
                << "После изменения уровня оповещений через бота уровень не соответствует заданному!";
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
            CString appendText;
            appendText = std::string_swprintf(L" %hs={\\'%s\\'}", kParamValue.c_str(), newLevelText.c_str());

            expectedMessage = std::string_swprintf(L"Отключить оповещения для канала '%s'?", channelName.GetString());
            callBackData += appendText;

            TgBot::InlineKeyboardMarkup::Ptr acceptKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
            acceptKeyboard->inlineKeyboard = { { generateKeyBoardButton(L"Установить", callBackData.c_str()) } };
            expectedReply = acceptKeyboard;

            emulateBroadcastMessage(newLevelText);

            expectedReply = std::make_shared<TgBot::GenericReply>();
            expectedMessage = std::string_swprintf(L"Оповещения для канала '%s' выключены", channelName.GetString());
            // Надо заменить все \\' на просто \'
            emulateBroadcastCallbackQuery(callBackData.c_str());

            // проверяем что число поменялось
            EXPECT_EQ(trendMonitoring->GetMonitoringChannelData(ind).bNotify, false)
                << "После отключения оповещений они не выключены!";

            // повторная установка NAN
            initCommand();

            expectedMessage = std::string_swprintf(L"Оповещения у канала '%s' уже выключены.", channelName.GetString());
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

    const CString errorMsg = L"Тестовое сообщение 111235апывафф1Фвasd 41234%$#@$$%6sfda";

    // ожидаемое сообщение телеграм боту, тестовое сообщение об ошибке
    CString expectedMessage = errorMsg;
    CString expectedMessageToRecipients = errorMsg;

    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();
    // ожидаемые список получателей
    std::list<int64_t>* expectedRecipientsChats = nullptr;

    // класс для проверки ответов пользователю
    const TelegramUserMessagesChecker checker(m_pTelegramThread, &expectedMessage, &expectedReply, &expectedRecipientsChats, &expectedMessageToRecipients);

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
                ASSERT_FALSE("Регистрация пользователя с не обработанным статусом");
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
    auto errorMessage = std::make_shared<MonitoringErrorEventData>();
    errorMessage->errorTextForAllChannels = errorMsg;
    // генерим идентификатор ошибки
    if (!SUCCEEDED(CoCreateGuid(&errorMessage->errorGUID)))
        EXT_ASSERT(!"Не удалось создать гуид!");

    // Клавиатура доступная пользователю
    TgBot::InlineKeyboardMarkup::Ptr expectedKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    expectedKeyboard->inlineKeyboard.push_back({ generateKeyBoardButton(L"Перезапустить систему", CString(restart::kKeyWord.c_str()).GetString()),
                                                 generateKeyBoardButton(L"Оповестить обычных пользователей",
                                                                        L"%hs %hs={\\\'%s\\\'}", resend::kKeyWord.c_str(), resend::kParamId.c_str(), CString(CComBSTR(errorMessage->errorGUID)).GetString()) });

    expectedReply = expectedKeyboard;
    expectedRecipientsChats = &adminChats;

    // эмулируем возникновение ошибки
    get_service<CMassages>().SendMessage(onMonitoringErrorEvent, 0,
                                         std::static_pointer_cast<IEventData>(errorMessage));

    // делаем нас админом
    m_pUserList->SetUserStatus(nullptr, ITelegramUsersList::UserStatus::eAdmin);

    // проверяем рестарт

    // создаем пустой файл батника для эмуляции рестарта
    std::ofstream ofs(get_service<TestHelper>().getRestartFilePath());
    ofs.close();

    expectedReply = std::make_shared<TgBot::GenericReply>();
    expectedMessage = L"Перезапуск системы осуществляется.";
    emulateBroadcastCallbackQuery(CString(restart::kKeyWord.c_str()).GetString());

    // удаляем файл рестарта системы
    std::filesystem::remove(get_service<TestHelper>().getRestartFilePath());

    // проверяем пересылку сообщения
    expectedRecipientsChats = &userChats;

    // пересылаем ошибку рандомную
    expectedMessage = L"Пересылаемой ошибки нет в списке, возможно ошибка является устаревшей (хранятся последние 200 ошибок) или программа была перезапущена.";
    const GUID testGuid = { 123 };
    // /resend errorId={'testGuid'}
    emulateBroadcastCallbackQuery(L"%hs %hs={'%s'}", resend::kKeyWord.c_str(), resend::kParamId.c_str(), CString(CComBSTR(testGuid)).GetString());

    // пересылаем реальную
    expectedMessage = L"Ошибка была успешно переслана обычным пользователям.";
    expectedMessageToRecipients = errorMsg;

    // /resend errorId={'errorGUID'}
    CString resendString;
    resendString = std::string_swprintf(L"%hs %hs={'%s'}", resend::kKeyWord.c_str(), resend::kParamId.c_str(), CString(CComBSTR(errorMessage->errorGUID)).GetString());
    emulateBroadcastCallbackQuery(resendString.GetString());

    // проверяем на двойную пересылку
    expectedMessage = L"Ошибка уже была переслана.";
    emulateBroadcastCallbackQuery(resendString.GetString());
}

//----------------------------------------------------------------------------//
// проверяем что не авторизованный пользователь не имеет доступа к выполнению команд
TEST_F(TestTelegramBot, TestUsingCallbacksWithoutPermission)
{
    // ожидаемое сообщение телеграм боту, тестовое сообщение об ошибке
    CString expectedMessage;

    // ответ с кнопочками
    TgBot::GenericReply::Ptr expectedReply = std::make_shared<TgBot::GenericReply>();

    // класс для проверки ответов пользователю
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

    // делаем нас никем
    m_pUserList->SetUserStatus(nullptr, users::ITelegramUsersList::UserStatus::eNotAuthorized);

    expectedMessage = L"Для работы бота вам необходимо авторизоваться.";
    for (const auto& callback : allCallbacksVariants)
    {
        emulateBroadcastCallbackQuery(callback.GetString());
    }
}

} // namespace telegram::bot