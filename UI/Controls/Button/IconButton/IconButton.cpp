#include "stdafx.h"
#include "IconButton.h"

CIconButton::CIconButton()
	: m_bImageLoad			(false)
	, m_Aligment			(Aligments::CenterCenter)
	, m_lOffset				(5)
	, m_bNeedCalcTextPos	(true)
	, m_bUseCustomBkColor	(false)
{
	m_TextColor = GetSysColor(COLOR_BTNTEXT);
	m_BkColor	= GetSysColor(COLOR_BTNFACE);

	m_ptooltip = new CToolTipCtrl();
}

CIconButton::~CIconButton()
{
	if (m_bImageLoad)
		m_ImageList.DeleteImageList();
	if (m_ptooltip)
	{
		delete m_ptooltip;
		m_ptooltip = nullptr;
	}
}

BEGIN_MESSAGE_MAP(CIconButton, CButton)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CIconButton::OnNMCustomdraw)
	ON_WM_SIZE()
END_MESSAGE_MAP()

HICON CIconButton::SetIcon(_In_ UINT uiImageID,
							_In_opt_ Aligments Aligment /*= CenterTop*/,
							_In_opt_ int IconWidth	/*= USE_ICON_SIZE*/,
							_In_opt_ int IconHeight /*= USE_ICON_SIZE*/)
{
	return SetIcon(AfxGetApp()->LoadIcon(uiImageID), Aligment, IconWidth, IconHeight);
}

HICON CIconButton::SetIcon(_In_ HICON hIcon,
							_In_opt_ Aligments Aligment /*= CenterTop*/,
							_In_opt_ int IconWidth	/*= USE_ICON_SIZE*/,
							_In_opt_ int IconHeight /*= USE_ICON_SIZE*/)
{
	if (m_bImageLoad)
	{
		m_ImageList.DeleteImageList();
		m_bImageLoad = false;
	}

	if (IconWidth == USE_ICON_SIZE || IconHeight == USE_ICON_SIZE)
	{
		ICONINFO info;
		if (GetIconInfo(hIcon, &info))
		{
			BITMAP m_bitmap;
			if (GetObject(info.hbmColor, sizeof(m_bitmap), &m_bitmap))
			{
				if (IconWidth  == USE_ICON_SIZE)
					IconWidth  = m_bitmap.bmWidth;
				if (IconHeight == USE_ICON_SIZE)
					IconHeight = m_bitmap.bmHeight;
			}
			DeleteObject(&m_bitmap);
		}
		DeleteObject(&info);
	}
	m_IconSize.SetSize(IconWidth, IconHeight);

	if (m_ImageList.Create(m_IconSize.cx, m_IconSize.cy, ILC_COLOR32, 1, 1))
	{
		m_ImageList.Add(hIcon);
		m_bImageLoad = true;
		m_Aligment = Aligment;
		RepositionItems();
	}

	return hIcon;
}

CBitmap* CIconButton::SetBitmap(_In_ UINT uiBitmapID,
								_In_opt_ bool bUseColorMask /*= false*/,
								_In_opt_ COLORREF ColorMask /*= RGB(0, 0, 0)*/,
								_In_opt_ Aligments Aligment /*= CenterTop*/,
								_In_opt_ int BitmapWidth  /*= USE_ICON_SIZE*/,
								_In_opt_ int BitmapHeight /*= USE_ICON_SIZE*/)
{
	CBitmap MyBitMap;
	MyBitMap.LoadBitmap(uiBitmapID);
	return SetBitmap(&MyBitMap, bUseColorMask, ColorMask, Aligment, BitmapWidth, BitmapHeight);
}

