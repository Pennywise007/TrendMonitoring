﻿
// TrendMonitorDlg.h : header file
//

#pragma once

#include "Controls/Tables/ListGroupCtrl/ListGroupCtrl.h"
#include "Controls/Tables/ListEditSubItems/ListEditSubItems.h"
#include "Controls/TabControl/TabControl.h"
#include "Controls/Splitter/Splitter.h"

#include <include/IDirService.h>
#include <include/ITrendMonitoring.h>

#include <ext/core/dependency_injection.h>

#include "TrayHelper.h"

// MainDlg dialog
class MainDlg
    : public CDialogEx
    , ext::events::ScopeSubscription<IMonitoringListEvents, ILogEvents, IReportEvents>
{
// Construction
public:
    MainDlg(std::shared_ptr<ITrendMonitoring>&& trendMonitoring,
            std::shared_ptr<IDirService>&& dirServide,
            CWnd* pParent = nullptr);	// standard constructor

// IMonitoringListEvents
private:
    void OnChanged() override;

// ILogEvents
private:
    void OnNewLogMessage(const std::shared_ptr<LogMessageData>& logMessage) override;

// IReportEvents
private:
    // notification when generating a report, see Message Text Data
    void OnReportDone(std::wstring messageText) override;

// Dialog Data
protected:
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TRENDMONITOR_DIALOG };
#endif

    HICON m_hIcon;

    DECLARE_MESSAGE_MAP()
    // оконные сообщения
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
    virtual void OnOK();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnPaint();
    afx_msg void OnClose();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
    // управление списком каналов
    afx_msg void OnBnClickedMfcbuttonAdd();
    afx_msg void OnBnClickedMfcbuttonRemove();
    afx_msg void OnBnClickedMfcbuttonMoveUp();
    afx_msg void OnBnClickedMfcbuttonMoveDown();
    afx_msg void OnBnClickedMfcbuttonRefresh();
    // показ трендов по каналам
    afx_msg void OnBnClickedMfcbuttonShowTrends();
    // сообщения от списка каналов
    afx_msg void OnNMCustomdrawListMonitoringChannels(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnItemchangedListMonitoringChannels(NMHDR *pNMHDR, LRESULT *pResult);

private:
    // инициализация контролов
    void initControls();
    // обработать параметры командной строки
    void processCommandLine();
    // перезагрузка списка каналов из сервиса
    void reloadChannelsList();
    // выделить элемент в списке каналов по индексу
    void selectChannelsListItem(const size_t index);
    // добавление иконки в трей
    void addTrayIcon();
    // показать нотификацию в трее
    void showTrayNotification(const CString& title,
                              const CString& descr,
                              DWORD dwBubbleFlags /*= NIIF_WARNING*/,
                              const CTrayHelper::OnUserClick& onUserClick = nullptr);
    // функция восстановления видимости диалога
    void restoreDlg();

// контролы
private:
    // таблица с каналами и информацией о них
    CListEditSubItems<CListGroupCtrl> m_monitorChannelsList;
    // контрол с переключаемыми вкладками
    CTabControl m_tabCtrl;
    // вспомогательный класс для показа иконки в трее
    CTrayHelper m_trayHelper;
    // Splitter between table and tabs
    CSplitter m_splitter;

private:
    // фдаг необходимости спрятать диалог
    bool m_bHiddenDialog = false;
    // флаг обновления таблицы
    bool m_bUpdatingTable = false;
public:
    // Колонки таблицы с каналами для мониторинга
    enum TableColumns
    {
        eNotify,                // Оповещатья
        eChannelName,           // название канала
        eInterval,              // интервал мониторинга
        eAlarmingValue,         // значение, достигнув которое необходимо оповестить

        // начало списка колонок с данными мониторинга
        eStartDataColumns,

        eLastDataExistTime = eStartDataColumns, // время последних существующих данных
        eNoDataTime,                            // время без данных
        eStartValue,                            // значение в начале интервала
        eCurrentValue,                          // текущее значение
        eMinValue,                              // минимальное значение за интервал
        eMaxValue,                              // максимальное значение за интервал

        // конец списка колонок с данными мониторинга
        eEndDataColumns = eMaxValue,

        // счетчик колонок
        eColumnsCount
    };

    // Индексы вкладок
    enum TabIndexes
    {
        eTabLog,        // вкладка с логом событий
        eTabReport      // таб с отчетом
    };

private: // dependencies
    std::shared_ptr<ITrendMonitoring> m_trendMonitoring;
    std::shared_ptr<IDirService> m_dirServide;
};
