#pragma once
//*************************************************************************************************
class CCheckBoxedHeaderCtrl : public CHeaderCtrl
{
	DECLARE_DYNAMIC(CCheckBoxedHeaderCtrl)

public:
	CCheckBoxedHeaderCtrl();
	virtual ~CCheckBoxedHeaderCtrl();

	enum ChildrenIDs { CheckAll = 5, };
protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void OnSelectAll();
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
};
//*************************************************************************************************
class CMyButton : public CButton
{
public:
	BOOL Create(LPCTSTR lpszCaption, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) override;
public:
	UINT m_ButtonID;
public:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClicked();
};	//*********************************************************************************************
