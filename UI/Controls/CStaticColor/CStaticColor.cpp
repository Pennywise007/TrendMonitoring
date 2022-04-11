//-----------------------------------------------------------------------------
/*		Идея о ON_WM_CTLCOLOR_REFLECT взята на сайте вопросов stackoverflow.com.
*/
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "CStaticColor.h"
//-----------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CStaticColor, CStatic)
	ON_WM_CTLCOLOR_REFLECT()
END_MESSAGE_MAP()
//-----------------------------------------------------------------------------
CStaticColor::CStaticColor(const COLORREF colorBackground, const COLORREF colorText)
	: m_bTransparent (false)
	, m_colorBk(colorBackground)
	, m_colorText(colorText)
{
	m_brushBk = CreateSolidBrush(m_colorBk);
}
//-----------------------------------------------------------------------------
CStaticColor::~CStaticColor()
{
	if (m_brushBk != NULL)
		DeleteObject(m_brushBk);
}
//-----------------------------------------------------------------------------
CStaticColor::CStaticColor(const CStaticColor & val)// : CStatic(val)
	: m_bTransparent (val.m_bTransparent)
	, m_colorBk (val.m_colorBk)
	, m_colorText (val.m_colorText)
{
	if (m_brushBk != NULL)
		DeleteObject(m_brushBk);
	m_brushBk = CreateSolidBrush(m_colorBk);
	*this = val;
}
//-----------------------------------------------------------------------------
CStaticColor & CStaticColor::operator = (const CStaticColor & val)
{
//	*((CStatic*)this) = *((CStatic*)&val);
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
bool CStaticColor::GetTransparent()
{
	return m_bTransparent;
}
//-----------------------------------------------------------------------------
void CStaticColor::SetTransparent(const bool bTransparent)
{
	if (m_bTransparent != bTransparent)
	{
		m_bTransparent = bTransparent;
		Invalidate();
	}
}
//-----------------------------------------------------------------------------
COLORREF CStaticColor::GetBkColor()
{
	return m_colorBk;
}
//-----------------------------------------------------------------------------
void CStaticColor::SetBkColor(const COLORREF color)
{
	m_bTransparent = false;
	if (m_colorBk != color)
	{
		m_colorBk = color;

		if (m_brushBk != NULL)
			DeleteObject(m_brushBk);
		m_brushBk = CreateSolidBrush(m_colorBk);
	}
	Invalidate();
}
//-----------------------------------------------------------------------------
COLORREF CStaticColor::GetTextColor()
{
	return m_colorText;
}
//-----------------------------------------------------------------------------
void CStaticColor::SetTextColor(const COLORREF color)
{
	m_bTransparent = false;
	if (m_colorText != color)
		m_colorText = color;
	Invalidate();
}
//-----------------------------------------------------------------------------
HBRUSH CStaticColor::CtlColor(CDC* pDC, UINT nCtlColor)
{
	if (m_bTransparent)
		return NULL;
	
	GetWindowText(m_text);

	CRect rect;
	GetClientRect(rect);

	FillRect(*pDC, rect, m_brushBk);

	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectObject(CStatic::GetFont());

	DWORD Style = CWnd::GetStyle();
	UINT TextFormat = DT_EDITCONTROL | DT_WORDBREAK;
	// устанавливаем привязку текста
	if (Style & SS_LEFT)
		TextFormat |= DT_LEFT;
	else if (Style & SS_RIGHT)
		TextFormat |= DT_RIGHT;
	else if (Style & SS_CENTER)
		TextFormat |= DT_CENTER;

	Style = CStatic::GetStyle();
	CRect TextRect(rect);
	if (Style & SS_CENTERIMAGE)
	{	// размер текста, который нужно отобразить, чтобы расcчитать как его центрировать
		pDC->DrawText(m_text, rect, TextFormat | DT_CALCRECT | DT_NOCLIP);
		TextRect.top = TextRect.CenterPoint().y - rect.Height() / 2;
		TextRect.bottom = TextRect.top + rect.Height();
	}

	if (IsWindowEnabled() != FALSE)
	{
		pDC->SetTextColor(m_colorText);
		pDC->DrawText(m_text, TextRect, TextFormat);
	}
	else
	{
		pDC->SetTextColor(GetSysColor(COLOR_WINDOW));
		pDC->DrawText(m_text, TextRect, TextFormat);

		pDC->SetTextColor(GetSysColor(COLOR_GRAYTEXT));
		pDC->DrawText(m_text, TextRect, TextFormat);
	}

	ReleaseDC(pDC);
	
	return m_brushBk;
}
//-----------------------------------------------------------------------------