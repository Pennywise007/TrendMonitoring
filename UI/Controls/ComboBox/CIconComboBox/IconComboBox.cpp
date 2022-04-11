#include "stdafx.h"
#include "IconComboBox.h"

CIconComboBox::CIconComboBox()
{
}

CIconComboBox::~CIconComboBox()
{
	m_ImageList.DeleteImageList();
}

BOOL CIconComboBox::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	dwStyle &= ~CBS_OWNERDRAWVARIABLE;
	dwStyle |= CBS_OWNERDRAWFIXED;

	return CComboBoxEx::Create(dwStyle, rect, pParentWnd, nID);
}

BOOL CIconComboBox::CreateEx(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	dwStyle &= ~CBS_OWNERDRAWVARIABLE;
	dwStyle |= CBS_OWNERDRAWFIXED;

	return CComboBoxEx::CreateEx(dwExStyle, dwStyle, rect, pParentWnd, nID);
}

void CIconComboBox::SetIconList(_In_ std::list<HICON> &IconList, _In_opt_ CSize IconSizes /*= CSize(15, 15)*/)
{
	m_ImageList.DeleteImageList();
	m_ImageList.Create(IconSizes.cx, IconSizes.cy, ILC_COLOR32, 1, 1);

	for (auto &it : IconList)
		m_ImageList.Add(it);

	CComboBoxEx::SetImageList(&m_ImageList);
}

int CIconComboBox::InsertItem(_In_ int iItemIndex, 
							  _In_ LPTSTR pszText, 
							  _In_ int iImageIndex, 
							  _In_opt_ int iSelectedImage /*= USE_IMAGE_INDEX*/, 
							  _In_opt_ int iIndent		  /*= 0*/)
{
	COMBOBOXEXITEM cbei;
	
	cbei.iItem		= iItemIndex;
	cbei.pszText	= pszText;
	cbei.cchTextMax = wcslen(pszText) * sizeof(WCHAR);
	cbei.iImage		= iImageIndex;
	cbei.iSelectedImage = iSelectedImage != USE_IMAGE_INDEX ? iSelectedImage : iImageIndex;
	cbei.iIndent	= iIndent;

	// Set the mask common to all items.
	cbei.mask		= CBEIF_TEXT | CBEIF_INDENT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
	
	return CComboBoxEx::InsertItem(&cbei);
}

int CIconComboBox::InsertItem(_In_ COMBOBOXEXITEM *citem)
{
	return CComboBoxEx::InsertItem(citem);
}

void CIconComboBox::RecreateCtrl(_In_opt_ int NewHeight /*= -1*/)
{
	CRect Rect;
	CComboBoxEx::GetWindowRect(Rect);
	CComboBoxEx::GetParent()->ScreenToClient(Rect);

	UINT ID			= CComboBoxEx::GetDlgCtrlID();
	UINT Style		= CComboBoxEx::GetStyle();
	Style &= ~CBS_OWNERDRAWVARIABLE;
	Style |= CBS_OWNERDRAWFIXED;
	UINT StyleEx	= CComboBoxEx::GetExStyle();
	CWnd *pParrent	= CComboBoxEx::GetParent();

	CComboBoxEx::DestroyWindow();

	INITCOMMONCONTROLSEX stuctMy;
	stuctMy.dwSize = sizeof(INITCOMMONCONTROLSEX);
	stuctMy.dwICC = ICC_USEREX_CLASSES;
	InitCommonControlsEx(&stuctMy);

	m_hWnd = CreateWindowExW(StyleEx, WC_COMBOBOXEX, WC_COMBOBOXEX, Style,
							 Rect.left, Rect.top, 
							 Rect.Width(), (NewHeight == -1 ? Rect.Height() : NewHeight), 
							 pParrent->m_hWnd, (HMENU)ID, 0, 0);
}
