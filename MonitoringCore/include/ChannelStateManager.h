#pragma once

#include <afx.h>
#include <afxwin.h>
#include <cassert>
#include <map>
#include <optional>

constexpr LONGLONG kCountOfHoursForIgnoringSimilarError = 3;

// TODO ADD TESTS
struct ChannelStateManager
{
    // Состояние канала
    enum ReportErrors
    {
        eFallenOff,         // произошло оповещение пользователей об отваливании датчика
        eExcessOfValue,     // произошло оповещение пользователей о превышении допустимого значения
        eLotOfEmptyData     // произошло оповещение пользователей о большом количестве пропусков
    };

    void OnAddChannelErrorReport(const ReportErrors error, CString& errorMessage, LPCWSTR newErrorMessageFormat, ...)
    {
        auto it = channelState.find(error);
        if (it == channelState.end())
        {
            switch (error)
            {
            case ReportErrors::eFallenOff:
                it = channelState.emplace(error, ErrorInfo(3, 1)).first;
                break;
            case ReportErrors::eExcessOfValue:
            case ReportErrors::eLotOfEmptyData:
                it = channelState.emplace(error, 3).first;
                break;
            default:
                assert(!"Не опознанный тип репорта");
                return;
            }
        }

        va_list argList;
        va_start(argList, newErrorMessageFormat);
        it->second.OnAddError(errorMessage, newErrorMessageFormat, argList);
        va_end(argList);
    }

    // true - если надо оповестить о новом состоянии
    void OnRemoveChannelErrorReport(const ReportErrors error, CString& errorMessage, LPCWSTR newErrorMessageFormat, ...)
    {
        auto it = channelState.find(error);
        if (it == channelState.end())
            return;

        va_list argList;
        va_start(argList, newErrorMessageFormat);
        if (it->second.OnRemove(errorMessage, newErrorMessageFormat, argList))
            channelState.erase(it);
        va_end(argList);
    }

    // true - если надо оповестить о новом состоянии
    void OnRemoveChannelErrorReport(const ReportErrors error)
    {
        if (auto it = channelState.find(error); it != channelState.end() && it->second.OnRemove())
            channelState.erase(it);
    }

    bool dataLoaded = false;
    bool loadingDataError = false;

    bool operator==(const ChannelStateManager& other) const
    {
        auto wrap_fields = [](const ChannelStateManager& manager)
        {
            return std::tie(manager.dataLoaded, manager.loadingDataError, manager.channelState);
        };

        return wrap_fields(*this) == wrap_fields(other);
    }

private:
    struct ErrorInfo
    {
        explicit ErrorInfo(const size_t _countOfIgnoringErrors,
                           const size_t _countOfSuccessBeforeDeleteError)
            : m_countOfIgnoringErrors(_countOfIgnoringErrors)
            , m_countOfSuccessBeforeDeleteError(_countOfSuccessBeforeDeleteError)
        {}

        explicit ErrorInfo(const size_t _countOfSuccessToDeleteState)
            : m_countOfSuccessBeforeDeleteError(_countOfSuccessToDeleteState)
        {}

        void OnAddError(CString& errorMessage, LPCWSTR newErrorFormat, const va_list& argList)
        {
            if (m_countOfIgnoringErrors > 0 && m_countOfIgnoringErrors-- != 0)
                return;

            const auto curTime = CTime::GetCurrentTime();
            if (!m_timeOfLastReporting.has_value())
            {
                if (!errorMessage.IsEmpty())
                    errorMessage.AppendChar(L' ');

                m_timeOfLastReporting = std::move(curTime);
                errorMessage.AppendFormatV(newErrorFormat, argList);
            }
            else if ((curTime - m_timeOfLastReporting.value()).GetTotalHours() > kCountOfHoursForIgnoringSimilarError)
            {
                if (!errorMessage.IsEmpty())
                    errorMessage.AppendChar(L' ');

                errorMessage.AppendFormat(L"В течениe %lld часов наблюдается ошибка: ", (curTime - m_timeOfFirstError).GetTotalHours());
                m_timeOfLastReporting = std::move(curTime);
                errorMessage.AppendFormatV(newErrorFormat, argList);
            }
        }

        // true если пользователя можно оповестить что проблема решена и удалить информацию об ошибке
        bool OnRemove()
        {
            // if m_countOfIgnoringErrors was setted
            if (!m_timeOfLastReporting.has_value())
                return true;

            assert(m_currentCountOfDeletingState + 1 > m_countOfSuccessBeforeDeleteError);
            return ++m_currentCountOfDeletingState == m_countOfSuccessBeforeDeleteError;
        }

        bool OnRemove(CString& errorMessage, LPCWSTR newErrorFormat, const va_list& argList)
        {
            const auto res = OnRemove();
            if (res)
            {
                if (!errorMessage.IsEmpty())
                    errorMessage.AppendChar(L' ');
                errorMessage.AppendFormatV(newErrorFormat, argList);
            }
            return res;
        }

        bool operator==(const ErrorInfo& other) const
        {
            auto wrap_fields = [](const ErrorInfo& info)
            {
                return std::tie(info.m_countOfIgnoringErrors, info.m_countOfSuccessBeforeDeleteError, info.m_currentCountOfDeletingState);
            };

            return wrap_fields(*this) == wrap_fields(other);
        }
    private:
        CTime m_timeOfFirstError = CTime::GetCurrentTime();
        std::optional<CTime> m_timeOfLastReporting;

        // некоторые сообщения будем фильтровать
        size_t m_countOfIgnoringErrors = 0;
        // некоторые состояния могут стрелять часто, например о превышении уровня, не даём спамить ими
        size_t m_countOfSuccessBeforeDeleteError = 1;
        size_t m_currentCountOfDeletingState = 0;
    };

    std::map<ReportErrors, ErrorInfo> channelState;
};
