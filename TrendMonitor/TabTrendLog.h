#pragma once

#include "Controls/ListBox/CListBoxEx/CListBoxEx.h"

#include <include/ITrendMonitoring.h>

class СTabTrendLog
    : public CDialogEx
    , ext::events::ScopeSubscription<ILogEvents>
{
    DECLARE_DYNAMIC(СTabTrendLog)

public:
    СTabTrendLog(CWnd* pParent = nullptr);   // standard constructor
    virtual ~СTabTrendLog() = default;

// ILogEvents
private:
    void OnNewLogMessage(const std::shared_ptr<LogMessageData>& logMessage) override;

// Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TAB_EVENTS_LOG };
#endif

protected:
    DECLARE_MESSAGE_MAP()

    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnBnClickedButtonLogClear();
    afx_msg void OnBnClickedButtonLogRemoveErrors();

public:
    // Список с событиями
    CListBoxEx m_listLog;

    // список индексов элементов с ошибками в логе
    std::set<int> m_errorsInLogIndexes;
};
