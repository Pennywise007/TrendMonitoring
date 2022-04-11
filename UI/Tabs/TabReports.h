#pragma once

#include "Controls/ListBox/CListBoxEx/CListBoxEx.h"

#include <include/ITrendMonitoring.h>

////////////////////////////////////////////////////////////////////////////////
// Вкладка с отчётами
class CTabReports
    : public CDialogEx
    , ext::events::ScopeSubscription<IReportEvents>
{
    DECLARE_DYNAMIC(CTabReports)

public:
    CTabReports(CWnd* pParent = nullptr);   // standard constructor
    virtual ~CTabReports() = default;

// IReportEvents
private:
    // notification when generating a report, see Message Text Data
    void OnReportDone(std::wstring messageText) override;

// Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TAB_REPORTS };
#endif

protected:
    DECLARE_MESSAGE_MAP()

    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnBnClickedButtonReportClear();
    afx_msg void OnBnClickedButtonReportRemoveSelected();

public:
    // список с отчётами
    CListBoxEx m_listReports;
};
