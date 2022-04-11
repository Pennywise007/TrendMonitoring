#pragma once

#include <afx.h>
#include <afxtooltipctrl.h>
#include <WinUser.h>

class CToolTipAttachLocation : public CMFCToolTipCtrl
{
    DECLARE_DYNCREATE(CToolTipCtrlEx)

public:
    CToolTipAttachLocation(CMFCToolTipInfo* pParams = NULL);

    /*
        The side of which the control is attached to the point specified in CMFCToolTipCtrl::SetLocation

        0*******1*******2
        *               *
        3*******4*******5
    */
    enum class AttachLocation
    {
        eTopLeft = 0,
        eTopMiddle,
        eTopRight,
        eBottomLeft,
        eBottomMiddle,
        eBottomRight
    } m_attachLocation = AttachLocation::eTopLeft;

    // apply attach location, force set window pos to validate control rect
    void ApplyAttachLocation();

protected:
    afx_msg void OnPop(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);

    DECLARE_MESSAGE_MAP()
};