CBitmap* CIconButton::SetBitmap(_In_ CBitmap* hBitmap,
								_In_opt_ bool bUseColorMask /*= false*/,
								_In_opt_ COLORREF ColorMask /*= RGB(0, 0, 0)*/,
								_In_opt_ Aligments Aligment /*= CenterTop*/,
								_In_opt_ int BitmapWidth  /*= USE_ICON_SIZE*/,
								_In_opt_ int BitmapHeight /*= USE_ICON_SIZE*/)
{
	if (m_bImageLoad)
	{
		m_ImageList.DeleteImageList();
		m_bImageLoad = false;
	}

	if (BitmapWidth == USE_ICON_SIZE || BitmapHeight == USE_ICON_SIZE)
	{
		BITMAP m_bitmap;
		if (hBitmap->GetBitmap(&m_bitmap))
		{
			if (BitmapWidth == USE_ICON_SIZE)
				BitmapWidth = m_bitmap.bmWidth;
			if (BitmapHeight == USE_ICON_SIZE)
				BitmapHeight = m_bitmap.bmHeight;
		}
		DeleteObject(&m_bitmap);
	}
	m_IconSize.SetSize(BitmapWidth, BitmapHeight);

	UINT Flags = ILC_COLORDDB;
	if (bUseColorMask)
		Flags |= ILC_MASK;

	if (m_ImageList.Create(m_IconSize.cx, m_IconSize.cy, Flags, 1, 1))
	{
		m_ImageList.Add(hBitmap, ColorMask);
		m_bImageLoad = true;
		m_Aligment = Aligment;
		RepositionItems();
	}

	return hBitmap;
}

HBRUSH CreateGradientBrush(COLORREF top, COLORREF bottom, LPNMCUSTOMDRAW item)
{
	HBRUSH Brush = NULL;
	HDC hdcmem = CreateCompatibleDC(item->hdc);
	HBITMAP hbitmap = CreateCompatibleBitmap(item->hdc, item->rc.right - item->rc.left, item->rc.bottom - item->rc.top);
	SelectObject(hdcmem, hbitmap);

	int r1 = GetRValue(top), r2 = GetRValue(bottom), g1 = GetGValue(top), g2 = GetGValue(bottom), b1 = GetBValue(top), b2 = GetBValue(bottom);
	for (int i = 0; i < item->rc.bottom - item->rc.top; ++i)
	{
		RECT temp;
		int r, g, b;
		r = int(r1 + double(i * (r2 - r1) / item->rc.bottom - item->rc.top));
		g = int(g1 + double(i * (g2 - g1) / item->rc.bottom - item->rc.top));
		b = int(b1 + double(i * (b2 - b1) / item->rc.bottom - item->rc.top));
		Brush = CreateSolidBrush(RGB(r, g, b));
		temp.left = 0;
		temp.top = i;
		temp.right = item->rc.right - item->rc.left;
		temp.bottom = i + 1;
		FillRect(hdcmem, &temp, Brush);
		DeleteObject(Brush);
	}
	HBRUSH pattern = CreatePatternBrush(hbitmap);

	DeleteDC(hdcmem);
	DeleteObject(hbitmap);
	if (Brush)
		DeleteObject(Brush);

	return pattern;
}

COLORREF Darker(COLORREF Color, int Percent)
{
	// уменьшениe яркости
	int r = GetRValue(Color);
	int g = GetGValue(Color);
	int b = GetBValue(Color);

	r = r - MulDiv(r, Percent, 100);
	g = g - MulDiv(g, Percent, 100);
	b = b - MulDiv(b, Percent, 100);
	return RGB(r, g, b);
}

COLORREF Lighter(COLORREF Color, int Percent)
{
	// уменьшениe яркости
	int r = GetRValue(Color);
	int g = GetGValue(Color);
	int b = GetBValue(Color);

	r = r + MulDiv(255 - r, Percent, 100);
	g = g + MulDiv(255 - g, Percent, 100);
	b = b + MulDiv(255 - b, Percent, 100);
	return RGB(r, g, b);
}

