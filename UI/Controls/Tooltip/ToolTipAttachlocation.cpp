#include "ToolTip.h"

IMPLEMENT_DYNCREATE(CToolTipAttachLocation, CMFCToolTipCtrl)

BEGIN_MESSAGE_MAP(CToolTipAttachLocation, CMFCToolTipCtrl)
    ON_NOTIFY_REFLECT(TTN_POP, &CToolTipAttachLocation::OnPop)
    ON_WM_WINDOWPOSCHANGING()
END_MESSAGE_MAP()

CToolTipAttachLocation::CToolTipAttachLocation(CMFCToolTipInfo* pParams/* = NULL*/)
    : CMFCToolTipCtrl(pParams)
{}

void CToolTipAttachLocation::ApplyAttachLocation()
{
    // force OnWindowPosChanging
    SetWindowPos(nullptr, NULL, NULL, NULL, NULL, SWP_NOACTIVATE | SWP_NOSIZE | SWP_DRAWFRAME | SWP_NOZORDER | SWP_NOREDRAW);
}

void CToolTipAttachLocation::OnPop(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    // avoid resetting all parameters in CMFCToolTipCtrl::OnPop
    *pResult = 0;
}

void CToolTipAttachLocation::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
    if (!(lpwndpos->flags & SWP_NOMOVE) && m_ptLocation != CPoint(-1, -1))
    {
        CSize controlSize;
        if (IsWindow(m_hWnd))
        {
            CRect rect;
            GetWindowRect(rect);
            controlSize = rect.Size();
        }
        else
        {
            ASSERT(TRUE);
            if (!(lpwndpos->flags & SWP_NOSIZE))
                controlSize = { lpwndpos->cx, lpwndpos->cy + 1 };
        }

        lpwndpos->x = m_ptLocation.x;
        lpwndpos->y = m_ptLocation.y;
        switch (m_attachLocation)
        {
        case AttachLocation::eTopLeft:
            break;
        case AttachLocation::eTopMiddle:
            lpwndpos->x -= controlSize.cx / 2;
            break;
        case AttachLocation::eTopRight:
            lpwndpos->x -= controlSize.cx;
            break;
        case AttachLocation::eBottomLeft:
            lpwndpos->y -= controlSize.cy;
            break;
        case AttachLocation::eBottomMiddle:
            lpwndpos->y -= controlSize.cy;
            lpwndpos->x -= controlSize.cx / 2;
            break;
        case AttachLocation::eBottomRight:
            lpwndpos->y -= controlSize.cy;
            lpwndpos->x -= controlSize.cx;
            break;
        default:
            ASSERT(TRUE);
            break;
        }
    }
    CMFCToolTipCtrl::OnWindowPosChanging(lpwndpos);
}
