#include "stdafx.h"
#include "CheckBoxedHeaderCtrl.h"

IMPLEMENT_DYNAMIC(CCheckBoxedHeaderCtrl, CHeaderCtrl)

CCheckBoxedHeaderCtrl::CCheckBoxedHeaderCtrl()
{
}

CCheckBoxedHeaderCtrl::~CCheckBoxedHeaderCtrl()
{
}


BEGIN_MESSAGE_MAP(CCheckBoxedHeaderCtrl, CHeaderCtrl)
	ON_BN_CLICKED(CheckAll, OnSelectAll)
	ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

void CCheckBoxedHeaderCtrl::OnSelectAll()
{
	GetParent()->PostMessage((WM_USER + 100));
}

void CCheckBoxedHeaderCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	HD_HITTESTINFO hd_hittestinfo;

	hd_hittestinfo.pt = point;
	SendMessage(HDM_HITTEST, 0, (LPARAM)(&hd_hittestinfo));

	if (hd_hittestinfo.flags == HHT_ONHEADER)
		GetParent()->PostMessage((WM_USER + 102), 0, hd_hittestinfo.iItem);

	CHeaderCtrl::OnLButtonUp(nFlags, point);
}

BEGIN_MESSAGE_MAP(CMyButton, CButton)
	ON_CONTROL_REFLECT(BN_CLICKED, &CMyButton::OnBnClicked)
END_MESSAGE_MAP()


void CMyButton::OnBnClicked()
{
	GetParent()->GetParent()->PostMessage((WM_USER + 101), m_ButtonID);
}

BOOL CMyButton::Create(LPCTSTR lpszCaption, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	m_ButtonID = nID;
	return CButton::Create(lpszCaption, dwStyle, rect, pParentWnd, nID);
}
