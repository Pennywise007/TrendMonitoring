#pragma once

#include "afxwin.h"
#include <memory>
#include <utility>
#include <Controls/Edit/CEditBase/CEditBase.h>
/*************************************************************************************************
	������ ������� ��������� ������������ CEdit ����������� � CSpinButtonCtrl

	��� ��������� ��������� �� CSpinCtrl ���������� ������ ��������� � ��������������� CEdit`� ������� �� �������
	��� ������� ���������� �������������� �������������� ����������� �������������

	��� �������������� ��������� CSpinCtrl ����������� ������� GetSpinCtrl()
*************************************************************************************************/
class CMySpinCtrl : public CSpinButtonCtrl
{
public:
	CMySpinCtrl();
	virtual ~CMySpinCtrl() {};

	DECLARE_MESSAGE_MAP()
public:
	// ��������� ���������
	void SetRange32(_In_	int	   Left,	_In_  int	 Right);
	void SetRange64(_In_	float  Left,	_In_  float  Right);
	void GetRange64(_Out_	float &Left,	_Out_ float &Right);
	// ��������� ������ ������� ��������� ������� �� ������� ����� �������������� ������������ �������
	void SetUseCustomDeltaPos(_In_opt_ bool bUseCustom = true);
	bool GetUseCustomDeltaPos();
protected:
	bool m_bNeedCustomDeltaPos;				// ������� DeltaPos ����� �������������� ������������ �������
	std::pair<float, float> m_SpinRange;	// ������� ��������
public:
	afx_msg BOOL OnDeltapos(NMHDR *pNMHDR, LRESULT *pResult);
};
//*************************************************************************************************
namespace SpinEdit
{
	// �������� ����� �������� �����
	enum SpinAligns
	{
		LEFT  = UDS_ALIGNLEFT,
		RIGHT = UDS_ALIGNRIGHT
	};
}
//*************************************************************************************************
class CSpinEdit :
	public CEditBase
{
public:	//*****************************************************************************************
	CSpinEdit();	// ����������� �����������, � �������� �������������� ��� ������������ ����� ����� ID �����
	CSpinEdit(_In_ UINT SpinID);	// ����������� � ������������ ������ ID ������������
	~CSpinEdit();
public:	//*****************************************************************************************
	// ��������� �������� ������������ ������������ �����
	void SetSpinAlign(_In_opt_ SpinEdit::SpinAligns Align = SpinEdit::RIGHT);
	// ��������� ������ ��� ������������
	void SetRange32(_In_ int Left, _In_ int Right);
	void SetRange64(_In_ float Left, _In_ float Right);
	void GetRange(_Out_ float &Left, _Out_ float &Right);
	//*********************************************************************************************
	void SetSpinWidth(_In_ int NewWidth);					// ��������� ������ ������������
	int GetSpinWidth() { return m_SpinWidth; };				// �������� ������ ������������
	CMySpinCtrl *GetSpinCtrl() { return m_SpinCtrl.get(); }	// �������� ��������� �� �����������
	//*********************************************************************************************
	void GetWindowRect(LPRECT lpRect) const;
	void GetClientRect(LPRECT lpRect) const;
	//*********************************************************************************************
	void UsePositiveDigitsOnly(_In_opt_ bool bUsePositiveDigitsOnly = true) override;
private://*****************************************************************************************
	void InitEdit();	// ������������ ��������
	void ReposCtrl(_In_opt_ const CRect& EditRect);	// ������������������� ��������
private://*****************************************************************************************
	std::unique_ptr<CMySpinCtrl> m_SpinCtrl;		// ��������� ���� ��������
	SpinEdit::SpinAligns m_SpinAlign;	            // �������� �����
	std::pair<float, float> m_SpinRange;	        // ������� ��������
	int m_SpinWidth;					            // ������ ��������
	int m_SpinCtrlID;					            // ������������� ������������
	bool m_bFromCreate;					            // ���� ���� ��� ������� ������ �� �����������
protected://***************************************************************************************
	DECLARE_MESSAGE_MAP()
public:
	virtual void PreSubclassWindow();
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
};	//*********************************************************************************************

