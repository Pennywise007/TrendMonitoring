#include "stdafx.h"

#include <float.h>
#include <locale>
#include <sstream>

#include "CEditBase.h"

BEGIN_MESSAGE_MAP(CEditBase, CEdit)
	ON_WM_CREATE()
	ON_WM_CTLCOLOR_REFLECT()
	ON_MESSAGE(WM_PASTE, &CEditBase::OnPaste)
END_MESSAGE_MAP()

CEditBase::CEditBase(_In_opt_ bool bUseNumbersOnly /*= false*/)
	: m_bUseDefaultColors		(true)
	, m_colorBk					(RGB(255, 255, 255))
	, m_colorText				(GetSysColor(COLOR_BTNTEXT))
	, m_bUseNumbersOnly			(bUseNumbersOnly)
	, m_bUsePositivesDigitsOnly	(false)
	, m_bUseOnlyIntegers		(false)
	, m_ValuesRange				(std::make_pair(-(FLT_MAX), FLT_MAX))
	, m_bUseLimits				(false)
{	
	m_ptooltip = new CToolTipCtrl();
	m_brushBk = CreateSolidBrush(m_colorBk);
}

CEditBase::~CEditBase()
{
	if (m_brushBk != NULL)
		DeleteObject(m_brushBk);

	if (m_ptooltip)
	{ 
		delete m_ptooltip;
		m_ptooltip = nullptr;
	}
}

void CEditBase::SetDefaultColors(_In_ bool bUseDefault /*= true*/)
{
	if (m_bUseDefaultColors != bUseDefault)
	{
		m_bUseDefaultColors = bUseDefault;
		CEdit::Invalidate();
	}
}

COLORREF CEditBase::GetBkColor()
{
	return m_colorBk;
}

void CEditBase::SetBkColor(const COLORREF color)
{
	m_bUseDefaultColors = false;
	if (m_colorBk != color)
	{
		m_colorBk = color;

		if (m_brushBk != NULL)
			DeleteObject(m_brushBk);
		m_brushBk = CreateSolidBrush(m_colorBk);
	}
	CEdit::Invalidate();
}

COLORREF CEditBase::GetTextColor()
{
	return m_colorText;
}

void CEditBase::SetTextColor(const COLORREF color)
{
	m_bUseDefaultColors = false;
	if (m_colorText != color)
		m_colorText = color;
	CEdit::Invalidate();
}

HBRUSH CEditBase::CtlColor(CDC* pDC, UINT /*nCtlColor*/)
{
	if (m_bUseDefaultColors)
		return NULL;

	pDC->SetBkColor(m_colorBk);    // change the background
	pDC->SetTextColor(m_colorText);  // change the text color

	return m_brushBk;
}

BOOL CEditBase::PreTranslateMessage(MSG* pMsg)
{
	m_ptooltip->RelayEvent(pMsg);

	if (CEdit::GetStyle() & ES_NUMBER)
	{
		if (m_bUseNumbersOnly)
			CEdit::ModifyStyle(ES_NUMBER, 0);
	}

	if (m_bUseNumbersOnly)
	{
		// проверяем не используются ли комбинации клавиш
		if (GetKeyState(VK_CONTROL) & 0x8000)
			return CEdit::PreTranslateMessage(pMsg);			

		switch (pMsg->message)
		{
			case WM_CHAR:
			{
				// virtual key code of the key pressed
				if (pMsg->wParam != 8)	// забой
				{
					CString EditText;
					CEdit::GetWindowText(EditText);

					DWORD	dwSelStart = 0,		// selection starting position
							dwSelEnd = 0;		// selection ending position

					// retrieve selection range
					CEdit::SendMessage(EM_GETSEL, (WPARAM)&dwSelStart, (LPARAM)&dwSelEnd);

					EditText = EditText.Left(dwSelStart) + (wchar_t)pMsg->wParam + EditText.Right(EditText.GetLength() - dwSelEnd);
					if (CheckNumericString(EditText))
						HideBubble();
					else
						return TRUE;
				}
				else
					HideBubble();

				break;
			}
			default:
				break;
		}
	}

	return CEdit::PreTranslateMessage(pMsg);
}

