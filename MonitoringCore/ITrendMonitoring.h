#pragma once

#include <bitset>
#include <list>
#include <memory>
#include <optional>
#include <set>

#include "Messages.h"

// оповещение об изменении в списке наблюдаемых каналов
// {BFE83474-9AC0-49CE-B26E-752657C366F3}
constexpr EventId onMonitoringListChanged =
{ 0xbfe83474, 0x9ac0, 0x49ce, { 0xb2, 0x6e, 0x75, 0x26, 0x57, 0xc3, 0x66, 0xf3 } };

// оповещение в случае необходимости добавить сообщение в лог, см LogMessageData и send_message_to_log
// {5B436604-D55E-4112-AC8C-B0521314BA25}
constexpr EventId onNewLogMessageEvent =
{ 0x5b436604, 0xd55e, 0x4112, { 0xac, 0x8c, 0xb0, 0x52, 0x13, 0x14, 0xba, 0x25 } };

// оповещение в случае возникшей ошибки в мониторинге, см MonitoringErrorEventData
// {FB0BF8DC-0AA5-41AC-B018-4CFE11CF14BE}
constexpr EventId onMonitoringErrorEvent =
{ 0xfb0bf8dc, 0xaa5, 0x41ac, { 0xb0, 0x18, 0x4c, 0xfe, 0x11, 0xcf, 0x14, 0xbe } };

// оповещение при формировании отчёта, см MessageTextData
// {39A54983-53D5-4D2B-9290-23E2D85ABA5F}
constexpr EventId onReportPreparedEvent =
{ 0x39a54983, 0x53d5, 0x4d2b, { 0x92, 0x90, 0x23, 0xe2, 0xd8, 0x5a, 0xba, 0x5f } };

////////////////////////////////////////////////////////////////////////////////
// Данные тренда
struct TrendChannelData
{
    // начальное значение в начале интервала
    float startValue = 0.f;
    // текущее значение
    float currentValue = 0.f;
    // максимальное значение за наблюдаемый интервал
    float maxValue = 0.f;
    // минимальное значение за наблюдаемый интервал
    float minValue = 0.f;
    // время без данных(пропуски данных) за наблюдаемый интервал
    CTimeSpan emptyDataTime = 0;
    // время последних "существующих" данных по каналу
    CTime lastDataExistTime;
};

////////////////////////////////////////////////////////////////////////////////
// интервалы для мониторинга данных
enum class MonitoringInterval
{
    eOneDay,            // один день
    eOneWeek,           // одна неделя
    eTwoWeeks,          // две недели
    eOneMonth,          // один месяц
    eThreeMonths,       // три месяца
    eHalfYear,          // пол года
    eOneYear,           // один год
    eLast
};

////////////////////////////////////////////////////////////////////////////////
// Структура с данными о канале
struct MonitoringChannelData
{
    // Название канала
    CString channelName;
    // Оповещать об изменениях пользователей
    bool bNotify = true;
    // наблюдаемый интервал
    MonitoringInterval monitoringInterval = MonitoringInterval::eOneMonth;
    // значение, достугнув которое необходимо оповестить. Не оповещать - NAN
    float allarmingValue = NAN;
    // данные по наблюдаемому каналу, пока не eDataLoaded - не валидные/пустые
    TrendChannelData trendData;

    // Состояние канала
    enum ChannelState
    {
        // данные
        //eWaitingForData,           // ждем загрузки данных
        eDataLoaded,                // данные загружены успешно
        eLoadingError,              // ошибка при загрузке данных
        //eErrorOnUpdatingData,       // возникла ошибка при обновлении данных

        // оповещения
        eReportedFallenOff,         // произошло оповещение пользователей об отваливании датчика
        eReportedExcessOfValue,     // произошло оповещение пользователей о превышении допустимого значения
        eReportedALotOfEmptyData,   // произошло оповещение пользователей о большом количестве пропусков

        eLast
    };
    std::bitset<ChannelState::eLast> channelState;
};

////////////////////////////////////////////////////////////////////////////////
// настройки телеграм бота
struct TelegramBotSettings
{
    // состояние работы бота
    bool bEnable = false;
    // токен бота
    CString sToken;
};

