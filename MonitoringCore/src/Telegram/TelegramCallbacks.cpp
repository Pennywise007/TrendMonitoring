#include "pch.h"

#include <regex>

#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/std_pair.hpp> // for CallbackParser

#include <include/IMonitoringTasksService.h>
#include <include/ITrendMonitoring.h>

#include "CommandsInfoService.h"
#include "TelegramCallbacks.h"
#include "KeyboardCallback.h"

namespace callback
{
// Максимальное количество последних ошибок хранимых программой
constexpr size_t g_kMaxErrorInfoCount = 200;

////////////////////////////////////////////////////////////////////////////////
// парсинг колбэков
namespace CallbackParser
{
    using namespace boost::spirit::x3;

    namespace
    {
        template <typename T>
        struct as_type
        {
            template <typename Expr>
            auto operator[](Expr expr) const
            {
                return rule<struct _, T>{"as"} = expr;
            }
        };

        template <typename T>
        static const as_type<T> as = {};
    }
    auto quoted = [](char q)
    {
        return lexeme[q >> *(q >> char_(q) | '\\' >> char_ | char_ - q) >> q];
    };

    auto value  = quoted('\'') | quoted('"');
    auto key    = lexeme[+alpha];
    auto pair   = key >> "={" >> value >> '}';
    auto parser = skip(space)[*as<std::pair<TelegramCallbacks::CallBackParams::key_type, TelegramCallbacks::CallBackParams::mapped_type>>[pair]];
}

////////////////////////////////////////////////////////////////////////////////
TelegramCallbacks::TelegramCallbacks(ITelegramThreadPtr& telegramThread, ITelegramUsersListPtr& userList)
    : m_telegramThread(telegramThread)
    , m_telegramUsers(userList)
{
    // подписываемся на события о завершении мониторинга
    EventRecipientImpl::subscribe(onCompletingMonitoringTask);
    // подписываемся на события о возникающих ошибках в процессе запроса новых данных
    EventRecipientImpl::subscribe(onMonitoringErrorEvent);

    // TODO C++20 replace to template lambda
    auto addCommandCallback = [commandCallbacks = &m_commandCallbacks]
        (const std::string& keyWord, const CommandCallback& callback)
        {
            if (!commandCallbacks->try_emplace(keyWord, callback).second)
                assert(!"Ключевые слова у колбэков должны отличаться!");
        };
    addCommandCallback(report       ::kKeyWord, &TelegramCallbacks::executeCallbackReport);
    addCommandCallback(restart      ::kKeyWord, &TelegramCallbacks::executeCallbackRestart);
    addCommandCallback(resend       ::kKeyWord, &TelegramCallbacks::executeCallbackResend);
    addCommandCallback(alertEnabling::kKeyWord, &TelegramCallbacks::executeCallbackAlert);
    addCommandCallback(alarmingValue::kKeyWord, &TelegramCallbacks::executeCallbackAlarmValue);
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::onCallbackQuery(const TgBot::CallbackQuery::Ptr& query)
{
    m_telegramUsers->setUserLastCommand(query->from, query->data);

    try
    {
        // парсим колбэк и проверяем каких параметров не хватает
        CallBackParams callBackParams;

        for (auto& [keyWord, callback] : m_commandCallbacks)
        {
            if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                         keyWord >> CallbackParser::parser, callBackParams))
            {
                if (!get_service<CommandsInfoService>().ensureNeedAnswerOnCallback(m_telegramUsers.get(), keyWord, query->message))
                {
                    // пользователь отправивший сообщение
                    const TgBot::User::Ptr& pUser = query->message->from;

                    if (m_telegramUsers->getUserStatus(pUser) == ITelegramUsersList::eNotAuthorized)
                        m_telegramThread->sendMessage(query->message->chat->id, L"Для работы бота вам необходимо авторизоваться.");
                    else
                        m_telegramThread->sendMessage(query->message->chat->id, L"Неизвестная или более не доступная команда.");
                }
                else
                    (this->*callback)(query->message, callBackParams, false);

                return;
            }
        }

        throw std::runtime_error("Ошибка разбора команды");
    }
    catch (const std::exception& exc)
    {
        std::cerr << exc.what() << " Callback '" << CStringA(getUNICODEString(query->data).c_str()).GetString() << "'" << std::endl;
        assert(false);

        CString errorStr;
        // дополняем ошибку текстом запроса
        errorStr.Format(L"%s. Обратитесь к администратору системы, текст запроса \"%s\"",
                        CString(exc.what()).GetString(), getUNICODEString(query->data).c_str());

        // отвечаем хоть что-то пользователю
        m_telegramThread->sendMessage(query->message->chat->id, errorStr.GetString());
        // оповещаем об ошибке
        send_message_to_log(LogMessageData::MessageType::eError, errorStr);
    }
}