LRESULT CEditBase::ShowBubble(_In_ CString sTitle, _In_ CString sText, _In_opt_ INT Icon /*= TTI_ERROR*/)
{
	EDITBALLOONTIP ebt;

	ebt.cbStruct = sizeof(EDITBALLOONTIP);
	ebt.pszText  = sText;
	ebt.pszTitle = sTitle;
	ebt.ttiIcon  = Icon;    // tooltip icon

	return CEdit::SendMessage(EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
}

LRESULT CEditBase::HideBubble()
{
	return CEdit::SendMessage(EM_HIDEBALLOONTIP, 0, 0);
}

void CEditBase::PreSubclassWindow()
{
	m_ptooltip->Create(this);
	m_ptooltip->Activate(TRUE);

	CEdit::PreSubclassWindow();
}

BOOL CEditBase::ShowError(_In_opt_ ErrorType Type /*= ErrorType::BAD_INPUT_VAL*/)
{
	// отображаемые строки при вводе неправильного текста
#ifdef CEDIT_BASE_USE_TRANSLATE
	const static CString ErrorTitle			(TranslateString(L"Недопустимый символ"));
	const static CString ErrorTextNumbers	(TranslateString(L"Здесь можно ввести только цифры и числа с плавающей точкой"));
	const static CString ErrorTextIntegers	(TranslateString(L"Здесь можно ввести только целые числа"));
	const static CString ErrorTextLimits	(TranslateString(L"Недопустимое число, вы можете задать число"));
	const static CString ErrorTextLimitsFrom(TranslateString(L"от"));
	const static CString ErrorTextLimitsTo	(TranslateString(L"до"));
#else
	const static CString ErrorTitle			(L"Недопустимый символ");
	const static CString ErrorTextNumbers	(L"Здесь можно ввести только цифры и числа с плавающей точкой");
	const static CString ErrorTextIntegers	(L"Здесь можно ввести только целые числа");
	const static CString ErrorTextLimits	(L"Недопустимое число, вы можете задать число");
	const static CString ErrorTextLimitsFrom(L"от");
	const static CString ErrorTextLimitsTo	(L"до");
#endif

	CString ErrorStr(_T(""));
	switch (Type)
	{
		case ErrorType::BAD_INPUT_VAL:
			ErrorStr = m_bUseOnlyIntegers ? ErrorTextIntegers : ErrorTextNumbers;
			break;
		case ErrorType::NOT_FIT_INTO_LIMITS:
			ErrorStr.Format(L"%s: %s %g %s %g.",
							ErrorTextLimits.GetString(),
							ErrorTextLimitsFrom.GetString(),
							m_ValuesRange.first,
							ErrorTextLimitsTo.GetString(),
							m_ValuesRange.second);
			break;
		default:
			break;
	}
	ShowBubble(ErrorTitle, ErrorStr);

	return FALSE;
}

CString GetClipboardText()
{
	// Try opening the clipboard
	if (!OpenClipboard(nullptr))
		return CString(); // error

		// Get handle of clipboard object for ANSI text
		HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData == nullptr)
		return CString(); // error

		// Lock the handle to get the actual text pointer
		char * pszText = static_cast<char*>(GlobalLock(hData));
	if (pszText == nullptr)
		return CString(); // error

	// Save text in a string class instance
	CString text(pszText);

	// Release the lock
	GlobalUnlock(hData);

	// Release the clipboard
	CloseClipboard();

	return text;
}

int CountElements(_In_ const CString& Str, _In_ wchar_t Char)
{
	int Count(0);
	for (int i = Str.GetLength() - 1; i >= 0; i--)
	{
		if (Str[i] == Char)
			Count++;
	}
	return Count;
}

