// TabReports.cpp : implementation file
//

#include "stdafx.h"

#include "UI.h"
#include "TabReports.h"
#include "afxdialogex.h"

#include <include/ITrendMonitoring.h>

////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CTabReports, CDialogEx)

//----------------------------------------------------------------------------//
CTabReports::CTabReports(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_TAB_REPORTS, pParent)
    , ScopeSubscription(false)
{
}

//----------------------------------------------------------------------------//
void CTabReports::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_REPORT, m_listReports);
}

//----------------------------------------------------------------------------//
BEGIN_MESSAGE_MAP(CTabReports, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_REPORT_CLEAR, &CTabReports::OnBnClickedButtonReportClear)
    ON_BN_CLICKED(IDC_BUTTON_REPORT_REMOVE_SELECTED, &CTabReports::OnBnClickedButtonReportRemoveSelected)
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

//----------------------------------------------------------------------------//
void CTabReports::OnBnClickedButtonReportClear()
{
    m_listReports.ResetContent();
}

//----------------------------------------------------------------------------//
void CTabReports::OnBnClickedButtonReportRemoveSelected()
{
    if (m_listReports.GetCurSel() != -1)
        m_listReports.SetCurSel(m_listReports.DeleteString((UINT)m_listReports.GetCurSel()));
}

//----------------------------------------------------------------------------//
int CTabReports::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (__super::OnCreate(lpCreateStruct) == -1)
        return -1;

    ScopeSubscription::SubscribeAll();

    return 0;
}

//----------------------------------------------------------------------------//
void CTabReports::OnDestroy()
{
    __super::OnDestroy();

    // отписываемся от событий
    ScopeSubscription::UnsubscribeAll();
}

void CTabReports::OnReportDone(std::wstring messageText)
{
    CString newReportMessage;
    newReportMessage.Format(L"%s    ", CTime::GetCurrentTime().Format(L"%d.%m.%Y %H:%M:%S").GetString());

    // делаем отступ справа на каждый перенос строки чтобы выровнять по горизонтали
    CString message = messageText.c_str();
    message.Replace(L"\n", L"\n" + CString(L' ', newReportMessage.GetLength() + 16));

    newReportMessage += std::move(message);

    // Скролируем к добавленному
    m_listReports.SetTopIndex(m_listReports.AddString(newReportMessage));
}
