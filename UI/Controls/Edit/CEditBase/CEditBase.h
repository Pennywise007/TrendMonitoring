#pragma once
#include "afxwin.h"
#include <utility>
//************************************************************************************************
#ifdef CEDIT_BASE_USE_TRANSLATE
#include <Dialog_ZET/Translate.h>
#endif // CEDIT_BASE_USE_TRANSLATE
/*************************************************************************************************
	������ ������� ��������� ��������� ���������� CEdit

	1) ����������� ���������� ����������� ��������� (������� ShowBubble)
	2) ��������� ToolTip
	3) ��������� ������ ������ � ����
	4) ��������� ������������� �������� ��� ������ � ������� � ��������� ������
		����� � ������������ CEditBase(true) ��� ������� FloatNumbersOnly(true)
**************************************************************************************************/
class CEditBase : public CEdit
{
public:	//*****************************************************************************************
	CEditBase(_In_opt_ bool bUseNumbersOnly = false);	// ����������� �����������
	~CEditBase();
public:	//*****************************************************************************************
	// ����������� ������������ ���������
	LRESULT ShowBubble(_In_ CString sTitle, _In_ CString sText, _In_opt_ INT Icon = TTI_ERROR);
	LRESULT HideBubble();		// �������� ����������� ���������
	//*********************************************************************************************
	// ��������� �������� �� ������ � �������
	virtual void UsePositiveDigitsOnly(_In_opt_ bool bUsePositiveDigitsOnly = true);
	void SetUseOnlyNumbers(_In_opt_ bool bUseNumbersOnly = true);
	void SetUseOnlyIntegersValue(_In_opt_ bool bUseIntegersOnly = true);
	//*********************************************************************************************
	// ����������� �������� �� ������������� � ������������ ��������
	void SetUseCtrlLimits(_In_opt_ bool bUseLimits) { m_bUseLimits = bUseLimits; }
	void SetMinMaxLimits (_In_ float MinVal, _In_ float MaxVal);
	//*********************************************************************************************
	void SetTooltip(_In_opt_ LPCTSTR lpszToolTipText = nullptr); // lpszToolTip == nullptr: disable tooltip
	CToolTipCtrl * GetToolTipCtrl() { return m_ptooltip; }
	//*********************************************************************************************
#pragma region �������� ������
	// ���������� ������������ �����������  ��� ������� �����
	void SetDefaultColors(_In_ bool bUseDefault = true);
	//*********************************************************************************************	
	COLORREF GetBkColor();						// ���� ����. ��� ��� ������� ��� �������� ������������
	void SetBkColor(const COLORREF color);		// ���� ����. ��� ��� ������� ��� �������� ������������
	//*********************************************************************************************	
	COLORREF GetTextColor();					// ���� ������
	void SetTextColor(const COLORREF color);	// ���� ������
#pragma endregion
private://*****************************************************************************************
	enum ErrorType
	{
		BAD_INPUT_VAL = 0,
		NOT_FIT_INTO_LIMITS
	};
	BOOL ShowError(_In_opt_ ErrorType Type = ErrorType::BAD_INPUT_VAL);
	// �������� ������ �� �� �������� ��� ����� ��� ���
	BOOL CheckNumericString(_In_ CString Str);
	void SetNumbersSettings(_In_ bool NewState);
protected://*****************************************************************************************
	// �����
	bool m_bUseDefaultColors;					// ������������� ����������� ������
	COLORREF m_colorBk;							// ���� ���� �� ��������� �����
	COLORREF m_colorText;						// ���� ������ �� ��������� ������	
	HBRUSH m_brushBk;							// ����� ��� ��������� ����
	//*********************************************************************************************
	bool m_bUseNumbersOnly;						// ���� ��� ����� �������������� ������ �����
	bool m_bUseOnlyIntegers;					// ���� ������������� ������ ����� �����
	bool m_bUsePositivesDigitsOnly;				// ���� ��� ����� �������������� ������ ������������� �����
	//*********************************************************************************************
	CToolTipCtrl *m_ptooltip;					// ����������� ���������
	//*********************************************************************************************
	bool m_bUseLimits;							// ���� ������������� ����������� �� �������� ��������
	std::pair<float, float> m_ValuesRange;		// ���������� � ������������ �������� ��������
protected://***************************************************************************************
	DECLARE_MESSAGE_MAP()
public://******************************************************************************************
	virtual void PreSubclassWindow();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg HBRUSH CtlColor(CDC* /*pDC*/, UINT /*nCtlColor*/);
	afx_msg LRESULT OnPaste(WPARAM wParam, LPARAM lParam);
};	//*********************************************************************************************