BOOL CEditBase::CheckNumericString(_In_ CString Str)
{
	if (Str.IsEmpty() || !m_bUseNumbersOnly)
		return TRUE;

    const auto pointSymbol = std::use_facet<std::numpunct<wchar_t>>(std::wostringstream().getloc()).decimal_point();

	// точек не должно быть больше 1
	if (CountElements(Str, pointSymbol) > 1)
		return ShowError();

	// букв "е" или "Е" не должно быть бьльше 1
	if (CountElements(Str, L'e') + CountElements(Str, L'E') > 1)
		return ShowError();

	// знаков отрицания не должно быть больше 2
	if (CountElements(Str, L'-') > 2)
		return ShowError();

	// проходим по всему тексту проверяя каждый символ
	for (auto i = Str.GetLength() - 1; i >= 0; --i)
	{
        const auto ch = Str[i];
		if (!iswdigit(ch))	// если вводим не цифры
		{
            if (ch == pointSymbol)
            {
                if (m_bUseOnlyIntegers)
                    return ShowError();
                else
                    continue;
            }

			switch (ch)
			{
				case 45:	// минус
					// минус может быть только в начале или перед E/e
					if ((m_bUsePositivesDigitsOnly && i == 0) || ((i != 0) && (Str[i - 1] != L'e' && Str[i - 1] != L'E')))
						return ShowError();
					break;
				case 101:	// "e" английская для 1.2e-3
				case 69:	// "E" английская для 1.2E-3
					// E/e не могут стоять с начала
					if (i == 0)
						return ShowError();
					break;
				default:
					return ShowError();
			}
		}
	}

	if (m_bUseLimits)
	{
		const float CurVal = (float)_wtof(Str);
		if (CurVal < m_ValuesRange.first || CurVal > m_ValuesRange.second)
			return ShowError(ErrorType::NOT_FIT_INTO_LIMITS);
	}

	return TRUE;
}

afx_msg LRESULT CEditBase::OnPaste(WPARAM wParam, LPARAM lParam)
{
	UNUSED_ALWAYS(wParam);
	UNUSED_ALWAYS(lParam);

	CString	strClipBrdText,		// text available on clipboard
			strCtrlText;		// text in the edit control
	DWORD	dwSelStart = 0,		// selection range starting position
			dwSelEnd = 0;		// selection range ending position

	strClipBrdText = GetClipboardText();
	// no (valid) clipboard data available?
	if (strClipBrdText.IsEmpty())
		return (0L);

	// get control's current text and selection range
	CEdit::GetWindowText(strCtrlText);
	CEdit::SendMessage(EM_GETSEL, (WPARAM)&dwSelStart, (LPARAM)&dwSelEnd);
	
	// формируем новую строку вставляя в нее текст из буфера
	strCtrlText = strCtrlText.Left(dwSelStart) + strClipBrdText + strCtrlText.Right(strCtrlText.GetLength() - dwSelEnd);
	if (m_bUseNumbersOnly)
	{
		if (!CheckNumericString(strCtrlText))
			return 0;
	}
	
	CEdit::SetWindowText(strCtrlText);
	CEdit::SetSel(int(dwSelStart + strClipBrdText.GetLength()), int(dwSelStart + strClipBrdText.GetLength()));	// position caret, scroll if necessary!
	
	return 0;
}

void CEditBase::SetTooltip(_In_opt_ LPCTSTR lpszToolTipText /*= nullptr*/)
{
	if (lpszToolTipText == nullptr)
		m_ptooltip->DelTool(this);
	else
		m_ptooltip->AddTool(this, lpszToolTipText);
}

void CEditBase::UsePositiveDigitsOnly(_In_opt_ bool bUsePositiveDigitsOnly /*= true*/)
{
	m_bUsePositivesDigitsOnly = bUsePositiveDigitsOnly;

	SetNumbersSettings(bUsePositiveDigitsOnly);
}

void CEditBase::SetUseOnlyNumbers(_In_opt_ bool bUseNumbersOnly /*= true*/)
{
	m_bUseNumbersOnly = bUseNumbersOnly;
	
	SetNumbersSettings(bUseNumbersOnly);
}

void CEditBase::SetUseOnlyIntegersValue(_In_opt_ bool bUseIntegersOnly /*= true*/)
{
	m_bUseOnlyIntegers = bUseIntegersOnly;
	SetNumbersSettings(bUseIntegersOnly);
}

void CEditBase::SetNumbersSettings(_In_ bool NewState)
{
	if (NewState)
	{
		m_bUseNumbersOnly = true;

		CString Text;
		CEdit::GetWindowText(Text);
		CheckNumericString(Text);
	}
}

void CEditBase::SetMinMaxLimits(_In_ float MinVal, _In_ float MaxVal)
{
	m_ValuesRange = std::make_pair(MinVal, MaxVal);

	if (m_bUseLimits)
	{
		CString Text;
		CEdit::GetWindowText(Text);
		CheckNumericString(Text);
	}
}
