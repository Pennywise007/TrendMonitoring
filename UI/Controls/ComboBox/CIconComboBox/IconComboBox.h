#pragma once
#include "afxcmn.h"
#include <list>
//*************************************************************************************************
/*	Описание: комбинированный список с иконками, контрол необходимо создавать динамически, 
	высота выпадающего списка зависит от заданной высоты контрола 
	
!!!	ИЛИ ЖЕ СОЗДАТЬ НА ФОРМЕ ПРОСТОЙ COMBOBOX НАСТРОИТЬ ЕГО И В INITDIALOG ВЫЗВАТЬ RecreateCtrl()!!!	*/
//*************************************************************************************************
//	Возможные интересные флаги : WS_VISIBLE, WS_CHILD, CBS_DROPDOWNLIST или CBS_DROPDOWN или CBS_SIMPLE
//*************************************************************************************************
/*	Пример работы с контролом

	// экземпляр контрола
	CIconComboBox m_ImageCombo;

	// список иконок привязанных к контролу
	std::list<HICON> m_IconList;
	m_IconList.push_back(NewIcon1);
	m_IconList.push_back(NewIcon2);

	// создание контрола
	m_ImageCombo.Create(WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, CRect(0, 50, 300, 250), this, 123);
	
	// задание списка отображаемых иконок
	m_ImageCombo.SetIconList(m_IconList);

	// заполнение элементов
	m_ImageCombo.InsertItem(0, L"Элемент 1", 0, 0, 0);
	m_ImageCombo.InsertItem(1, L"Элемент 2", 1, 1, 1);*/
//*************************************************************************************************
#define USE_IMAGE_INDEX -1		// использование по умолчанию iIconIndex
//*************************************************************************************************
class CIconComboBox : public CComboBoxEx
{
public://******************************************************************************************
	CIconComboBox();
	~CIconComboBox();
public://******************************************************************************************
	// задания перечня иконок, с заданием размеров отображаемых иконок
	virtual void SetIconList(_In_ std::list<HICON> &IconList, _In_opt_ CSize IconSizes = CSize(15, 15));
	//*********************************************************************************************
	// добавление строки в контрол
	// @param iItemIndex		- индекс добавляемого элемента
	// @param pszText			- добавляемый текст
	// @param iIconIndex		- индекс отображаемой иконки
	// @param iSelectedImage	- индекс иконки при выборе этой строчки(по умолчанию iIconIndex)
	// @param iIndent			- отступ слева (по умолчанию 0)
	virtual int InsertItem(_In_ int iItemIndex, 
						   _In_ LPTSTR pszText, 
						   _In_ int iImageIndex,
						   _In_opt_ int iSelectedImage	= USE_IMAGE_INDEX,
						   _In_opt_ int iIndent			= 0);
	// добавление строки в контрол
	virtual int InsertItem(_In_ COMBOBOXEXITEM *citem);
	//*********************************************************************************************
	// пересоздание контрола, NewHeight - новая высота контрола, если не задано то не изменяется
	virtual void RecreateCtrl(_In_opt_ int NewHeight = -1);
public://******************************************************************************************
	virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
	virtual BOOL CreateEx(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
protected://***************************************************************************************
	CImageList m_ImageList;	// список отображаемых иконок
};	//*********************************************************************************************