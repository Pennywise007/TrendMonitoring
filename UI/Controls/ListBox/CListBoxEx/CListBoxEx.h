#pragma once

// Класс обёртка над CListBox, позволяет устанавливать цвета элементам и рисовать их в несколько строк
#include <afxwin.h>

////////////////////////////////////////////////////////////////////////////////
// CListBoxEx
class CListBoxEx : public CListBox
{
public:
    CListBoxEx()
    {

    }
    // Вставить элемент с заданием цвета
    // если nIndex == -1 делается AddString иначе - вставляется по указанному индексу
    int AddItem(const CString& itemText, COLORREF color, int nIndex = -1);

    DECLARE_MESSAGE_MAP()

protected:
    virtual void PreSubclassWindow() override;
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDIS) override;
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    afx_msg void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
};
