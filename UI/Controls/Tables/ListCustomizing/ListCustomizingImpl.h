#pragma once

#include "Controls/ThemeManagement.h"

#include "ListCustomizing.h"

/////////////////////////////////////////////////////////////////////////////
// CHeaderCtrlEx
//
//BEGIN_MESSAGE_MAP(CHeaderCtrlEx1, CHeaderCtrl)
//END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHeaderCtrlEx message handlers
inline void CHeaderCtrlEx::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{

    CHeaderCtrl::DrawItem(lpDrawItemStruct);
        
    if (true)
        return;

    ASSERT(lpDrawItemStruct->CtlType == ODT_HEADER);

    HDITEM hdi;
    TCHAR  lpBuffer[256];

    hdi.mask = HDI_TEXT;
    hdi.pszText = lpBuffer;
    hdi.cchTextMax = sizeof(lpBuffer) - 1;

    GetItem(lpDrawItemStruct->itemID, &hdi);


    CDC* pDC;
    pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

    //THIS FONT IS ONLY FOR DRAWING AS LONG AS WE DON'T DO A SetFont(...)
    pDC->SelectObject(GetStockObject(DEFAULT_GUI_FONT));
    // Draw the button frame.
    ::DrawFrameControl(lpDrawItemStruct->hDC, 
                       &lpDrawItemStruct->rcItem, DFC_BUTTON, DFCS_BUTTONPUSH);


    UINT uFormat = DT_CENTER;
    //DRAW THE TEXT
    ::DrawText(lpDrawItemStruct->hDC, lpBuffer, wcslen(lpBuffer),
               &lpDrawItemStruct->rcItem, uFormat);

    pDC->SelectStockObject(SYSTEM_FONT);

}
////////////////////////////////////////////////////////////////////////////////
// Реализация расширения над таблицей

BEGIN_TEMPLATE_MESSAGE_MAP(CListCustomizing, CBaseList, CBaseList)
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CListCustomizing::OnNMCustomdraw)
END_MESSAGE_MAP()

//----------------------------------------------------------------------------//
template <typename CBaseList>
void CListCustomizing<CBaseList>::PreSubclassWindow()
{
    CBaseList::PreSubclassWindow();

    DWORD extendedListStyle = GetExtendedStyle();
    // Focus retangle is not painted properly without double-buffering
#if (_WIN32_WINNT >= 0x501)
    extendedListStyle |= LVS_EX_DOUBLEBUFFER;
#endif
    extendedListStyle |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP;

    // Убираем флаг показа выделения всегда
   // extendedListStyle &= ~LVS_SHOWSELALWAYS;

    SetExtendedStyle(extendedListStyle);

    EnableWindowTheme(GetSafeHwnd(), L"ListView", L"Explorer", NULL);

    ///////////////////////SET UP THE MULTILINE HEADER CONTROL
    m_NewHeaderFont.CreatePointFont(190, L"MS Serif");

    CHeaderCtrl* pHeader = NULL;
    pHeader= GetHeaderCtrl();

    if(pHeader==NULL)
        return;

    VERIFY(m_HeaderCtrl.SubclassWindow(pHeader->m_hWnd));	

    //A BIGGER FONT MAKES THE CONTROL BIGGER
    m_HeaderCtrl.SetFont(&m_NewHeaderFont);

    HDITEM hdItem;
    hdItem.mask = HDI_FORMAT;

    for(int i=0; i<m_HeaderCtrl.GetItemCount(); i++)
    {
        m_HeaderCtrl.GetItem(i,&hdItem);

        hdItem.fmt|= HDF_OWNERDRAW;

        m_HeaderCtrl.SetItem(i,&hdItem);
    }
}

template<typename CBaseList>
void CListCustomizing<CBaseList>::OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLVCUSTOMDRAW pNMCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);

    // Сначало надо определить текущую стадию
    switch (pNMCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        // если рисуется весь элемент целиком - запрашиваем получение сообщений
        // для каждого элемента списка.
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;

    case CDDS_ITEMPREPAINT:
        // если рисуется весь элемент списка целиком - запрашиваем получение сообщений
        // для каждого подэлемента списка.
        *pResult = CDRF_NOTIFYSUBITEMDRAW;
        break;

    case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
        {
            // Стадия, которая наступает перед отрисовкой каждого элемента списка.
            DWORD iItem = pNMCD->nmcd.dwItemSpec;
            DWORD iSubItem = pNMCD->iSubItem;

            if (GetItemState(iItem, LVIS_SELECTED) == LVIS_SELECTED)
            {
                // см https://docs.microsoft.com/ru-ru/windows/win32/controls/parts-and-states?redirectedfrom=MSDN
                HTHEME hTheme = OpenThemeData(m_hWnd, L"LISTVIEW");

                //pNMCD->clrText   = GetThemeSysColor(hTheme, COLOR_HIGHLIGHTTEXT);
                pNMCD->clrTextBk = GetThemeSysColor(hTheme, COLOR_HIGHLIGHT);

                // прекращаем работу с темой
                if (hTheme)
                    CloseThemeData(hTheme);
            }
           // ColumnInfo *pInfo = GetColumnInfo(iCol);

            pNMCD->clrTextBk = GetTextBkColor();

            // Уведомляем систему, чтобы она самостоятельно нарисовала элемент.
            *pResult = CDRF_DODEFAULT | CDRF_NEWFONT;
        }
        break;

    default:
        // Будем выполнять стандартную обработку для всех сообщений по умолчанию
        *pResult = CDRF_DODEFAULT;
        break;
    }
}
