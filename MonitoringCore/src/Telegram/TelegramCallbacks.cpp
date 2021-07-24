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
// ������������ ���������� ��������� ������ �������� ����������
constexpr size_t g_kMaxErrorInfoCount = 200;

////////////////////////////////////////////////////////////////////////////////
// ������� ��������
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
    // ������������� �� ������� � ���������� �����������
    EventRecipientImpl::subscribe(onCompletingMonitoringTask);
    // ������������� �� ������� � ����������� ������� � �������� ������� ����� ������
    EventRecipientImpl::subscribe(onMonitoringErrorEvent);

    // TODO C++20 replace to template lambda
    auto addCommandCallback = [commandCallbacks = &m_commandCallbacks]
        (const std::string& keyWord, const CommandCallback& callback)
        {
            if (!commandCallbacks->try_emplace(keyWord, callback).second)
                assert(!"�������� ����� � �������� ������ ����������!");
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
        // ������ ������ � ��������� ����� ���������� �� �������
        CallBackParams callBackParams;

        for (auto& [keyWord, callback] : m_commandCallbacks)
        {
            if (boost::spirit::x3::parse(query->data.begin(), query->data.end(),
                                         keyWord >> CallbackParser::parser, callBackParams))
            {
                if (!get_service<CommandsInfoService>().ensureNeedAnswerOnCallback(m_telegramUsers.get(), keyWord, query->message))
                {
                    // ������������ ����������� ���������
                    const TgBot::User::Ptr& pUser = query->message->from;

                    if (m_telegramUsers->getUserStatus(pUser) == ITelegramUsersList::eNotAuthorized)
                        m_telegramThread->sendMessage(query->message->chat->id, L"��� ������ ���� ��� ���������� ��������������.");
                    else
                        m_telegramThread->sendMessage(query->message->chat->id, L"����������� ��� ����� �� ��������� �������.");
                }
                else
                    (this->*callback)(query->message, callBackParams, false);

                return;
            }
        }

        throw std::runtime_error("������ ������� �������");
    }
    catch (const std::exception& exc)
    {
        std::cerr << exc.what() << " Callback '" << CStringA(getUNICODEString(query->data).c_str()).GetString() << "'" << std::endl;
        assert(false);

        CString errorStr;
        // ��������� ������ ������� �������
        errorStr.Format(L"%s. ���������� � �������������� �������, ����� ������� \"%s\"",
                        CString(exc.what()).GetString(), getUNICODEString(query->data).c_str());

        // �������� ���� ���-�� ������������
        m_telegramThread->sendMessage(query->message->chat->id, errorStr.GetString());
        // ��������� �� ������
        send_message_to_log(LogMessageData::MessageType::eError, errorStr);
    }
}

