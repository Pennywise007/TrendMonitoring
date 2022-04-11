#pragma once
#include "afxcmn.h"
#include <list>
//*************************************************************************************************
/*	��������: ��������������� ������ � ��������, ������� ���������� ��������� �����������, 
	������ ����������� ������ ������� �� �������� ������ �������� 
	
!!!	��� �� ������� �� ����� ������� COMBOBOX ��������� ��� � � INITDIALOG ������� RecreateCtrl()!!!	*/
//*************************************************************************************************
//	��������� ���������� ����� : WS_VISIBLE, WS_CHILD, CBS_DROPDOWNLIST ��� CBS_DROPDOWN ��� CBS_SIMPLE
//*************************************************************************************************
/*	������ ������ � ���������

	// ��������� ��������
	CIconComboBox m_ImageCombo;

	// ������ ������ ����������� � ��������
	std::list<HICON> m_IconList;
	m_IconList.push_back(NewIcon1);
	m_IconList.push_back(NewIcon2);

	// �������� ��������
	m_ImageCombo.Create(WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, CRect(0, 50, 300, 250), this, 123);
	
	// ������� ������ ������������ ������
	m_ImageCombo.SetIconList(m_IconList);

	// ���������� ���������
	m_ImageCombo.InsertItem(0, L"������� 1", 0, 0, 0);
	m_ImageCombo.InsertItem(1, L"������� 2", 1, 1, 1);*/
//*************************************************************************************************
#define USE_IMAGE_INDEX -1		// ������������� �� ��������� iIconIndex
//*************************************************************************************************
class CIconComboBox : public CComboBoxEx
{
public://******************************************************************************************
	CIconComboBox();
	~CIconComboBox();
public://******************************************************************************************
	// ������� ������� ������, � �������� �������� ������������ ������
	virtual void SetIconList(_In_ std::list<HICON> &IconList, _In_opt_ CSize IconSizes = CSize(15, 15));
	//*********************************************************************************************
	// ���������� ������ � �������
	// @param iItemIndex		- ������ ������������ ��������
	// @param pszText			- ����������� �����
	// @param iIconIndex		- ������ ������������ ������
	// @param iSelectedImage	- ������ ������ ��� ������ ���� �������(�� ��������� iIconIndex)
	// @param iIndent			- ������ ����� (�� ��������� 0)
	virtual int InsertItem(_In_ int iItemIndex, 
						   _In_ LPTSTR pszText, 
						   _In_ int iImageIndex,
						   _In_opt_ int iSelectedImage	= USE_IMAGE_INDEX,
						   _In_opt_ int iIndent			= 0);
	// ���������� ������ � �������
	virtual int InsertItem(_In_ COMBOBOXEXITEM *citem);
	//*********************************************************************************************
	// ������������ ��������, NewHeight - ����� ������ ��������, ���� �� ������ �� �� ����������
	virtual void RecreateCtrl(_In_opt_ int NewHeight = -1);
public://******************************************************************************************
	virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
	virtual BOOL CreateEx(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
protected://***************************************************************************************
	CImageList m_ImageList;	// ������ ������������ ������
};	//*********************************************************************************************