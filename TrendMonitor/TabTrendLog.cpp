// СTabTrendLog.cpp : implementation file
//

#include "stdafx.h"

#include "TrendMonitor.h"
#include "TabTrendLog.h"
#include "afxdialogex.h"

#include <include/ITrendMonitoring.h>

////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(СTabTrendLog, CDialogEx)

//----------------------------------------------------------------------------//
СTabTrendLog::СTabTrendLog(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_TAB_EVENTS_LOG, pParent)
    , ScopeSubscription(false)
{
}

//----------------------------------------------------------------------------//
void СTabTrendLog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_LOG, m_listLog);
}

//----------------------------------------------------------------------------//
BEGIN_MESSAGE_MAP(СTabTrendLog, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_LOG_CLEAR, &СTabTrendLog::OnBnClickedButtonLogClear)
    ON_BN_CLICKED(IDC_BUTTON_LOG_REMOVE_ERRORS, &СTabTrendLog::OnBnClickedButtonLogRemoveErrors)
    ON_WM_CREATE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

//----------------------------------------------------------------------------//
void СTabTrendLog::OnBnClickedButtonLogClear()
{
    m_listLog.ResetContent();
    m_errorsInLogIndexes.clear();
}

//----------------------------------------------------------------------------//
void СTabTrendLog::OnBnClickedButtonLogRemoveErrors()
{
    for (auto it = m_errorsInLogIndexes.rbegin(), end = m_errorsInLogIndexes.rend(); it != end; ++it)
    {
        m_listLog.DeleteString(*it);
    }

    m_errorsInLogIndexes.clear();
}

//----------------------------------------------------------------------------//
int СTabTrendLog::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (__super::OnCreate(lpCreateStruct) == -1)
        return -1;

    ScopeSubscription::SubscribeAll();

    return 0;
}

//----------------------------------------------------------------------------//
void СTabTrendLog::OnDestroy()
{
    __super::OnDestroy();

    // отписываемся от событий
    ScopeSubscription::UnsubscribeAll();
}

void СTabTrendLog::OnNewLogMessage(const std::shared_ptr<LogMessageData>& logMessageData)
{
    CString newLogMessage;
    newLogMessage.Format(L"%s    ", CTime::GetCurrentTime().Format(L"%d.%m.%Y %H:%M:%S").GetString());

    // делаем отступ справа на каждый перенос строки чтобы выровнять по горизонтали
    CString message = logMessageData->logMessage.c_str();
    message.Replace(L"\n", L"\n" + CString(L' ', newLogMessage.GetLength() + 16));

    newLogMessage += std::move(message);

    int newItemIndex;
    switch (logMessageData->messageType)
    {
    case LogMessageData::MessageType::eError:
        newItemIndex = m_listLog.AddItem(newLogMessage, RGB(255, 128, 128));
        m_errorsInLogIndexes.emplace(newItemIndex);
        break;
    default:
        EXT_ASSERT(!"Неизвестный тип сообщения");
        [[fallthrough]];
    case LogMessageData::MessageType::eOrdinary:
        newItemIndex = m_listLog.AddString(newLogMessage);
        break;
    }

    // Скролируем к добавленному
    m_listLog.SetTopIndex(newItemIndex);
}

