#pragma once

#include <afxcmn.h>
#include <map>
#include <memory>

////////////////////////////////////////////////////////////////////////////////
// Расширение для контрола с табами позволяет вставлять диалоги во вкладки
// и переключаться между ними / раставлять их на места и сайзировать вместе с контролом
class CTabControl : public CTabCtrl
{
public:
    CTabControl() = default;

public:
    /// Вставка таба с окном привязанным к нему, при переключении вкладок
    /// будет включаться окно привязанное к вкладке.
    ///
    /// Внимание! Вставлять необходимо по очереди (по возрастанию индексов)
    /// Вставка между существующими табами может привести к проблемам(не реализована)
    LONG InsertTab(_In_ int nItem, _In_z_ LPCTSTR lpszItem,
                   _In_ std::shared_ptr<CWnd> tabWindow);

    /// Вставка диалога в таб, если у диалога нет окна - будет вызван
    /// CDialog::Create с заданным идентификатором nIDTemplate и родителем будет проставлен таб контрол
    ///
    /// Внимание! Вставлять необходимо по очереди (по возрастанию индексов)
    /// Вставка между существующими табами может привести к проблемам(не реализована)
    LONG InsertTab(_In_ int nItem, _In_z_ LPCTSTR lpszItem,
                   _In_ std::shared_ptr<CDialog> tabDialog, UINT nIDTemplate);

public:
    DECLARE_MESSAGE_MAP()

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTcnSelchange(NMHDR *pNMHDR, LRESULT *pResult);

private:
    // располагает текущее активное окно в клиентской области таба
    void layoutCurrentWindow();

private:
    // диалоги для каждой вкладки
    std::map<LONG, std::shared_ptr<CWnd>> m_tabWindows;
};