////////////////////////////////////////////////////////////////////////////////
// Структура передаваемая в событии onNewLogMessageEvent
struct LogMessageData : public IEventData
{
    // Тип сообщения
    enum class MessageType
    {
        eError,         // сообщение об ошибке, в логе помечается красным цветом
        eOrdinary,      // обычное сообщение
    };
    MessageType messageType = MessageType::eOrdinary;
    // текст сообщения для лога
    CString logMessage;
};

////////////////////////////////////////////////////////////////////////////////
// Структура передаваемая в случае возникновения ошибки мониторинга(onMonitoringErrorEvent)
struct MonitoringErrorEventData : public IEventData
{
    // текст ошибки
    CString errorText;
    // идентификатор ошибки
    GUID errorGUID = {};
};

////////////////////////////////////////////////////////////////////////////////
// Структура с текстом передаваемая в событиях
struct MessageTextData : public IEventData
{
    // текст сообщения
    CString messageText;
};

////////////////////////////////////////////////////////////////////////////////
// Интерфейс для мониторинга данных, использутеся для получения списка каналов и управления им
interface ITrendMonitoring
{
    virtual ~ITrendMonitoring() = default;

#pragma region Общие функции над списком каналов
    /// <summary>Получить список имём всех доступным для мониторинга каналов.</summary>
    virtual std::set<CString> getNamesOfAllChannels() = 0;
    /// <summary>Получить список имен каналов по которым происходит мониторинг.</summary>
    virtual std::list<CString> getNamesOfMonitoringChannels() = 0;

    /// <summary>Обновить данные для всех каналов.</summary>
    virtual void updateDataForAllChannels() = 0;

    /// <summary>Получить количества наблюдаемых каналов.</summary>
    virtual size_t getNumberOfMonitoringChannels() = 0;
    /// <summary>Получить данные для наблюдаемого канала по индексу.</summary>
    /// <param name="channelIndex">Индекс в списке каналов.</param>
    /// <returns>Данные для канала.</returns>
    virtual const MonitoringChannelData& getMonitoringChannelData(const size_t channelIndex) = 0;
#pragma endregion Общие функции над списком каналов

#pragma region Управление списком каналов
    /// <summary>Добавить канал для мониторинга.</summary>
    /// <returns>Индекс добавленного канала в списке.</returns>
    virtual size_t addMonitoingChannel() = 0;
    /// <summary>Удалить наблюдаемый канал из списка по индексу.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <returns>Индекс выделения после удаления.</returns>
    virtual size_t removeMonitoringChannelByIndex(const size_t channelIndex) = 0;

    /// <summary>Изменить флаг оповещения у канала по номеру.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newNotifyState">Новое состояние оповещения.</param>
    virtual void changeMonitoingChannelNotify(const size_t channelIndex,
                                              const bool newNotifyState) = 0;
    /// <summary>Изменить имя наблюдаемого канала.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newChannelName">Новое имя канала.</param>
    virtual void changeMonitoingChannelName(const size_t channelIndex,
                                            const CString& newChannelName) = 0;
    /// <summary>Изменить интервал мониторинга данных для наблюдаемого канала.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newInterval">Новый интервал мониторинга.</param>
    virtual void changeMonitoingChannelInterval(const size_t channelIndex,
                                                const MonitoringInterval newInterval) = 0;
    /// <summary>Изменить значение по достижению которого будет произведено оповещение.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <param name="newValue">Новое значение с оповещением.</param>
    virtual void changeMonitoingChannelAllarmingValue(const size_t channelIndex,
                                                      const float newValue) = 0;


    /// <summary>Передвинуть вверх по списку канал по индексу.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <returns>Новый индекс канала.</returns>
    virtual size_t moveUpMonitoingChannelByIndex(const size_t channelIndex) = 0;
    /// <summary>Передвинуть вниз по списку канал по индексу.</summary>
    /// <param name="channelIndex">Индекс канала в списке каналов.</param>
    /// <returns>Новый индекс канала.</returns>
    virtual size_t moveDownMonitoingChannelByIndex(const size_t channelIndex) = 0;
#pragma endregion Управление списком каналов

#pragma region Управление ботом
    // Получить настройки бота телеграма
    virtual const TelegramBotSettings& getBotSettings() = 0;
    // установить настройки бота телеграма
    virtual void setBotSettings(const TelegramBotSettings& newSettings) = 0;
#pragma endregion Управление ботом
};