void CIconButton::OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);

	CString Temp;
	CButton::GetWindowText(Temp);
	if (!Temp.IsEmpty())
	{
		m_ButtonText = Temp;
		RepositionItems();
		CButton::SetWindowText(CString());
	}

	if (m_bUseCustomBkColor)
	{
		HBRUSH TempBrush;
		HPEN TempPen;

		// задний фон кнопки
		TempBrush = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
		FillRect(pNMCD->hdc, &pNMCD->rc, TempBrush);
		DeleteObject(TempBrush);

		// задание цвета границы кнопки (оконтовки)
		TempPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_ACTIVEBORDER));

		if (pNMCD->uItemState & ODS_DISABLED)
			TempBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
		else
		{
			if (pNMCD->uItemState & ODS_HOTLIGHT)
			{
				if (pNMCD->uItemState & ODS_SELECTED)
				{
					// выбор мышкой
					DeleteObject(TempPen);
					TempPen = CreatePen(PS_SOLID, 1, RGB(44, 98, 139));

					TempBrush = m_bUseCustomBkColor ? CreateSolidBrush(Darker(m_BkColor, 20)) :
						CreateGradientBrush(RGB(216, 238, 250), RGB(140, 202, 235), pNMCD);
				}
				else
				{
					// перемещении мыши над контролом
					TempBrush = m_bUseCustomBkColor ? CreateSolidBrush(Lighter(m_BkColor, 20)) :
						CreateGradientBrush(RGB(230, 245, 253), RGB(172, 220, 247), pNMCD);
				}
			}
			else
			{
				// стандартное состояние контрола
				TempBrush = m_bUseCustomBkColor ? CreateSolidBrush(m_BkColor) :
					CreateGradientBrush(GetSysColor(COLOR_BTNFACE), RGB(216, 216, 216), pNMCD);
			}
		}

		SelectObject(pNMCD->hdc, TempPen);
		SelectObject(pNMCD->hdc, TempBrush);
		SetBkMode(pNMCD->hdc, TRANSPARENT);
		RoundRect(pNMCD->hdc, pNMCD->rc.left, pNMCD->rc.top, pNMCD->rc.right, pNMCD->rc.bottom, 6, 6);

		DeleteObject(TempPen);
		DeleteObject(TempBrush);
	}

	CDC *pDC = CDC::FromHandle(pNMCD->hdc);

	//Drawing Bitmap
	if (m_bImageLoad)
	{
		m_ImageList.DrawIndirect(
			pDC,
			0,
			m_IconPos,
			m_IconSize,
			CPoint(0, 0),
			ILD_TRANSPARENT,	// UINT fStyle
			DSTINVERT,			// DWORD dwRop
			CLR_DEFAULT,		// COLORREF rgbBack
			CLR_DEFAULT,		// COLORREF rgbFore
			ILS_NORMAL,			// DWORD fState
			255,				// DWOR D Frame
			CLR_DEFAULT			// COLORREF crEffect
			);
	}

	pDC->SetBkMode(TRANSPARENT);

	if (m_bNeedCalcTextPos)
	{
		// получаем размер текста который нужно отобразить чтобы расчитать как его центрировать
		CRect tempRect(m_TextRect);
		pDC->DrawText(m_ButtonText, tempRect, DT_EDITCONTROL | DT_WORDBREAK | DT_CALCRECT | DT_NOCLIP);
		m_TextRect.top = m_TextRect.CenterPoint().y - tempRect.Height() / 2;
		m_TextRect.bottom = m_TextRect.top + tempRect.Height();
		m_bNeedCalcTextPos = false;
	}

	pDC->SetBkColor(GetSysColor(COLOR_BTNFACE));

	if (pNMCD->uItemState & ODS_DISABLED)
	{
		pDC->SetTextColor(GetSysColor(COLOR_WINDOW));
		pDC->DrawText(m_ButtonText, m_TextRect, DT_EDITCONTROL | DT_WORDBREAK | DT_END_ELLIPSIS | DT_CENTER);

		pDC->SetTextColor(GetSysColor(COLOR_GRAYTEXT));
		pDC->DrawText(m_ButtonText, m_TextRect, DT_EDITCONTROL | DT_WORDBREAK | DT_END_ELLIPSIS | DT_CENTER);
	}
	else
	{
		pDC->SetTextColor(m_TextColor);
		pDC->DrawText(m_ButtonText, m_TextRect, DT_EDITCONTROL | DT_WORDBREAK | DT_CENTER);
	}

	*pResult = 0;
}

