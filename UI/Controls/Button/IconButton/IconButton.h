#pragma once
#include "afxwin.h"
//*************************************************************************************************
#define USE_ICON_SIZE	-1
//*************************************************************************************************
// ������������ ����� �������� ������ �� ��������
enum Aligments
{
	LeftCenter = 0,	// ����� �� ������
	CenterTop,		// �� ������ ������
	CenterCenter,	// �� ������
	CenterBottom,	// �� ������ �����
	RightCenter		// ������ �� ������
};
//*************************************************************************************************
class CIconButton :	public CButton
{
public:	//*****************************************************************************************
	CIconButton();
	~CIconButton();
public:	//*****************************************************************************************
	// �������� ����������� ��� ������, Aligments - ���� �������� ����������� ������������ ������
	// IconWidth  - ������ ������, ���� USE_ICON_SIZE �� ����� ������������ ������ ������
	// IconHeight - ������ ������, ���� USE_ICON_SIZE �� ����� ������������ ������ ������
	HICON SetIcon(_In_ UINT uiImageID,	_In_opt_ Aligments Aligment = CenterTop,
				  _In_opt_ int IconWidth = USE_ICON_SIZE, _In_opt_ int IconHeight = USE_ICON_SIZE);
	HICON SetIcon(_In_ HICON hIcon,		_In_opt_ Aligments Aligment = CenterTop,
				  _In_opt_ int IconWidth = USE_ICON_SIZE, _In_opt_ int IconHeight = USE_ICON_SIZE);
	// �������� ����������� ��� ������, Aligments - ���� �������� ����������� ������������ ������
	// ColorMask - ����� ����������� � BITMAP
	// bUseColorMask - ���� ������������� �����
	// IconWidth  - ������ ������, ���� USE_ICON_SIZE �� ����� ������������ ������ ������
	// IconHeight - ������ ������, ���� USE_ICON_SIZE �� ����� ������������ ������ ������
	CBitmap* SetBitmap(_In_ UINT uiBitmapID, _In_opt_ bool bUseColorMask = false,
					   _In_opt_ COLORREF ColorMask = RGB(0, 0, 0), _In_opt_ Aligments Aligment = CenterTop,
					   _In_opt_ int BitmapWidth = USE_ICON_SIZE, _In_opt_ int BitmapHeight = USE_ICON_SIZE);
	CBitmap* SetBitmap(_In_ CBitmap* hBitmap, _In_opt_ bool bUseColorMask = false,
					   _In_opt_ COLORREF ColorMask = RGB(0, 0, 0), _In_opt_ Aligments Aligment = CenterTop,
					   _In_opt_ int BitmapWidth = USE_ICON_SIZE, _In_opt_ int BitmapHeight = USE_ICON_SIZE);
	//*********************************************************************************************
	void SetImageOffset	(_In_ long lOffset);		// ��������� ������� ����������� �� ������ ������
	void SetTextColor	(_In_ COLORREF Color);		// ��������� ����� ������
	void SetBkColor		(_In_ COLORREF BkColor);	// ��������� ���� ��� ������
	// ��������� ����� ������������� ������������ ���� ��� ��������� �������������
	void UseDefaultBkColor(_In_opt_ bool bUseStandart = true);
	//*********************************************************************************************
	void SetTooltip(_In_ CString Tooltip);
protected://***************************************************************************************
	void RepositionItems();		// ������������ ������� ������� ������
protected://***************************************************************************************
	bool m_bImageLoad;			// ���� ���� ��� ������ ���������
	long m_lOffset;				// ������ �� ������ �������� ��� ��������� ������
	CSize m_IconSize;			// ������ ������
	CPoint m_IconPos;			// �������������� �������� ������ ���� ������
	Aligments m_Aligment;		// �������� ������
	CImageList m_ImageList;		// ������
	//*********************************************************************************************
	bool m_bUseCustomBkColor;	// ���� ������������� ����������������� ���� ��� ������
	COLORREF m_TextColor;		// ���� ������
	COLORREF m_BkColor;			// ���� ����
	CString m_ButtonText;		// ����� �� ������
	CRect m_TextRect;			// ������ ������
	bool m_bNeedCalcTextPos;	// ���� ������������� ����������� ������� ������� ��� �����
	//*********************************************************************************************
	CToolTipCtrl *m_ptooltip;	// ������
public:	//*****************************************************************************************
	DECLARE_MESSAGE_MAP()
	virtual void PreSubclassWindow();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};	//*********************************************************************************************