//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "CCheckBoxColor.h"
//-----------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CCheckBoxColor, CButton)
	ON_WM_CTLCOLOR_REFLECT()
END_MESSAGE_MAP()
//-----------------------------------------------------------------------------
CCheckBoxColor::CCheckBoxColor(const COLORREF colorBackground, const COLORREF colorText)
	: m_bTransparent (false)
	, m_colorBk (GetSysColor(COLOR_3DFACE))
	, m_colorText (GetSysColor(COLOR_BTNTEXT))
{

	m_colorText = colorText;
	m_colorBk = colorBackground;
	m_brushBk = CreateSolidBrush(m_colorBk);
	SetBkColor(m_colorBk);
}
//-----------------------------------------------------------------------------
CCheckBoxColor::~CCheckBoxColor()
{
	if (m_brushBk != NULL)
		DeleteObject(m_brushBk);
}
//-----------------------------------------------------------------------------
CCheckBoxColor::CCheckBoxColor(const CCheckBoxColor & val)// : CButton(val)
	: m_bTransparent (val.m_bTransparent)
	, m_colorBk (val.m_colorBk)
	, m_colorText (val.m_colorText)
{
	if (m_brushBk != NULL)
		DeleteObject(m_brushBk);
	m_brushBk = CreateSolidBrush(m_colorBk);
	SetBkColor(GetSysColor(COLOR_3DFACE));
	*this = val;
}
//-----------------------------------------------------------------------------
CCheckBoxColor & CCheckBoxColor::operator = (const CCheckBoxColor & val)
{
//	*((CButton*)this) = *((CButton*)&val);
	m_bTransparent = val.m_bTransparent;
	m_colorText = val.m_colorText;
	m_colorBk = val.m_colorBk;

	if (m_brushBk != NULL)
		DeleteObject(m_brushBk);
	m_brushBk = CreateSolidBrush(m_colorBk);

	Invalidate();
	return *this;
}
//-----------------------------------------------------------------------------
void CCheckBoxColor::PreSubclassWindow()
{	//	SetWindowTheme(m_hWnd, L"wstr", L"wstr");
	SetWindowTheme(m_hWnd, L"", L"");
	CButton::PreSubclassWindow();
}
//-----------------------------------------------------------------------------
BOOL CCheckBoxColor::PreCreateWindow(CREATESTRUCT& cs)
{
	SetWindowTheme(m_hWnd, L"", L"");
	return CButton::PreCreateWindow(cs);
}
//-----------------------------------------------------------------------------
bool CCheckBoxColor::GetTransparent()
{
	return m_bTransparent;
}
//-----------------------------------------------------------------------------
void CCheckBoxColor::SetTransparent(const bool bTransparent)
{
	if (m_bTransparent != bTransparent)
	{
		m_bTransparent = bTransparent;
		Invalidate();
	}
}
//-----------------------------------------------------------------------------
COLORREF CCheckBoxColor::GetBkColor()
{
	return m_colorBk;
}
//-----------------------------------------------------------------------------
void CCheckBoxColor::SetBkColor(const COLORREF color)
{
	m_bTransparent = false;
	if (m_colorBk != color)
	{
		m_colorBk = color;

		if (m_brushBk != NULL)
			DeleteObject(m_brushBk);
		m_brushBk = CreateSolidBrush(m_colorBk);
	}
	if ((m_hWnd != NULL) && (IsWindow(m_hWnd) != FALSE))
		Invalidate();
}
//-----------------------------------------------------------------------------
COLORREF CCheckBoxColor::GetTextColor()
{
	return m_colorText;
}
//-----------------------------------------------------------------------------
void CCheckBoxColor::SetTextColor(const COLORREF color)
{
	m_bTransparent = false;
	if (m_colorText != color)
		m_colorText = color;
	Invalidate();
}
//-----------------------------------------------------------------------------
HBRUSH CCheckBoxColor::CtlColor(CDC* pDC, UINT nCtlColor)
{
	pDC->SetTextColor(m_colorText);
	// Мы не хотим рисовать фон при рисовании текста.    
	// Цвет фона происходит при рисования фона контрола.
	pDC->SetBkMode(TRANSPARENT);

	// Возврашаем NULL (чтобы указать, что родительский объект будет
	// использовать кисть имеющегося у него цвета фона) или m_brush
	return m_bTransparent ? NULL : m_brushBk;
}
//-----------------------------------------------------------------------------