#include <assert.h>

#include "TabControl.h"

////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CTabControl, CTabCtrl)
    ON_WM_SIZE()
    ON_NOTIFY_REFLECT(TCN_SELCHANGE, &CTabControl::OnTcnSelchange)
END_MESSAGE_MAP()

//----------------------------------------------------------------------------//
LONG CTabControl::InsertTab(_In_ int nItem, _In_z_ LPCTSTR lpszItem,
                            _In_ std::shared_ptr<CWnd> tabWindow)
{
    // вставляем элемент
    LONG insertedIndex = CTabCtrl::InsertItem(nItem, lpszItem);

    if (m_tabWindows.find(insertedIndex) != m_tabWindows.end())
        assert(!"Уже существует окно на этой вкладке, возможно их надо удалить!");

    m_tabWindows[insertedIndex] = tabWindow;

    // Если вставленная вкладка стала текущей
    if (CTabCtrl::GetCurSel() == insertedIndex)
    {
        tabWindow->ShowWindow(SW_SHOW);
        tabWindow->SetFocus();

        layoutCurrentWindow();
    }

    return insertedIndex;
}

//----------------------------------------------------------------------------//
LONG CTabControl::InsertTab(_In_ int nItem, _In_z_ LPCTSTR lpszItem,
                            _In_ std::shared_ptr<CDialog> tabDialog, UINT nIDTemplate)
{
    // вставляем элемент
    LONG insertedIndex = CTabCtrl::InsertItem(nItem, lpszItem);

    if (m_tabWindows.find(insertedIndex) != m_tabWindows.end())
        assert(!"Уже существует окно на этой вкладке, возможно их надо удалить!");

    // если у диалога нет окна создаем его как наш дочерний диалог
    if (!::IsWindow(tabDialog->m_hWnd))
        tabDialog->Create(nIDTemplate, this);

    m_tabWindows[insertedIndex] = tabDialog;

    // Если вставленная вкладка стала текущей
    if (CTabCtrl::GetCurSel() == insertedIndex)
    {
        tabDialog->ShowWindow(SW_SHOW);
        tabDialog->SetFocus();

        layoutCurrentWindow();
    }

    return insertedIndex;
}

//----------------------------------------------------------------------------//
void CTabControl::OnSize(UINT nType, int cx, int cy)
{
    CTabCtrl::OnSize(nType, cx, cy);

    layoutCurrentWindow();
}

//----------------------------------------------------------------------------//
void CTabControl::OnTcnSelchange(NMHDR *pNMHDR, LRESULT *pResult)
{
    int curSel = CTabCtrl::GetCurSel();
    assert(curSel != -1);

    // Переключаем активную вкладку
    for (auto& windowTab : m_tabWindows)
    {
        if (windowTab.first == curSel)
        {
            windowTab.second->ShowWindow(SW_SHOW);
            windowTab.second->SetFocus();

            layoutCurrentWindow();
        }
        else
            windowTab.second->ShowWindow(SW_HIDE);
    }

    *pResult = 0;
}

//----------------------------------------------------------------------------//
void CTabControl::layoutCurrentWindow()
{
    int curSel = CTabCtrl::GetCurSel();

    auto currentWindow = m_tabWindows.find(curSel);
    if (currentWindow == m_tabWindows.end())
        return;

    // рассчитываем позицию где должен быть диалог
    CRect lpRect;
    CTabCtrl::GetItemRect(curSel, &lpRect);
    CRect clRc;
    CTabCtrl::GetClientRect(&clRc);
    clRc.top += lpRect.bottom;

    clRc.InflateRect(-2, -2);

    currentWindow->second->MoveWindow(&clRc, TRUE);
}
