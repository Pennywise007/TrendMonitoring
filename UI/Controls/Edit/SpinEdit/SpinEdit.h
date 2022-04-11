#pragma once

#include "afxwin.h"
#include <memory>
#include <utility>
#include <Controls/Edit/CEditBase/CEditBase.h>
/*************************************************************************************************
	Данный контрол позволяет использовать CEdit соединенный с CSpinButtonCtrl

	Для обработки сообщений от CSpinCtrl необходимо ловить сообщения с идентификатором CEdit`а который вы создали
	Для задания отдельного идентификатора воспользуйтесь специальным конструктором

	Для дополнительной настройки CSpinCtrl используйте фунецию GetSpinCtrl()
*************************************************************************************************/
class CMySpinCtrl : public CSpinButtonCtrl
{
public:
	CMySpinCtrl();
	virtual ~CMySpinCtrl() {};

	DECLARE_MESSAGE_MAP()
public:
	// установка диапозона
	void SetRange32(_In_	int	   Left,	_In_  int	 Right);
	void SetRange64(_In_	float  Left,	_In_  float  Right);
	void GetRange64(_Out_	float &Left,	_Out_ float &Right);
	// установка флагов события изменения нажатия на контрол будут обрабатываться родительским классом
	void SetUseCustomDeltaPos(_In_opt_ bool bUseCustom = true);
	bool GetUseCustomDeltaPos();
protected:
	bool m_bNeedCustomDeltaPos;				// функция DeltaPos будет обрабатываться родительским классом
	std::pair<float, float> m_SpinRange;	// границы контрола
public:
	afx_msg BOOL OnDeltapos(NMHDR *pNMHDR, LRESULT *pResult);
};
//*************************************************************************************************
namespace SpinEdit
{
	// перечень типов привязки спина
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
	CSpinEdit();	// стандартный конструктор, в качестве идентификатора для спинконтрола будет задан ID эдита
	CSpinEdit(_In_ UINT SpinID);	// конструктор с возможностью задать ID спинконтрола
	~CSpinEdit();
public:	//*****************************************************************************************
	// установка привязки спинконтрола относительно эдита
	void SetSpinAlign(_In_opt_ SpinEdit::SpinAligns Align = SpinEdit::RIGHT);
	// установка границ для спинконтрола
	void SetRange32(_In_ int Left, _In_ int Right);
	void SetRange64(_In_ float Left, _In_ float Right);
	void GetRange(_Out_ float &Left, _Out_ float &Right);
	//*********************************************************************************************
	void SetSpinWidth(_In_ int NewWidth);					// установка ширины спинконтрола
	int GetSpinWidth() { return m_SpinWidth; };				// получаем ширину спинконтрола
	CMySpinCtrl *GetSpinCtrl() { return m_SpinCtrl.get(); }	// получаем указатель на спинконтрол
	//*********************************************************************************************
	void GetWindowRect(LPRECT lpRect) const;
	void GetClientRect(LPRECT lpRect) const;
	//*********************************************************************************************
	void UsePositiveDigitsOnly(_In_opt_ bool bUsePositiveDigitsOnly = true) override;
private://*****************************************************************************************
	void InitEdit();	// иницилизация контрола
	void ReposCtrl(_In_opt_ const CRect& EditRect);	// перемасштабирование контрола
private://*****************************************************************************************
	std::unique_ptr<CMySpinCtrl> m_SpinCtrl;		// экземпляр спин контрола
	SpinEdit::SpinAligns m_SpinAlign;	            // привязка спина
	std::pair<float, float> m_SpinRange;	        // границы контрола
	int m_SpinWidth;					            // ширина контрола
	int m_SpinCtrlID;					            // идентификатор спинконтрола
	bool m_bFromCreate;					            // флаг того что контрол создан не динамически
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

