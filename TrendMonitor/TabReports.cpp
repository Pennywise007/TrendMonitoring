// TabReports.cpp : implementation file
//

#include "stdafx.h"

#include "TrendMonitor.h"
#include "TabReports.h"
#include "afxdialogex.h"

#include "ITrendMonitoring.h"

////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CTabReports, CDialogEx)

//----------------------------------------------------------------------------//
CTabReports::CTabReports(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_TAB_REPORTS, pParent)
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

    // подписываемся на событие готовности отчёта
    EventRecipientImpl::subscribe(onReportPreparedEvent);

    return 0;
}

//----------------------------------------------------------------------------//
void CTabReports::OnDestroy()
{
    __super::OnDestroy();

    // отписываемся от событий
    EventRecipientImpl::subscribe(onReportPreparedEvent);
}

//----------------------------------------------------------------------------//
// IEventRecipient
void CTabReports::onEvent(const EventId& code, float eventValue,
                          std::shared_ptr<IEventData> eventData)
{
    if (code == onReportPreparedEvent)
    {
        std::shared_ptr<MessageTextData> reportMessageData = std::static_pointer_cast<MessageTextData>(eventData);

        CString newReportMessage;
        newReportMessage.Format(L"%s    ", CTime::GetCurrentTime().Format(L"%d.%m.%Y %H:%M:%S").GetString());

        // делаем отступ справа на каждый перенос строки чтобы выровнять по горизонтали
        reportMessageData->messageText.Replace(L"\n",  L"\n" + CString(L' ', newReportMessage.GetLength() + 16));

        newReportMessage += reportMessageData->messageText;

        // Скролируем к добавленному
        m_listReports.SetTopIndex(m_listReports.AddString(newReportMessage));
    }
    else
        assert(!"Возникло не обработанное событие на которое подписались");
}
