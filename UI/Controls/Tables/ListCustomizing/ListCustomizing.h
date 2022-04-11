#pragma once

#include "afxcmn.h"

class CHeaderCtrlEx : public CHeaderCtrl
{
    // Construction
public:
    CHeaderCtrlEx() = default;
    virtual ~CHeaderCtrlEx() = default;

   // DECLARE_MESSAGE_MAP()

    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    DECLARE_MESSAGE_MAP()
    afx_msg void OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
};

//****************************************************************************//
/*
    Расширенный класс над списком, может использоваться как виджет для таблицы

    Позволяет:
        1. Сделать многострочные заголовки колонок
        2. Задавать цвета ячейкам
*/
template <typename CBaseList = CListCtrl>
class CListCustomizing : public CBaseList
{
public:
    CListCustomizing() = default;

public:


protected://********************************************************************
    DECLARE_MESSAGE_MAP()

    virtual void PreSubclassWindow() override;

    afx_msg void OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult);

    CFont m_NewHeaderFont;
    CHeaderCtrlEx m_HeaderCtrl;
};

// т.к класс шаблонный прячем реализацию в другой хедер
#include "ListCustomizingImpl.h"