//----------------------------------------------------------------------------//
bool TelegramCallbacks::gotResponseToPreviousCallback(const TgBot::Message::Ptr& commandMessage)
{
    const std::string lastBotCommand = m_telegramUsers->getUserLastCommand(commandMessage->from);

    try
    {
        // парсим колбэк и проверяем каких параметров не хватает
        CallBackParams callBackParams;

        const std::initializer_list<std::string> callbacksWithAnswer = { alarmingValue::kKeyWord };
        for (const auto& callbackKeyWord : callbacksWithAnswer)
        {
            if (boost::spirit::x3::parse(lastBotCommand.begin(), lastBotCommand.end(),
                                         callbackKeyWord >> CallbackParser::parser, callBackParams))
            {
                if (const auto callbackIt = m_commandCallbacks.find(callbackKeyWord);
                    callbackIt != m_commandCallbacks.end())
                {
                    const CommandCallback& callback = callbackIt->second;
                    (this->*callback)(commandMessage, callBackParams, true);
                    return true;
                }
                else
                {
                    std::cerr << "Отсутствует обработчик для команды " + callbackKeyWord + "!";
                    assert(false);
                    throw std::runtime_error("Отсутствует обработчик для команды " + callbackKeyWord + "!");
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        OUTPUT_LOG_FUNC;
        OUTPUT_LOG_SET_TEXT(L"Неизвестное сообщение от пользователя: %s\nТекст сообщения: %s\nПоследняя команда пользователя: %s\nОшибка: %s",
                            getUNICODEString(commandMessage->from->username).c_str(),
                            getUNICODEString(commandMessage->text).c_str(),
                            getUNICODEString(lastBotCommand).c_str(),
                            CString(exception.what()).GetString());
    }

    return false;
}

//----------------------------------------------------------------------------//
// IEventRecipient
void TelegramCallbacks::onEvent(const EventId& code, float /*eventValue*/, std::shared_ptr<IEventData> eventData)
{
    if (code == onCompletingMonitoringTask)
    {
        MonitoringResult::Ptr monitoringResult = std::static_pointer_cast<MonitoringResult>(eventData);

        // проверяем что это наше задание
        auto taskIt = m_monitoringTasksInfo.find(monitoringResult->m_taskId);
        if (taskIt == m_monitoringTasksInfo.end())
            return;

        // флаг что нужен детальный отчёт
        const bool bDetailedInfo = taskIt->second.userStatus == ITelegramUsersList::UserStatus::eAdmin;
        // запоминаем идентификатор чата
        const int64_t chatId = taskIt->second.chatId;

        // удаляем задание из списка
        m_monitoringTasksInfo.erase(taskIt);

        // формируем отчёт
        CString reportText;
        reportText.Format(L"Отчёт за %s - %s\n\n",
                          monitoringResult->m_taskResults.front().pTaskParameters->startTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString(),
                          monitoringResult->m_taskResults.front().pTaskParameters->endTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString());

        // формируем отчёт по всем каналам
        for (auto& channelResData : monitoringResult->m_taskResults)
        {
            // допустимое количество пропусков данных - десятая часть от интервала
            CTimeSpan permissibleEmptyDataTime =
                (channelResData.pTaskParameters->endTime - channelResData.pTaskParameters->startTime).GetTotalSeconds() / 30;

            reportText.AppendFormat(L"Канал \"%s\":", channelResData.pTaskParameters->channelName.GetString());

            switch (channelResData.resultType)
            {
            case MonitoringResult::Result::eSucceeded:  // данные успешно получены
                {
                    if (bDetailedInfo)
                    {
                        // значение, достугнув которое необходимо оповестить. Не оповещать - NAN
                        float alarmingValue = NAN;

                        // если отчёт подробный - ищем какое оповещательное значение у канала
                        auto pMonitoringService = get_monitoring_service();
                        for (size_t i = 0, count = pMonitoringService->getNumberOfMonitoringChannels();
                             i < count; ++i)
                        {
                            const MonitoringChannelData& channelData = pMonitoringService->getMonitoringChannelData(i);
                            if (channelData.channelName == channelResData.pTaskParameters->channelName)
                            {
                                alarmingValue = channelData.alarmingValue;
                                break;
                            }
                        }

                        // если мы нашли значение при котором стоит оповещать - проверяем превышение этого значения
                        if (isfinite(alarmingValue))
                        {
                            // если за интервал одно из значений вышло за допустимые
                            if ((alarmingValue >= 0 && channelResData.maxValue >= alarmingValue) ||
                                (alarmingValue < 0 && channelResData.minValue <= alarmingValue))
                                reportText.AppendFormat(L"допустимый уровень %.02f был превышен, ",
                                                        alarmingValue);
                        }
                    }

                    reportText.AppendFormat(L"значения за интервал [%.02f..%.02f], последнее показание - %.02f.",
                                            channelResData.minValue, channelResData.maxValue,
                                            channelResData.currentValue);

                    // если много пропусков данных
                    if (bDetailedInfo && channelResData.emptyDataTime > permissibleEmptyDataTime)
                        reportText.AppendFormat(L" Много пропусков данных (%s).",
                                                time_span_to_string(channelResData.emptyDataTime).GetString());
                }
                break;
            case MonitoringResult::Result::eNoData:     // в переданном интервале нет данных
            case MonitoringResult::Result::eErrorText:  // возникла ошибка
                {
                    // оповещаем о возникшей ошибке
                    if (!channelResData.errorText.IsEmpty())
                        reportText.Append(channelResData.errorText);
                    else
                        reportText.Append(L"Нет данных в запрошенном интервале.");
                }
                break;
            default:
                assert(!"Не известный тип результата");
                break;
            }

            reportText += L"\n";
        }

        // отправляем ответом текст отчёта
        m_telegramThread->sendMessage(chatId, reportText.GetString());
    }
    else if (code == onMonitoringErrorEvent)
    {
        std::shared_ptr<MonitoringErrorEventData> errorData = std::static_pointer_cast<MonitoringErrorEventData>(eventData);
        assert(!errorData->errorText.IsEmpty());

        // получаем список админских чатов
        const auto adminsChats = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin);
        if (adminsChats.empty() || errorData->errorText.IsEmpty())
            return;

        // добавляем новую ошибку в список и запоминаем её идентификатор
        m_monitoringErrors.emplace_back(errorData.get());

        if (m_monitoringErrors.size() > g_kMaxErrorInfoCount)
            m_monitoringErrors.pop_front();

        // Добавляем кнопки действий для этой ошибки
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

        // колбэк на перезапуск мониторинга
        // kKeyWord
        KeyboardCallback callbackRestart(restart::kKeyWord);

        // колбэк на передачу этого сообщения обычным пользователям
        // kKeyWord kParamid={'GUID'}
        KeyboardCallback callBackOrdinaryUsers(resend::kKeyWord);
        callBackOrdinaryUsers.addCallbackParam(resend::kParamId, CString(CComBSTR(errorData->errorGUID)));

        keyboard->inlineKeyboard.push_back({ create_keyboard_button(L"Перезапустить систему", callbackRestart),
                                           create_keyboard_button(L"Оповестить обычных пользователей", callBackOrdinaryUsers) });

        // пересылаем всем админам текст ошибки и клавиатуру для решения проблем
        m_telegramThread->sendMessage(adminsChats, errorData->errorText.GetString(), false, 0, keyboard);
    }
    else
        assert(!"Неизвестное событие");
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackReport(const TgBot::Message::Ptr& message, const CallBackParams& reportParams, bool /*gotAnswer*/)
{
    // В колбэке отчёта должны быть определенные параметры, итоговый колбэк должен быть вида
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(ОПЦИОНАЛЬНО) kParamInterval={'1000000'}

    // тип отчёта
    auto reportTypeIt = reportParams.find(report::kParamType);
    // проверяем какой вид колбэка пришел и каких параметров не хватает
    if (reportTypeIt == reportParams.end())
        throw std::runtime_error("Не известный колбэк.");

    // получаем список каналов
    std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("Не удалось получить список каналов, попробуйте повторить попытку");

    // имя канала по которому делаем отчёт
    const auto channelIt = reportParams.find(report::kParamChan);

    // тип отчёта
    const report::ReportType reportType = (report::ReportType)std::stoul(reportTypeIt->second);

    // первый колбэк формата "kKeyWord kParamType={'ReportType'}" и проверяем может надо спросить по какому каналу нужен отчёт
    switch (reportType)
    {
    default:
        assert(!"Не известный тип отчёта.");
        [[fallthrough]];
    case report::ReportType::eSpecialChannel:
        {
            // если канал не указан надо его запросить
            if (channelIt == reportParams.end())
            {
                // формируем колбэк
                KeyboardCallback defCallBack(report::kKeyWord);
                defCallBack.addCallbackParam(reportTypeIt->first, reportTypeIt->second);

                // показываем пользователю кнопки в выбором канала по которому нужен отчёт
                TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
                keyboard->inlineKeyboard.reserve(monitoringChannels.size());

                for (const auto& channel : monitoringChannels)
                {
                    const auto channelButton =
                        create_keyboard_button(channel, KeyboardCallback(defCallBack).addCallbackParam(report::kParamChan, channel));

                    keyboard->inlineKeyboard.push_back({ channelButton });
                }

                m_telegramThread->sendMessage(message->chat->id, L"Выберите канал", false, 0, keyboard);
                return;
            }
        }
        break;

    case report::ReportType::eAllChannels:
        break;
    }

    const auto timeIntervalIt = reportParams.find(report::kParamInterval);
    // проверяем что kParamInterval задан
    if (timeIntervalIt == reportParams.end())
    {
        // формируем колбэк
        KeyboardCallback defCallBack(report::kKeyWord);
        defCallBack.addCallbackParam(reportTypeIt->first, reportTypeIt->second);

        // если указано имя - добавляем его
        if (channelIt != reportParams.end())
            defCallBack.addCallbackParam(channelIt->first, channelIt->second);

        // просим пользователя задать интервал
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        keyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);

        // добавляем кнопки со всеми интервалами
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            const auto intervalButton = create_keyboard_button(
                monitoring_interval_to_string(MonitoringInterval(i)),
                KeyboardCallback(defCallBack).addCallbackParam(report::kParamInterval, std::to_wstring(i)));

            keyboard->inlineKeyboard[i] = { intervalButton };
        }

        m_telegramThread->sendMessage(message->chat->id,
                                      L"Выберите интервал времени за который нужно показать отчёт",
                                      false, 0, keyboard);
    }
    else
    {
        // получили все необходимые параметры, запускаем задание мониторинга
        // список каналов для мониторинга
        std::list<CString> channelsForTask;

        switch (reportType)
        {
        default:
            assert(!"Не известный тип отчёта.");
            [[fallthrough]];
        case report::ReportType::eAllChannels:
            channelsForTask = std::move(monitoringChannels);
            break;

        case report::ReportType::eSpecialChannel:
            if (channelIt == reportParams.end())
                throw std::runtime_error("Не удалось распознать имя канала, попробуйте повторить попытку");

            channelsForTask.emplace_back(getUNICODEString(channelIt->second).c_str());
            break;
        }

        const CTime stopTime = CTime::GetCurrentTime();
        const CTime startTime = stopTime -
            monitoring_interval_to_timespan((MonitoringInterval)std::stoi(timeIntervalIt->second));

        TaskInfo taskInfo;
        taskInfo.chatId = message->chat->id;
        taskInfo.userStatus = m_telegramUsers->getUserStatus(message->from);

        // Отвечаем пользователю что запрос выполняется, выполняться может долго
        // пользователь может испугаться что ничего не происходит
        m_telegramThread->sendMessage(message->chat->id,
                                      L"Выполняется расчёт данных, это может занять некоторое время.");

        m_monitoringTasksInfo.try_emplace(
            get_monitoring_tasks_service()->addTaskList(channelsForTask, startTime, stopTime,
                                                        IMonitoringTasksService::eHigh),
            std::move(taskInfo));
    }
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackRestart(const TgBot::Message::Ptr& message, const CallBackParams& params, bool /*gotAnswer*/)
{
    assert(params.empty() && "Параметров не предусмотрено");

    std::wstring messageToUser;
    if (!get_service<CommandsInfoService>().ensureNeedAnswerOnCommand(m_telegramUsers, CommandsInfoService::Command::eRestart, message, messageToUser))
    {
        m_telegramThread->sendMessage(message->chat->id, messageToUser);
    }
    else
        // эмитируем что пользователь выполнил запрос рестарта
        execute_restart_command(message, m_telegramThread.get());
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackResend(const TgBot::Message::Ptr& message, const CallBackParams& params, bool /*gotAnswer*/)
{
    const auto errorIdIt = params.find(resend::kParamId);
    if (errorIdIt == params.end())
        throw std::runtime_error("Нет необходимого параметра у колбэка пересылки сообщения.");

    // вытаскиваем гуид из строки
    GUID errorGUID;
    if (FAILED(CLSIDFromString(CComBSTR(errorIdIt->second.c_str()), &errorGUID)))
        assert(!"Не удалось получить гуид!");

    const auto errorIt = std::find_if(m_monitoringErrors.begin(), m_monitoringErrors.end(),
                                      [&errorGUID](const ErrorInfo& errorInfo)
                                      {
                                          return IsEqualGUID(errorGUID, errorInfo.errorGUID);
                                      });
    if (errorIt == m_monitoringErrors.end())
    {
        CString text;
        text.Format(L"Пересылаемой ошибки нет в списке, возможно ошибка является устаревшей (хранятся последние %u ошибок) или программа была перезапущена.",
                    g_kMaxErrorInfoCount);
        m_telegramThread->sendMessage(message->chat->id, text.GetString());
        return;
    }

    if (errorIt->bResendToOrdinaryUsers)
    {
        m_telegramThread->sendMessage(message->chat->id,
                                      L"Ошибка уже была переслана.");
        return;
    }

    // пересылаем ошибку обычным пользователям
    const auto ordinaryUsersChatList = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser);
    m_telegramThread->sendMessage(ordinaryUsersChatList, errorIt->errorText.GetString());

    errorIt->bResendToOrdinaryUsers = true;

    m_telegramThread->sendMessage(message->chat->id,
                                  L"Ошибка была успешно переслана обычным пользователям.");
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackAlert(const TgBot::Message::Ptr& message, const CallBackParams& params, bool /*gotAnswer*/)
{
    // Формат колбэка kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
    const auto enableParam = params.find(alertEnabling::kParamEnable);
    const auto channelParam = params.find(alertEnabling::kParamChan);

    if (enableParam == params.end() || channelParam == params.end())
        throw std::runtime_error("Нет необходимого параметра у колбэка управления оповещениями.");
    // включаемость/выключаемость оповещений
    const bool bEnableAlert = enableParam->second == "true";
    // сервис мониторинга
    auto* monitoringService = get_monitoring_service();
    // сообщение в ответ пользователю
    CString messageText;
    if (channelParam->second == alertEnabling::kValueAllChannels)
    {
        // настраивают оповещения для всех каналов
        size_t channelsCount = monitoringService->getNumberOfMonitoringChannels();
        if (channelsCount == 0)
            throw std::runtime_error("Нет выбранных для мониторинга каналов, обратитесь к администратору");

        for (size_t channelInd = 0; channelInd < channelsCount; ++channelInd)
        {
            monitoringService->changeMonitoringChannelNotify(channelInd, bEnableAlert);
        }

        messageText.Format(L"Оповещения для всех каналов %s", bEnableAlert ? L"включены" : L"выключены");
    }
    else
    {
        // получаем список каналов
        std::list<CString> monitoringChannels = monitoringService->getNamesOfMonitoringChannels();
        if (monitoringChannels.empty())
            throw std::runtime_error("Нет выбранных для мониторинга каналов, обратитесь к администратору");

        // имя канала из колбэка
        const CString callBackChannel = getUNICODEString(channelParam->second).c_str();
        // считаем что в списке мониторинга каналы по именам не повторяются иначе это глупо
        const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
                                            [&callBackChannel](const auto& channelName)
                                            {
                                                return callBackChannel == channelName;
                                            });

        if (channelIt == monitoringChannels.cend())
            throw std::runtime_error("В данный момент в списке мониторинга нет выбранного вами канала.");

        monitoringService->changeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt),
                                                         bEnableAlert);

        messageText.Format(L"Оповещения для канала %s %s", callBackChannel.GetString(), bEnableAlert ? L"включены" : L"выключены");
    }

    assert(!messageText.IsEmpty() && "Сообщение пользователю пустое.");
    m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackAlarmValue(const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer)
{
    // Формат колбэка kKeyWord kParamChan={'chan1'} kLevel={'5.5'}
    const auto channelParam = params.find(alarmingValue::kParamChan);

    if (channelParam == params.end())
        throw std::runtime_error("Нет необходимого параметра у колбэка управления оповещениями.");

    // получаем список каналов
    const std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("Нет выбранных для мониторинга каналов, обратитесь к администратору");

    const std::wstring callBackChannel = getUNICODEString(channelParam->second).c_str();
    const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
                                        [_channelName = CString(callBackChannel.c_str())](const auto& channelName)
    {
        return channelName == _channelName;
    });
    if (channelIt == monitoringChannels.cend())
        throw std::runtime_error("Выбранный канал отсутствует в списке наблюдаемых каналов.");

    auto getNewLevelFromText = [](const std::string& text)
    {
        float newLevel = NAN;
        if (text != "NAN")
        {
            std::istringstream str(text);
            str >> newLevel;
            if (str.fail())
                throw std::runtime_error("Не удалось преобразовать переданное значение в число.");
        }

        return newLevel;
    };

    const auto newLevelParam = params.find(alarmingValue::kParamValue);
    if (newLevelParam == params.end())
    {
        if (!gotAnswer)
        {
            m_telegramThread->sendMessage(message->chat->id, L"Для того чтобы изменить допустимый уровень значений у канала '" + callBackChannel +
                                          L"' отправьте новый уровень ответным сообщением, отправьте NAN чтобы отключить оповещения совсем.");
            return;
        }

        CString messageText;
        std::wstringstream newLevelText;

        const float newLevel = getNewLevelFromText(message->text);
        if (!isfinite(newLevel))
        {
            if (!get_monitoring_service()->getMonitoringChannelData(std::distance(monitoringChannels.cbegin(), channelIt)).bNotify)
            {
                messageText.Format(L"Оповещения у канала '%s' уже выключены.", callBackChannel.c_str());
                m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
                return;
            }

            newLevelText << L"NAN";
            messageText.Format(L"Отключить оповещения для канала '%s'?", callBackChannel.c_str());
        }
        else
        {
            newLevelText << newLevel;
            messageText.Format(L"Установить значение оповещений для канала '%s' как %s?", callBackChannel.c_str(), newLevelText.str().c_str());
        }

        // формируем колбэк подтверждения операции
        KeyboardCallback acceptCallBack(alarmingValue::kKeyWord);
        acceptCallBack.addCallbackParam(alarmingValue::kParamChan, callBackChannel);
        acceptCallBack.addCallbackParam(alarmingValue::kParamValue, newLevelText.str());

        TgBot::InlineKeyboardMarkup::Ptr acceptingOperationKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        acceptingOperationKeyboard->inlineKeyboard = {{ create_keyboard_button(L"Установить", acceptCallBack) }};

        m_telegramThread->sendMessage(message->chat->id, messageText.GetString(), false, 0, acceptingOperationKeyboard);
    }
    else
    {
        const float newLevel = getNewLevelFromText(newLevelParam->second);

        CString messageText;
        if (!isfinite(newLevel))
        {
            messageText.Format(L"Оповещения для канала '%s' выключены", callBackChannel.c_str());
            get_monitoring_service()->changeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt), false);
        }
        else
        {
            std::wstringstream newLevelText;
            newLevelText << newLevel;

            messageText.Format(L"Значение %s установлено для канала '%s' успешно", newLevelText.str().c_str(), callBackChannel.c_str());
            get_monitoring_service()->changeMonitoringChannelAlarmingValue(std::distance(monitoringChannels.cbegin(), channelIt), newLevel);
        }

        m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
    }
}

} // namespace callback
