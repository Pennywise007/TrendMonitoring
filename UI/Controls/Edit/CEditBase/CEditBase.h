#pragma once
#include "afxwin.h"
#include <utility>
//************************************************************************************************
#ifdef CEDIT_BASE_USE_TRANSLATE
#include <Dialog_ZET/Translate.h>
#endif // CEDIT_BASE_USE_TRANSLATE
/*************************************************************************************************
	Данный контрол позволяет расширить функционал CEdit

	1) возможность отобразить всплывающее сообщение (функция ShowBubble)
	2) установка ToolTip
	3) установка цветов текста и фона
	4) доработка использования контрола для работы с числами с плавающей точкой
		задав в конструкторе CEditBase(true) или функция FloatNumbersOnly(true)
**************************************************************************************************/
class CEditBase : public CEdit
{
public:	//*****************************************************************************************
	CEditBase(_In_opt_ bool bUseNumbersOnly = false);	// стандартный конструктор
	~CEditBase();
public:	//*****************************************************************************************
	// отображение высплывающих сообщений
	LRESULT ShowBubble(_In_ CString sTitle, _In_ CString sText, _In_opt_ INT Icon = TTI_ERROR);
	LRESULT HideBubble();		// спрятать всплывающее сообщение
	//*********************************************************************************************
	// установка контрола на работу с числами
	virtual void UsePositiveDigitsOnly(_In_opt_ bool bUsePositiveDigitsOnly = true);
	void SetUseOnlyNumbers(_In_opt_ bool bUseNumbersOnly = true);
	void SetUseOnlyIntegersValue(_In_opt_ bool bUseIntegersOnly = true);
	//*********************************************************************************************
	// ограничения контрола по максимальному и минимальному значению
	void SetUseCtrlLimits(_In_opt_ bool bUseLimits) { m_bUseLimits = bUseLimits; }
	void SetMinMaxLimits (_In_ float MinVal, _In_ float MaxVal);
	//*********************************************************************************************
	void SetTooltip(_In_opt_ LPCTSTR lpszToolTipText = nullptr); // lpszToolTip == nullptr: disable tooltip
	CToolTipCtrl * GetToolTipCtrl() { return m_ptooltip; }
	//*********************************************************************************************
#pragma region Устновка цветов
	// определяем использовать стандартные  или заданые цвета
	void SetDefaultColors(_In_ bool bUseDefault = true);
	//*********************************************************************************************	
	COLORREF GetBkColor();						// Цвет фона. При его задании фон делается непрозрачным
	void SetBkColor(const COLORREF color);		// Цвет фона. При его задании фон делается непрозрачным
	//*********************************************************************************************	
	COLORREF GetTextColor();					// Цвет текста
	void SetTextColor(const COLORREF color);	// Цвет текста
#pragma endregion
private://*****************************************************************************************
	enum ErrorType
	{
		BAD_INPUT_VAL = 0,
		NOT_FIT_INTO_LIMITS
	};
	BOOL ShowError(_In_opt_ ErrorType Type = ErrorType::BAD_INPUT_VAL);
	// проверка строки на то содержит она число или нет
	BOOL CheckNumericString(_In_ CString Str);
	void SetNumbersSettings(_In_ bool NewState);
protected://*****************************************************************************************
	// цвета
	bool m_bUseDefaultColors;					// Использование стандартных цветов
	COLORREF m_colorBk;							// Цвет фона по умолчанию белый
	COLORREF m_colorText;						// Цвет текста по умолчанию чёрный	
	HBRUSH m_brushBk;							// Кисти для рисования фона
	//*********************************************************************************************
	bool m_bUseNumbersOnly;						// флаг что будут использоваться только цифры
	bool m_bUseOnlyIntegers;					// флаг использования только целых чисел
	bool m_bUsePositivesDigitsOnly;				// флаг что будут использоваться только положительные цифры
	//*********************************************************************************************
	CToolTipCtrl *m_ptooltip;					// всплывающая подсказка
	//*********************************************************************************************
	bool m_bUseLimits;							// флаг использования ограничения на значения контрола
	std::pair<float, float> m_ValuesRange;		// минимальое и максимальное значение контрола
protected://***************************************************************************************
	DECLARE_MESSAGE_MAP()
public://******************************************************************************************
	virtual void PreSubclassWindow();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg HBRUSH CtlColor(CDC* /*pDC*/, UINT /*nCtlColor*/);
	afx_msg LRESULT OnPaste(WPARAM wParam, LPARAM lParam);
};	//*********************************************************************************************

