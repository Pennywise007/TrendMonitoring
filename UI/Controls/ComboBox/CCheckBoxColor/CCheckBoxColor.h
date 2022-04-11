//-----------------------------------------------------------------------------
#pragma once

#include <afxwin.h>
//-----------------------------------------------------------------------------
class CCheckBoxColor : public CButton
{
private:
	// Прозрачность, если не задан цвет фона
	bool m_bTransparent;			// = true;
	// Цвет фона по умолчанию белый (если кто-то задает непрозрачность без установки цвета)
	COLORREF m_colorBk;				// = GetSysColor(COLOR_3DFACE)
	// Цвет текста по умолчанию чёрный
	COLORREF m_colorText;			// = (GetSysColor(COLOR_BTNTEXT))
	// Кисти для рисования фона
	HBRUSH m_brushBk;

	CString m_text;
		
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
	afx_msg void OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult);
public:
	CCheckBoxColor(const COLORREF colorBackground = GetSysColor(COLOR_3DFACE), const COLORREF colorText = GetSysColor(COLOR_BTNTEXT));
	~CCheckBoxColor();
	CCheckBoxColor(const CCheckBoxColor & val);
	CCheckBoxColor & operator = (const CCheckBoxColor & val);
	virtual void PreSubclassWindow();
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

	// Сделать фон прозрачным (или если bTransparent == true, восстановить предыдущий цвет фона)
	bool GetTransparent();
	void SetTransparent(const bool bTransparent);

	// Цвет фона. При его задании фон делается непрозрачным
	COLORREF GetBkColor();
	void SetBkColor(const COLORREF color);

	// Цвет текста
	COLORREF GetTextColor();
	void SetTextColor(const COLORREF color);
};
//-----------------------------------------------------------------------------