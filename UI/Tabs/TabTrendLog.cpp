// CTabTrendLog.cpp : implementation file
//

#include "stdafx.h"

#include "UI.h"
#include "TabTrendLog.h"
#include "afxdialogex.h"

#include <Controls/Layout/Layout.h>

#include <include/ITrendMonitoring.h>

////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CTabTrendLog, CDialogEx)

//----------------------------------------------------------------------------//
CTabTrendLog::CTabTrendLog(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_TAB_EVENTS_LOG, pParent)
    , ScopeSubscription(false)
{
}

//----------------------------------------------------------------------------//
void CTabTrendLog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_LOG, m_listLog);
}

//----------------------------------------------------------------------------//
BEGIN_MESSAGE_MAP(CTabTrendLog, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_LOG_CLEAR, &CTabTrendLog::OnBnClickedButtonLogClear)
    ON_BN_CLICKED(IDC_BUTTON_LOG_REMOVE_ERRORS, &CTabTrendLog::OnBnClickedButtonLogRemoveErrors)
    ON_WM_CREATE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

//----------------------------------------------------------------------------//
void CTabTrendLog::OnBnClickedButtonLogClear()
{
    m_listLog.ResetContent();
    m_errorsInLogIndexes.clear();
}

//----------------------------------------------------------------------------//
void CTabTrendLog::OnBnClickedButtonLogRemoveErrors()
{
    for (auto it = m_errorsInLogIndexes.rbegin(), end = m_errorsInLogIndexes.rend(); it != end; ++it)
    {
        m_listLog.DeleteString(*it);
    }

    m_errorsInLogIndexes.clear();
}

//----------------------------------------------------------------------------//
int CTabTrendLog::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (__super::OnCreate(lpCreateStruct) == -1)
        return -1;

    ScopeSubscription::SubscribeAll();

    LayoutLoader::ApplyLayoutFromResource(*this, m_lpszTemplateName);

    return 0;
}

//----------------------------------------------------------------------------//
void CTabTrendLog::OnDestroy()
{
    __super::OnDestroy();

    // отписываемся от событий
    ScopeSubscription::UnsubscribeAll();
}

void CTabTrendLog::OnNewLogMessage(const std::shared_ptr<LogMessageData>& logMessageData)
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

