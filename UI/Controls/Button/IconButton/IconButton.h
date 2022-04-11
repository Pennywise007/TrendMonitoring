#pragma once
#include "afxwin.h"
//*************************************************************************************************
#define USE_ICON_SIZE	-1
//*************************************************************************************************
// перечисление видов привязки иконки на контроле
enum Aligments
{
	LeftCenter = 0,	// слева по центру
	CenterTop,		// по центру сверху
	CenterCenter,	// по центру
	CenterBottom,	// по центру снизу
	RightCenter		// справа по центру
};
//*************************************************************************************************
class CIconButton :	public CButton
{
public:	//*****************************************************************************************
	CIconButton();
	~CIconButton();
public:	//*****************************************************************************************
	// загрузка изображения для кнопки, Aligments - флаг привязки изображения относительно кнопки
	// IconWidth  - ширина иконки, если USE_ICON_SIZE то будет использована ширина иконки
	// IconHeight - высота иконки, если USE_ICON_SIZE то будет использована высота иконки
	HICON SetIcon(_In_ UINT uiImageID,	_In_opt_ Aligments Aligment = CenterTop,
				  _In_opt_ int IconWidth = USE_ICON_SIZE, _In_opt_ int IconHeight = USE_ICON_SIZE);
	HICON SetIcon(_In_ HICON hIcon,		_In_opt_ Aligments Aligment = CenterTop,
				  _In_opt_ int IconWidth = USE_ICON_SIZE, _In_opt_ int IconHeight = USE_ICON_SIZE);
	// загрузка изображения для кнопки, Aligments - флаг привязки изображения относительно кнопки
	// ColorMask - маска загружаемая с BITMAP
	// bUseColorMask - флаг использования маски
	// IconWidth  - ширина иконки, если USE_ICON_SIZE то будет использована ширина иконки
	// IconHeight - высота иконки, если USE_ICON_SIZE то будет использована высота иконки
	CBitmap* SetBitmap(_In_ UINT uiBitmapID, _In_opt_ bool bUseColorMask = false,
					   _In_opt_ COLORREF ColorMask = RGB(0, 0, 0), _In_opt_ Aligments Aligment = CenterTop,
					   _In_opt_ int BitmapWidth = USE_ICON_SIZE, _In_opt_ int BitmapHeight = USE_ICON_SIZE);
	CBitmap* SetBitmap(_In_ CBitmap* hBitmap, _In_opt_ bool bUseColorMask = false,
					   _In_opt_ COLORREF ColorMask = RGB(0, 0, 0), _In_opt_ Aligments Aligment = CenterTop,
					   _In_opt_ int BitmapWidth = USE_ICON_SIZE, _In_opt_ int BitmapHeight = USE_ICON_SIZE);
	//*********************************************************************************************
	void SetImageOffset	(_In_ long lOffset);		// установка отступа изображения от границ кнопки
	void SetTextColor	(_In_ COLORREF Color);		// установка цвета текста
	void SetBkColor		(_In_ COLORREF BkColor);	// установка фона для кнопки
	// установка флага использования стандартного фона или заданного пользователем
	void UseDefaultBkColor(_In_opt_ bool bUseStandart = true);
	//*********************************************************************************************
	void SetTooltip(_In_ CString Tooltip);
protected://***************************************************************************************
	void RepositionItems();		// масштабируем рабочие области кнопки
protected://***************************************************************************************
	bool m_bImageLoad;			// флаг того что иконка загружена
	long m_lOffset;				// отступ от границ контрола при отрисовке иконки
	CSize m_IconSize;			// размер иконки
	CPoint m_IconPos;			// местоположение верхнего левого угла иконки
	Aligments m_Aligment;		// привязка кнопки
	CImageList m_ImageList;		// иконка
	//*********************************************************************************************
	bool m_bUseCustomBkColor;	// флаг использования пользовательского фона для кнокпи
	COLORREF m_TextColor;		// цвет текста
	COLORREF m_BkColor;			// цвет фона
	CString m_ButtonText;		// текст на кнопке
	CRect m_TextRect;			// размер текста
	bool m_bNeedCalcTextPos;	// флаг необходимости пересчитать размеры области под текст
	//*********************************************************************************************
	CToolTipCtrl *m_ptooltip;	// тултип
public:	//*****************************************************************************************
	DECLARE_MESSAGE_MAP()
	virtual void PreSubclassWindow();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};	//*********************************************************************************************