//----------------------------------------------------------------------------//
bool TelegramCallbacks::gotResponseToPreviousCallback(const TgBot::Message::Ptr& commandMessage)
{
    const std::string lastBotCommand = m_telegramUsers->getUserLastCommand(commandMessage->from);

    try
    {
        // ������ ������ � ��������� ����� ���������� �� �������
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
                    std::cerr << "����������� ���������� ��� ������� " + callbackKeyWord + "!";
                    assert(false);
                    throw std::runtime_error("����������� ���������� ��� ������� " + callbackKeyWord + "!");
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        OUTPUT_LOG_FUNC;
        OUTPUT_LOG_SET_TEXT(L"����������� ��������� �� ������������: %s\n����� ���������: %s\n��������� ������� ������������: %s\n������: %s",
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

        // ��������� ��� ��� ���� �������
        auto taskIt = m_monitoringTasksInfo.find(monitoringResult->m_taskId);
        if (taskIt == m_monitoringTasksInfo.end())
            return;

        // ���� ��� ����� ��������� �����
        const bool bDetailedInfo = taskIt->second.userStatus == ITelegramUsersList::UserStatus::eAdmin;
        // ���������� ������������� ����
        const int64_t chatId = taskIt->second.chatId;

        // ������� ������� �� ������
        m_monitoringTasksInfo.erase(taskIt);

        // ��������� �����
        CString reportText;
        reportText.Format(L"����� �� %s - %s\n\n",
                          monitoringResult->m_taskResults.front().pTaskParameters->startTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString(),
                          monitoringResult->m_taskResults.front().pTaskParameters->endTime.Format(L"%d.%m.%Y  %H:%M:%S").GetString());

        // ��������� ����� �� ���� �������
        for (auto& channelResData : monitoringResult->m_taskResults)
        {
            // ���������� ���������� ��������� ������ - ������� ����� �� ���������
            CTimeSpan permissibleEmptyDataTime =
                (channelResData.pTaskParameters->endTime - channelResData.pTaskParameters->startTime).GetTotalSeconds() / 30;

            reportText.AppendFormat(L"����� \"%s\":", channelResData.pTaskParameters->channelName.GetString());

            switch (channelResData.resultType)
            {
            case MonitoringResult::Result::eSucceeded:  // ������ ������� ��������
                {
                    if (bDetailedInfo)
                    {
                        // ��������, ��������� ������� ���������� ����������. �� ��������� - NAN
                        float alarmingValue = NAN;

                        // ���� ����� ��������� - ���� ����� �������������� �������� � ������
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

                        // ���� �� ����� �������� ��� ������� ����� ��������� - ��������� ���������� ����� ��������
                        if (isfinite(alarmingValue))
                        {
                            // ���� �� �������� ���� �� �������� ����� �� ����������
                            if ((alarmingValue >= 0 && channelResData.maxValue >= alarmingValue) ||
                                (alarmingValue < 0 && channelResData.minValue <= alarmingValue))
                                reportText.AppendFormat(L"���������� ������� %.02f ��� ��������, ",
                                                        alarmingValue);
                        }
                    }

                    reportText.AppendFormat(L"�������� �� �������� [%.02f..%.02f], ��������� ��������� - %.02f.",
                                            channelResData.minValue, channelResData.maxValue,
                                            channelResData.currentValue);

                    // ���� ����� ��������� ������
                    if (bDetailedInfo && channelResData.emptyDataTime > permissibleEmptyDataTime)
                        reportText.AppendFormat(L" ����� ��������� ������ (%s).",
                                                time_span_to_string(channelResData.emptyDataTime).GetString());
                }
                break;
            case MonitoringResult::Result::eNoData:     // � ���������� ��������� ��� ������
            case MonitoringResult::Result::eErrorText:  // �������� ������
                {
                    // ��������� � ��������� ������
                    if (!channelResData.errorText.IsEmpty())
                        reportText.Append(channelResData.errorText);
                    else
                        reportText.Append(L"��� ������ � ����������� ���������.");
                }
                break;
            default:
                assert(!"�� ��������� ��� ����������");
                break;
            }

            reportText += L"\n";
        }

        // ���������� ������� ����� ������
        m_telegramThread->sendMessage(chatId, reportText.GetString());
    }
    else if (code == onMonitoringErrorEvent)
    {
        std::shared_ptr<MonitoringErrorEventData> errorData = std::static_pointer_cast<MonitoringErrorEventData>(eventData);
        assert(!errorData->errorText.IsEmpty());

        // �������� ������ ��������� �����
        const auto adminsChats = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eAdmin);
        if (adminsChats.empty() || errorData->errorText.IsEmpty())
            return;

        // ��������� ����� ������ � ������ � ���������� � �������������
        m_monitoringErrors.emplace_back(errorData.get());

        if (m_monitoringErrors.size() > g_kMaxErrorInfoCount)
            m_monitoringErrors.pop_front();

        // ��������� ������ �������� ��� ���� ������
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

        // ������ �� ���������� �����������
        // kKeyWord
        KeyboardCallback callbackRestart(restart::kKeyWord);

        // ������ �� �������� ����� ��������� ������� �������������
        // kKeyWord kParamid={'GUID'}
        KeyboardCallback callBackOrdinaryUsers(resend::kKeyWord);
        callBackOrdinaryUsers.addCallbackParam(resend::kParamId, CString(CComBSTR(errorData->errorGUID)));

        keyboard->inlineKeyboard.push_back({ create_keyboard_button(L"������������� �������", callbackRestart),
                                           create_keyboard_button(L"���������� ������� �������������", callBackOrdinaryUsers) });

        // ���������� ���� ������� ����� ������ � ���������� ��� ������� �������
        m_telegramThread->sendMessage(adminsChats, errorData->errorText.GetString(), false, 0, keyboard);
    }
    else
        assert(!"����������� �������");
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackReport(const TgBot::Message::Ptr& message, const CallBackParams& reportParams, bool /*gotAnswer*/)
{
    // � ������� ������ ������ ���� ������������ ���������, �������� ������ ������ ���� ����
    // kKeyWord kParamType={'ReportType'} kParamChan={'chan1'}(�����������) kParamInterval={'1000000'}

    // ��� ������
    auto reportTypeIt = reportParams.find(report::kParamType);
    // ��������� ����� ��� ������� ������ � ����� ���������� �� �������
    if (reportTypeIt == reportParams.end())
        throw std::runtime_error("�� ��������� ������.");

    // �������� ������ �������
    std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("�� ������� �������� ������ �������, ���������� ��������� �������");

    // ��� ������ �� �������� ������ �����
    const auto channelIt = reportParams.find(report::kParamChan);

    // ��� ������
    const report::ReportType reportType = (report::ReportType)std::stoul(reportTypeIt->second);

    // ������ ������ ������� "kKeyWord kParamType={'ReportType'}" � ��������� ����� ���� �������� �� ������ ������ ����� �����
    switch (reportType)
    {
    default:
        assert(!"�� ��������� ��� ������.");
        [[fallthrough]];
    case report::ReportType::eSpecialChannel:
        {
            // ���� ����� �� ������ ���� ��� ���������
            if (channelIt == reportParams.end())
            {
                // ��������� ������
                KeyboardCallback defCallBack(report::kKeyWord);
                defCallBack.addCallbackParam(reportTypeIt->first, reportTypeIt->second);

                // ���������� ������������ ������ � ������� ������ �� �������� ����� �����
                TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
                keyboard->inlineKeyboard.reserve(monitoringChannels.size());

                for (const auto& channel : monitoringChannels)
                {
                    const auto channelButton =
                        create_keyboard_button(channel, KeyboardCallback(defCallBack).addCallbackParam(report::kParamChan, channel));

                    keyboard->inlineKeyboard.push_back({ channelButton });
                }

                m_telegramThread->sendMessage(message->chat->id, L"�������� �����", false, 0, keyboard);
                return;
            }
        }
        break;

    case report::ReportType::eAllChannels:
        break;
    }

    const auto timeIntervalIt = reportParams.find(report::kParamInterval);
    // ��������� ��� kParamInterval �����
    if (timeIntervalIt == reportParams.end())
    {
        // ��������� ������
        KeyboardCallback defCallBack(report::kKeyWord);
        defCallBack.addCallbackParam(reportTypeIt->first, reportTypeIt->second);

        // ���� ������� ��� - ��������� ���
        if (channelIt != reportParams.end())
            defCallBack.addCallbackParam(channelIt->first, channelIt->second);

        // ������ ������������ ������ ��������
        TgBot::InlineKeyboardMarkup::Ptr keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        keyboard->inlineKeyboard.resize((size_t)MonitoringInterval::eLast);

        // ��������� ������ �� ����� �����������
        for (int i = (int)MonitoringInterval::eLast - 1; i >= 0; --i)
        {
            const auto intervalButton = create_keyboard_button(
                monitoring_interval_to_string(MonitoringInterval(i)),
                KeyboardCallback(defCallBack).addCallbackParam(report::kParamInterval, std::to_wstring(i)));

            keyboard->inlineKeyboard[i] = { intervalButton };
        }

        m_telegramThread->sendMessage(message->chat->id,
                                      L"�������� �������� ������� �� ������� ����� �������� �����",
                                      false, 0, keyboard);
    }
    else
    {
        // �������� ��� ����������� ���������, ��������� ������� �����������
        // ������ ������� ��� �����������
        std::list<CString> channelsForTask;

        switch (reportType)
        {
        default:
            assert(!"�� ��������� ��� ������.");
            [[fallthrough]];
        case report::ReportType::eAllChannels:
            channelsForTask = std::move(monitoringChannels);
            break;

        case report::ReportType::eSpecialChannel:
            if (channelIt == reportParams.end())
                throw std::runtime_error("�� ������� ���������� ��� ������, ���������� ��������� �������");

            channelsForTask.emplace_back(getUNICODEString(channelIt->second).c_str());
            break;
        }

        const CTime stopTime = CTime::GetCurrentTime();
        const CTime startTime = stopTime -
            monitoring_interval_to_timespan((MonitoringInterval)std::stoi(timeIntervalIt->second));

        TaskInfo taskInfo;
        taskInfo.chatId = message->chat->id;
        taskInfo.userStatus = m_telegramUsers->getUserStatus(message->from);

        // �������� ������������ ��� ������ �����������, ����������� ����� �����
        // ������������ ����� ���������� ��� ������ �� ����������
        m_telegramThread->sendMessage(message->chat->id,
                                      L"����������� ������ ������, ��� ����� ������ ��������� �����.");

        m_monitoringTasksInfo.try_emplace(
            get_monitoring_tasks_service()->addTaskList(channelsForTask, startTime, stopTime,
                                                        IMonitoringTasksService::eHigh),
            std::move(taskInfo));
    }
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackRestart(const TgBot::Message::Ptr& message, const CallBackParams& params, bool /*gotAnswer*/)
{
    assert(params.empty() && "���������� �� �������������");

    std::wstring messageToUser;
    if (!get_service<CommandsInfoService>().ensureNeedAnswerOnCommand(m_telegramUsers, CommandsInfoService::Command::eRestart, message, messageToUser))
    {
        m_telegramThread->sendMessage(message->chat->id, messageToUser);
    }
    else
        // ��������� ��� ������������ �������� ������ ��������
        execute_restart_command(message, m_telegramThread.get());
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackResend(const TgBot::Message::Ptr& message, const CallBackParams& params, bool /*gotAnswer*/)
{
    const auto errorIdIt = params.find(resend::kParamId);
    if (errorIdIt == params.end())
        throw std::runtime_error("��� ������������ ��������� � ������� ��������� ���������.");

    // ����������� ���� �� ������
    GUID errorGUID;
    if (FAILED(CLSIDFromString(CComBSTR(errorIdIt->second.c_str()), &errorGUID)))
        assert(!"�� ������� �������� ����!");

    const auto errorIt = std::find_if(m_monitoringErrors.begin(), m_monitoringErrors.end(),
                                      [&errorGUID](const ErrorInfo& errorInfo)
                                      {
                                          return IsEqualGUID(errorGUID, errorInfo.errorGUID);
                                      });
    if (errorIt == m_monitoringErrors.end())
    {
        CString text;
        text.Format(L"������������ ������ ��� � ������, �������� ������ �������� ���������� (�������� ��������� %u ������) ��� ��������� ���� ������������.",
                    g_kMaxErrorInfoCount);
        m_telegramThread->sendMessage(message->chat->id, text.GetString());
        return;
    }

    if (errorIt->bResendToOrdinaryUsers)
    {
        m_telegramThread->sendMessage(message->chat->id,
                                      L"������ ��� ���� ���������.");
        return;
    }

    // ���������� ������ ������� �������������
    const auto ordinaryUsersChatList = m_telegramUsers->getAllChatIdsByStatus(ITelegramUsersList::UserStatus::eOrdinaryUser);
    m_telegramThread->sendMessage(ordinaryUsersChatList, errorIt->errorText.GetString());

    errorIt->bResendToOrdinaryUsers = true;

    m_telegramThread->sendMessage(message->chat->id,
                                  L"������ ���� ������� ��������� ������� �������������.");
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackAlert(const TgBot::Message::Ptr& message, const CallBackParams& params, bool /*gotAnswer*/)
{
    // ������ ������� kKeyWord kParamEnable={'true'} kParamChan={'chan1'}
    const auto enableParam = params.find(alertEnabling::kParamEnable);
    const auto channelParam = params.find(alertEnabling::kParamChan);

    if (enableParam == params.end() || channelParam == params.end())
        throw std::runtime_error("��� ������������ ��������� � ������� ���������� ������������.");
    // ������������/������������� ����������
    const bool bEnableAlert = enableParam->second == "true";
    // ������ �����������
    auto* monitoringService = get_monitoring_service();
    // ��������� � ����� ������������
    CString messageText;
    if (channelParam->second == alertEnabling::kValueAllChannels)
    {
        // ����������� ���������� ��� ���� �������
        size_t channelsCount = monitoringService->getNumberOfMonitoringChannels();
        if (channelsCount == 0)
            throw std::runtime_error("��� ��������� ��� ����������� �������, ���������� � ��������������");

        for (size_t channelInd = 0; channelInd < channelsCount; ++channelInd)
        {
            monitoringService->changeMonitoringChannelNotify(channelInd, bEnableAlert);
        }

        messageText.Format(L"���������� ��� ���� ������� %s", bEnableAlert ? L"��������" : L"���������");
    }
    else
    {
        // �������� ������ �������
        std::list<CString> monitoringChannels = monitoringService->getNamesOfMonitoringChannels();
        if (monitoringChannels.empty())
            throw std::runtime_error("��� ��������� ��� ����������� �������, ���������� � ��������������");

        // ��� ������ �� �������
        const CString callBackChannel = getUNICODEString(channelParam->second).c_str();
        // ������� ��� � ������ ����������� ������ �� ������ �� ����������� ����� ��� �����
        const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
                                            [&callBackChannel](const auto& channelName)
                                            {
                                                return callBackChannel == channelName;
                                            });

        if (channelIt == monitoringChannels.cend())
            throw std::runtime_error("� ������ ������ � ������ ����������� ��� ���������� ���� ������.");

        monitoringService->changeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt),
                                                         bEnableAlert);

        messageText.Format(L"���������� ��� ������ %s %s", callBackChannel.GetString(), bEnableAlert ? L"��������" : L"���������");
    }

    assert(!messageText.IsEmpty() && "��������� ������������ ������.");
    m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
}

//----------------------------------------------------------------------------//
void TelegramCallbacks::executeCallbackAlarmValue(const TgBot::Message::Ptr& message, const CallBackParams& params, bool gotAnswer)
{
    // ������ ������� kKeyWord kParamChan={'chan1'} kLevel={'5.5'}
    const auto channelParam = params.find(alarmingValue::kParamChan);

    if (channelParam == params.end())
        throw std::runtime_error("��� ������������ ��������� � ������� ���������� ������������.");

    // �������� ������ �������
    const std::list<CString> monitoringChannels = get_monitoring_service()->getNamesOfMonitoringChannels();
    if (monitoringChannels.empty())
        throw std::runtime_error("��� ��������� ��� ����������� �������, ���������� � ��������������");

    const std::wstring callBackChannel = getUNICODEString(channelParam->second).c_str();
    const auto channelIt = std::find_if(monitoringChannels.cbegin(), monitoringChannels.cend(),
                                        [_channelName = CString(callBackChannel.c_str())](const auto& channelName)
    {
        return channelName == _channelName;
    });
    if (channelIt == monitoringChannels.cend())
        throw std::runtime_error("��������� ����� ����������� � ������ ����������� �������.");

    auto getNewLevelFromText = [](const std::string& text)
    {
        float newLevel = NAN;
        if (text != "NAN")
        {
            std::istringstream str(text);
            str >> newLevel;
            if (str.fail())
                throw std::runtime_error("�� ������� ������������� ���������� �������� � �����.");
        }

        return newLevel;
    };

    const auto newLevelParam = params.find(alarmingValue::kParamValue);
    if (newLevelParam == params.end())
    {
        if (!gotAnswer)
        {
            m_telegramThread->sendMessage(message->chat->id, L"��� ���� ����� �������� ���������� ������� �������� � ������ '" + callBackChannel +
                                          L"' ��������� ����� ������� �������� ����������, ��������� NAN ����� ��������� ���������� ������.");
            return;
        }

        CString messageText;
        std::wstringstream newLevelText;

        const float newLevel = getNewLevelFromText(message->text);
        if (!isfinite(newLevel))
        {
            if (!get_monitoring_service()->getMonitoringChannelData(std::distance(monitoringChannels.cbegin(), channelIt)).bNotify)
            {
                messageText.Format(L"���������� � ������ '%s' ��� ���������.", callBackChannel.c_str());
                m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
                return;
            }

            newLevelText << L"NAN";
            messageText.Format(L"��������� ���������� ��� ������ '%s'?", callBackChannel.c_str());
        }
        else
        {
            newLevelText << newLevel;
            messageText.Format(L"���������� �������� ���������� ��� ������ '%s' ��� %s?", callBackChannel.c_str(), newLevelText.str().c_str());
        }

        // ��������� ������ ������������� ��������
        KeyboardCallback acceptCallBack(alarmingValue::kKeyWord);
        acceptCallBack.addCallbackParam(alarmingValue::kParamChan, callBackChannel);
        acceptCallBack.addCallbackParam(alarmingValue::kParamValue, newLevelText.str());

        TgBot::InlineKeyboardMarkup::Ptr acceptingOperationKeyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        acceptingOperationKeyboard->inlineKeyboard = {{ create_keyboard_button(L"����������", acceptCallBack) }};

        m_telegramThread->sendMessage(message->chat->id, messageText.GetString(), false, 0, acceptingOperationKeyboard);
    }
    else
    {
        const float newLevel = getNewLevelFromText(newLevelParam->second);

        CString messageText;
        if (!isfinite(newLevel))
        {
            messageText.Format(L"���������� ��� ������ '%s' ���������", callBackChannel.c_str());
            get_monitoring_service()->changeMonitoringChannelNotify(std::distance(monitoringChannels.cbegin(), channelIt), false);
        }
        else
        {
            std::wstringstream newLevelText;
            newLevelText << newLevel;

            messageText.Format(L"�������� %s ����������� ��� ������ '%s' �������", newLevelText.str().c_str(), callBackChannel.c_str());
            get_monitoring_service()->changeMonitoringChannelAlarmingValue(std::distance(monitoringChannels.cbegin(), channelIt), newLevel);
        }

        m_telegramThread->sendMessage(message->chat->id, messageText.GetString());
    }
}

} // namespace callback
