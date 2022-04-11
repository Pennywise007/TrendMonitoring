#include "stdafx.h"

#include <assert.h>

#include "Controls/ThemeManagement.h"

#include "CListBoxEx.h"

BEGIN_MESSAGE_MAP(CListBoxEx, CListBox)
    ON_WM_MEASUREITEM_REFLECT()
    ON_WM_SIZE()
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////
// CListBoxEx
int CListBoxEx::AddItem(const CString& itemText, COLORREF color, int nIndex /*= -1*/)
{
    // индекс куда вставл€ем элемент
    int index;

    if (nIndex == -1)
        index = AddString(itemText);
    else
        index = InsertString(nIndex, itemText);

    SetItemData(index,color);

    // иногда при добавлении элементов не возникает перерисовки
    Invalidate();

    return index;
}

//----------------------------------------------------------------------------//
void CListBoxEx::PreSubclassWindow()
{
    // провер€ем необходимые стили, их невозможно установить через ModifyStyle(не будут работать)
    if (!(CListBox::GetStyle() & LBS_HASSTRINGS) ||
        !(CListBox::GetStyle() & LBS_OWNERDRAWVARIABLE))
        assert(!"” списка должен быть установлен стиль LBS_OWNERDRAWVARIABLE и LBS_HASSTRINGS дл€ корректной работы!");

    EnableWindowTheme(GetSafeHwnd(), L"ListBox", L"Explorer", NULL);

    CListBox::PreSubclassWindow();
}

//----------------------------------------------------------------------------//
void CListBoxEx::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
    // „тобы вызывалс€ нужен стиль LBS_OWNERDRAWVARIABLE
    int nItem = lpMeasureItemStruct->itemID;
    CString sLabel;
    CRect rcLabel;

    CListBox::GetText( nItem, sLabel );
    CListBox::GetItemRect(nItem, rcLabel);

    // рассчитываем высоту элемента
    CPaintDC dc(this);
    dc.SelectObject(CListBox::GetFont());

    lpMeasureItemStruct->itemHeight = dc.DrawText(sLabel, -1, rcLabel, DT_WORDBREAK | DT_CALCRECT);
}

//----------------------------------------------------------------------------//
void CListBoxEx::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
    // ѕри клике в пустую область может придти DrawItem с невалидным идентификеатором
    if (lpDIS->itemID == -1)
        return;

    CDC* pDC = CDC::FromHandle(lpDIS->hDC);

    COLORREF rColor = (COLORREF)lpDIS->itemData; // RGB in item data

    CString sLabel;
    GetText(lpDIS->itemID, sLabel);

    // item selected
    if ((lpDIS->itemState & ODS_SELECTED) &&
        (lpDIS->itemAction & (ODA_SELECT | ODA_DRAWENTIRE)))
    {
        // draw color box
        CBrush colorBrush(rColor);
        CRect colorRect = lpDIS->rcItem;

        HTHEME hTheme = OpenThemeData(m_hWnd, L"LISTBOX");

        // draw label background
        CBrush labelBrush(::GetThemeSysColor(hTheme, COLOR_HIGHLIGHT));
        CRect labelRect = lpDIS->rcItem;
        pDC->FillRect(&labelRect,&labelBrush);

        // draw label text
        COLORREF colorTextSave = pDC->SetTextColor(::GetThemeSysColor(hTheme, COLOR_HIGHLIGHTTEXT));
        COLORREF colorBkSave = pDC->SetBkColor(::GetThemeSysColor(hTheme, COLOR_HIGHLIGHT));
        pDC->DrawText( sLabel, -1, &lpDIS->rcItem, DT_WORDBREAK);

        pDC->SetTextColor(colorTextSave);
        pDC->SetBkColor(colorBkSave);

        // прекращаем работу с темой
        if (hTheme)
            CloseThemeData(hTheme);

        return;
    }

    // item brought into box
    if (lpDIS->itemAction & ODA_DRAWENTIRE)
    {
        CBrush brush(rColor);
        CRect rect = lpDIS->rcItem;
        pDC->SetBkColor(rColor);
        pDC->FillRect(&rect,&brush);
        pDC->DrawText( sLabel, -1, &lpDIS->rcItem, DT_WORDBREAK);

        return;
    }

    // item deselected
    if (!(lpDIS->itemState & ODS_SELECTED) &&
        (lpDIS->itemAction & ODA_SELECT))
    {
        CRect rect = lpDIS->rcItem;
        CBrush brush(rColor);
        pDC->SetBkColor(rColor);
        pDC->FillRect(&rect,&brush);
        pDC->DrawText( sLabel, -1, &lpDIS->rcItem, DT_WORDBREAK);

        return;
    }
}

//----------------------------------------------------------------------------//
void CListBoxEx::OnSize(UINT nType, int cx, int cy)
{
    MEASUREITEMSTRUCT measureStruct;

    // дл€ каждого элемента пересчитываем размер
    for (int item = 0, count = CListBox::GetCount();
         item < count; ++item)
    {
        // смотрим сколько он занимает по высоте
        measureStruct.itemID = item;
        MeasureItem(&measureStruct);

        // если высота изменилась надо еЄ переустановить
        if (GetItemHeight(item) != measureStruct.itemHeight)
            SetItemHeight(item, measureStruct.itemHeight);
    }

    // иногда возникают случаи(при показе скроллера) когда не происходит перерисовка
    Invalidate();

    CListBox::OnSize(nType, cx, cy);
}

//----------------------------------------------------------------------------//
LRESULT CListBoxEx::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == LB_ADDSTRING || message == LB_INSERTSTRING)
    {
        LRESULT res = CListBox::WindowProc(message, wParam, lParam);

        // ≈сли добавл€етс€ строка надо ей задать данные с нормальным цветом
        SetItemData((int)res, GetBkColor(GetDC()->m_hDC));

        return res;
    }

    return CListBox::WindowProc(message, wParam, lParam);
}