// получение сервиса для мониторинга
ITrendMonitoring* get_monitoing_service();

////////////////////////////////////////////////////////////////////////////////
// конвертация интервала мониторинга в текст
inline CString monitoring_interval_to_string(const MonitoringInterval interval)
{
    switch (interval)
    {
    default:
        assert(!"Неизвестный интервал мониторинга");
        [[fallthrough]];
    case MonitoringInterval::eOneDay:
        return L"День";
    case MonitoringInterval::eOneWeek:
        return L"Неделя";
    case MonitoringInterval::eTwoWeeks:
        return L"Две недели";
    case MonitoringInterval::eOneMonth:
        return L"Месяц";
    case MonitoringInterval::eThreeMonths:
        return L"Три месяца";
    case MonitoringInterval::eHalfYear:
        return L"Пол года";
    case MonitoringInterval::eOneYear:
        return L"Год";
    }
}

//------------------------------------------------------------------------//
// конвертация интервала мониторинга в промежуток времени
inline CTimeSpan monitoring_interval_to_timespan(const MonitoringInterval interval)
{
    switch (interval)
    {
    default:
        assert(!"Неизвестный интервал мониторинга");
        [[fallthrough]];
    case MonitoringInterval::eOneDay:
        return CTimeSpan(1, 0, 0, 0);
    case MonitoringInterval::eOneWeek:
        return CTimeSpan(7, 0, 0, 0);
    case MonitoringInterval::eTwoWeeks:
        return CTimeSpan(14, 0, 0, 0);
    case MonitoringInterval::eOneMonth:
        return CTimeSpan(30, 0, 0, 0);
    case MonitoringInterval::eThreeMonths:
        return CTimeSpan(91, 0, 0, 0);
    case MonitoringInterval::eHalfYear:
        return CTimeSpan(183, 0, 0, 0);
    case MonitoringInterval::eOneYear:
        return CTimeSpan(365, 0, 0, 0);
    }
}

//------------------------------------------------------------------------//
// конвертация временного промежутка в текст
inline CString time_span_to_string(const CTimeSpan& value)
{
    CString res;

    if (auto countDays = value.GetDays(); countDays > 0)
    {
        LONGLONG countHours = (value - CTimeSpan((LONG)countDays, 0, 0, 0)).GetTotalHours();
        if (countHours == 0)
            res.Format(L"%lld дней", countDays);
        else
            res.Format(L"%lld дней %lld часов", countDays, countHours);
    }
    else if (auto countHours = value.GetTotalHours(); countHours > 0)
    {
        LONGLONG countMinutes = (value - CTimeSpan(0, (LONG)countHours, 0, 0)).GetTotalMinutes();
        if (countMinutes == 0)
            res.Format(L"%lld часов", countHours);
        else
            res.Format(L"%lld часов %lld минут", countHours, countMinutes);
    }
    else if (auto countMinutes = value.GetTotalMinutes(); countMinutes > 0)
    {
        LONGLONG countSeconds = (value - CTimeSpan(0, 0, (LONG)countMinutes, 0)).GetTotalSeconds();
        if (countSeconds == 0)
            res.Format(L"%lld минут", countMinutes);
        else
            res.Format(L"%lld минут %lld секунд", countMinutes, countSeconds);
    }
    else if (auto countSeconds = value.GetTotalSeconds(); countSeconds > 0)
        res.Format(L"%lld секунд", countSeconds);
    else
        res = L"Нет пропусков";

    assert(!res.IsEmpty());

    return res;
}

//----------------------------------------------------------------------------//
template <typename... Args>
inline void send_message_to_log(const LogMessageData::MessageType type, Args&&... textFormat)
{
    // оповещаем о возникшей ошибке
    auto logMessage = std::make_shared<LogMessageData>();
    logMessage->messageType = type;
    logMessage->logMessage.Format(textFormat...);

    get_service<CMassages>().postMessage(onNewLogMessageEvent, 0,
                                         std::static_pointer_cast<IEventData>(logMessage));
}