void CIconButton::RepositionItems()
{
	CRect ControlRect;
	GetClientRect(ControlRect);

	if (m_bImageLoad)
	{
		switch (m_Aligment)
		{
			case LeftCenter:
				m_IconPos.SetPoint(m_lOffset, ControlRect.CenterPoint().y - m_IconSize.cy / 2);

				m_TextRect.top = ControlRect.top + m_lOffset;
				m_TextRect.bottom = ControlRect.bottom - m_lOffset;
				m_TextRect.left = m_IconPos.x + m_IconSize.cx + m_lOffset;
				m_TextRect.right = ControlRect.right - m_lOffset;
				break;
			case CenterTop:
				m_IconPos.SetPoint(ControlRect.CenterPoint().x - m_IconSize.cx / 2, m_lOffset);
				m_TextRect.top = m_IconSize.cy + m_lOffset;
				m_TextRect.bottom = ControlRect.bottom - m_lOffset;
				m_TextRect.left = ControlRect.left + m_lOffset;
				m_TextRect.right = ControlRect.right - m_lOffset;
				break;
			case CenterBottom:
				m_IconPos.SetPoint(ControlRect.CenterPoint().x - m_IconSize.cx / 2, ControlRect.Height() - m_IconSize.cy);
				m_TextRect.top = ControlRect.top + m_lOffset;
				m_TextRect.bottom = ControlRect.bottom - m_IconSize.cy - m_lOffset;
				m_TextRect.left = ControlRect.left + m_lOffset;
				m_TextRect.right = ControlRect.right - m_lOffset;
				break;
			case RightCenter:
				m_IconPos.SetPoint(ControlRect.Width() - m_IconSize.cx - m_lOffset, ControlRect.CenterPoint().y - m_IconSize.cy / 2);
				m_TextRect.top = ControlRect.top + m_lOffset;
				m_TextRect.bottom = ControlRect.bottom - m_lOffset;
				m_TextRect.left = ControlRect.left + m_lOffset;
				m_TextRect.right = ControlRect.right - m_IconSize.cx - m_lOffset;
				break;
			case CenterCenter:
				m_IconPos.SetPoint(ControlRect.CenterPoint().x - m_IconSize.cx / 2, ControlRect.CenterPoint().y - m_IconSize.cy / 2);
				m_TextRect.SetRectEmpty();
				break;
			default:
				m_TextRect = ControlRect;
				break;
		}
	}
	else
		m_TextRect = ControlRect;

	m_bNeedCalcTextPos = true;
	Invalidate();
}

void CIconButton::PreSubclassWindow()
{
	GetWindowText(m_ButtonText);

	CButton::SetWindowText(CString());

	GetClientRect(m_TextRect);

//	m_ptooltip->Create(this);
	//m_ptooltip->Activate(1);

	CButton::PreSubclassWindow();
}

void CIconButton::OnSize(UINT nType, int cx, int cy)
{
	CButton::OnSize(nType, cx, cy);
	RepositionItems();
}

void CIconButton::SetImageOffset(_In_ long lOffset)
{
	m_lOffset = lOffset;
	RepositionItems();
}

void CIconButton::SetTextColor(_In_ COLORREF Color)
{
	m_TextColor = Color;
	Invalidate();
}

void CIconButton::SetBkColor(_In_ COLORREF BkColor)
{
	m_bUseCustomBkColor = true;
	m_BkColor = BkColor;
	Invalidate();
}

void CIconButton::UseDefaultBkColor(_In_opt_ bool bUseStandart /*= true*/)
{
	m_bUseCustomBkColor = !bUseStandart;
	Invalidate();
}

BOOL CIconButton::PreTranslateMessage(MSG* pMsg)
{
	//m_ptooltip->RelayEvent(pMsg);
	return CButton::PreTranslateMessage(pMsg);
}

void CIconButton::SetTooltip(_In_ CString Tooltip)
{
	m_ptooltip->AddTool(this, Tooltip);
}