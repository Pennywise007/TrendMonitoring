/**
* @file   Logger.h
* @author Швец С.
* @short  Реализация логирования.
*/

#pragma once

// для отработки логирования необходимо включить его через #define OUTPUT_LOGGING

#ifdef OUTPUT_LOGGING

#include <afx.h>
#include <afxstr.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdio.h>
#include <Windows.h>
#include <WinBase.h>
#include <corecrt.h>

namespace custom_logger
{
    // глобальная переменная которая отвечает за включение логирования в текущий момент
    inline std::atomic_bool g_enableLogger = true;
    // текущий уровень на котором идет логирование, нужно чтобы сдвигать дебаг внутри вызова функций
    inline std::atomic_int g_loggerLevel = 0;

    class Logging
    {
    public:
        // funcName - имя функции которую логируем
        // logEnter - флаг что нужно оповестить о заходе в функцию
        Logging(const CString& funcName, bool logEnter)
            : m_funcName(funcName)
        {
            m_currentLoggingLevel = g_loggerLevel++;

            // если это шаблон - вытаскиваем из него лишний текст между <>
            auto start = m_funcName.Find(u'<');
            auto end = m_funcName.Find(u'>');
            if (start != -1 && end != -1 && start < end)
                m_funcName = m_funcName.Mid(start) +
                    m_funcName.Mid(end + 1, m_funcName.GetLength() - end - 1);

            startLogging = std::chrono::system_clock::now();
            if (logEnter)
                OutputLoggerText(m_funcName + L" Вход");
        }

        ~Logging()
        {
            CString debugText;  // текст выводимый в дебаг
            debugText.Format(L"%sВыход; %s",
                             (m_funcName.IsEmpty() ? m_funcName : (m_funcName + L" ")).GetString(),
                             (m_comment.IsEmpty()  ? m_comment : (m_comment + L"; ")).GetString());

            // если надо логировать время выполнения
            if (m_bEnableCalcTime)
            {
                auto finish = std::chrono::system_clock::now();
                long long elapsed_seconds =
                    std::chrono::duration_cast<std::chrono::milliseconds>(finish - startLogging).count();

                debugText.Format(L"%s; Время выполнения %lld мс",
                                 debugText.GetString(),
                                 elapsed_seconds);
            }

            OutputLoggerText(debugText);

            --g_loggerLevel;
        }

        // добавление комментария к логгеру
        template <typename ...Args>
        void addComment(const CString& format, Args&&... args)
        {
            if (!m_comment.IsEmpty())
                m_comment += L";\t";

            m_comment.AppendFormat(format, args...);
        }

        // установка комментария
        template <typename ...Args>
        void setText(const CString& format, Args&&... args)
        {
            m_comment.Format(format, args...);
        }

        // вывод текста на текущем уровне, не влияет на внутренний текст класса
        template <typename ...Args>
        void outputText(const CString& format, Args&&... args)
        {
            CString debugText;  // текст выводимый в дебаг
            debugText.Format(format, args...);

            OutputLoggerText(debugText);
        }

        // вывести с дебаг текущую информацию
        void outputDebug()
        {
            CString debugText;  // текст выводимый в дебаг

            if (!m_funcName.IsEmpty())
                debugText = m_funcName + L"; ";
            debugText += m_comment;
            OutputLoggerText(debugText);
        }

        // отключение логирования
        void disableLog()
        {
            m_bEnableLog = false;
        }

        // включить рассчет времени вычисления
        void enableCalcTime(bool enable)
        {
            m_bEnableCalcTime = enable;
        }


    private:
        // конечная функция для вывода дебажной информации
        void OutputLoggerText(CString str)
        {
            if (!g_enableLogger || !m_bEnableLog)
                return;

            CString extraText;
            // добавляем уровень на который сейчас отступет наш текст
            for (int curLevel = m_currentLoggingLevel; curLevel != 0; --curLevel)
                extraText += L"|\t";

            // к переносам тоже добавляем уровень
            str.Replace(L"\n", L"\n" + extraText);

            OutputDebugString((extraText + str + L'\n').GetString());
        }

    private:
        CString m_funcName;   // имя функции
        CString m_comment;    // комментарий к логу
        std::chrono::system_clock::time_point startLogging; // время начала логирования
        bool m_bEnableLog = true;           // флаг возможности логирования
        bool m_bEnableCalcTime = false;     // флаг необходимости логирования времени выполнения

        int m_currentLoggingLevel;  // уровень на котором отображается текст в логе
    };

} // custom_logger

// создание логгера с прокидыванием имени функции
#define OUTPUT_LOG_FUNC std::shared_ptr<custom_logger::Logging> outputLogger = custom_logger::g_enableLogger ? \
                                        std::make_shared<custom_logger::Logging>(CString(__func__), false) : nullptr;

// создание логгера с прокидыванием имени функции и логированием что в функцию вошли
#define OUTPUT_LOG_FUNC_ENTER  std::shared_ptr<custom_logger::Logging> outputLogger = custom_logger::g_enableLogger ? \
                                        std::make_shared<custom_logger::Logging>(CString(__func__), true) : nullptr;

// добавление комментария к функции
#define OUTPUT_LOG_ADD_COMMENT(...) if (outputLogger) outputLogger->addComment(__VA_ARGS__);

// установка комментария к функции
#define OUTPUT_LOG_SET_TEXT(...) if (outputLogger) outputLogger->setText(__VA_ARGS__);

// вывод текста в лог по текущему уровню логгера
#define OUTPUT_LOG_TEXT(...) if (outputLogger) outputLogger->outputText(__VA_ARGS__);

// отключение логирования
#define OUTPUT_LOG_DISABLE  if (outputLogger) outputLogger->disableLog();

// включение логирования времени выполнения
#define OUTPUT_LOG_CALCTIME_ENABLE(enable) if (outputLogger) outputLogger->enableCalcTime(enable);

// немедленное логирование текущего содержимого
#define OUTPUT_LOG_DO  if (outputLogger) outputLogger->outputDebug();

// включить лоигрование
#define OUTPUT_LOG_ENABLELOGGING(expression)  custom_logger::g_enableLogger = !!(expression);

// остановить отладку в случае срабатывания выражения
#define OUTPUT_LOG_DEBUGBREAK(expression)  if (!!(expression) && custom_logger::g_enableLogger) ::DebugBreak();

#else   // OUTPUT_LOGGING

#define OUTPUT_LOG_FUNC ;
#define OUTPUT_LOG_FUNC_ENTER ;
#define OUTPUT_LOG_ADD_COMMENT(...) ;
#define OUTPUT_LOG_SET_TEXT(...) ;
#define OUTPUT_LOG_TEXT(...) ;
#define OUTPUT_LOG_DISABLE ;
#define OUTPUT_LOG_CALCTIME_ENABLE(enable) ;
#define OUTPUT_LOG_DO ;
#define OUTPUT_LOG_ENABLELOGGING(expression) ;
#define OUTPUT_LOG_DEBUGBREAK(expression) ;
#endif
