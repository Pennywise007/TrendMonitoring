#pragma once

#include <afx.h>
#include <afxtooltipctrl.h>
#include <afxribbonbutton.h>
#include <vcruntime.h>
#include <Windows.h>

/*
* Extended tool window for showing tips, based on CMFCToolTipCtrl, see atlmfc\src\mfc\afxtooltipctrl.cpp
* Usage:
* m_tooltip.Create(GetSafeHwnd(), true);
* m_tooltip.SetLabel(L"Text");
*
* const CSize windowSize = m_tooltip.GetWindowSize();
* CRect toolWindowRect = { CPoint(0,0), windowSize};
* m_tooltip.SetWindowPos(NULL, toolWindowRect.left, toolWindowRect.top, toolWindowRect.Width(), toolWindowRect.Height(),
                         SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);

// for hide
* m_tooltip.ShowWindow(SW_HIDE);
*/
class CToolWindow : public CWnd
{
    DECLARE_DYNCREATE(CToolWindow)

    // proxy class for getting protected info from CMFCToolTipCtrl
    struct CMFCToolTipCtrlProxy : CMFCToolTipCtrl
    {
        CMFCToolTipCtrlProxy(CMFCToolTipInfo* pParams)
            : CMFCToolTipCtrl(pParams)
            , m_description(m_strDescription)
            , m_imageSize(m_sizeImage)
        {}

        void GetHotButton() { CMFCToolTipCtrl::GetHotButton(); }
        int GetFixedWidth() { return CMFCToolTipCtrl::GetFixedWidth(); }
        int RibbonButtonExists() const { return m_pRibbonButton != NULL; }

        const CString& m_description;
        CSize& m_imageSize;
    };

public:
    CToolWindow(CMFCToolTipInfo* pParams = NULL);
    ~CToolWindow() override;

    // create window
    BOOL Create(HWND parent, bool topMost);

    // main tool text
    void SetLabel(const CString& label);
    // set description for label
    void SetDescription(const CString& strDescription, bool redraw = true);

    // setting control settings
    void SetParams(CMFCToolTipInfo* pParams, bool redraw = true);
    _NODISCARD const CMFCToolTipInfo& GetParams() const;
    void SetHotRibbonButton(CMFCRibbonButton* pRibbonButton, bool redraw = true);

    // getting size of CMFCRibbonButton
    _NODISCARD CSize GetIconSize();
    // get window size with all window content
    _NODISCARD CSize GetWindowSize();

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    // copy of CMFCToolTipCtrl::OnPaint
    afx_msg void OnPaint();

    DECLARE_MESSAGE_MAP()

private:
    // copy of CMFCToolTipCtrl::OnDrawLabel, just to avoid problems with CMFCToolTipCtrl::GetWindowsText
    CSize OnDrawLabel(CDC* pDC, CRect rect, BOOL bCalcOnly);

private:
    // proxy class for drawing tool control
    CMFCToolTipCtrlProxy m_drawProxy;
    // current window main label, @see SetLabel
    CString m_label;
    // constant margin, see CMFCToolTipCtrl::OnShow
    const CPoint m_ptMargin = { 6, 4 };
};
