#pragma once

/* �������� ��������
	CColumnTreeCtrl m_ListCtrl.Create(WS_CHILD | WS_BORDER, Rect, this, IDD_ZETSEISMOGRAPH_DIALOG);

	������������� ��� ������� �������
	UINT uTreeStyle = TVS_HASBUTTONS | TVS_FULLROWSELECT | LVS_REPORT;
	m_ListCtrl.ModifyStyle(0, uTreeStyle);

	������������� ��� ������
	UINT uTreeStyle = TVS_HASBUTTONS | TVS_FULLROWSELECT | LVS_REPORT | TVS_EX_DOUBLEBUFFER | TVS_CHECKBOXES;
	m_ListCtrl.ModifyStyle(0, uTreeStyle);


	�����:
	TVS_CHECKBOXES - � ������� �������� ���� ������� � ���� �������� ������� ������ �����������/���������� ���
	TVS_EX_DOUBLEBUFFER - ��������� ���������� ������
	TVS_FULLROWSELECT - ��������� ���� �������
*/
//	��� ������������� ���������� ������ ���������� �������� #define _USE_HORSCALE
//	��� ������������� ���������� ��������� (ProgressCtrlEx) ���������� �������� #define _USE_PROGRESSCTRLEX

#include <stdint.h>
#include "CheckBoxedHeaderCtrl.h"
#include <vector>
#include <list>

#ifdef _USE_HORSCALE
#include <Grafic1\gdiplushorscalectrl1.h>
#endif // USE_HORSCALE

#ifdef _USE_PROGRESSCTRLEX
#include <ProgressCtrlEx\ProgressCtrlEx.h>
#endif // USE_HORSCALE

// ��������� �� ��������
// WPARAM wParam - HTREEITEM � ������� ��������� ���������
// LPARAM lParam - ���������
#define WM_CHANGE_COLOR				WM_USER + 1		// ��������� ��� ���� ���������	� �������� ��������� - COLORREF
#define WM_CHANGE_CHECK				WM_USER + 2		// ��������� ��� ���������� ��������� CheckBoxa, �������� - BOOL
#define WM_BUTTON_PRESSED			WM_USER + 3		// ��������� ����� ��������� ������� �� ������ � ������� lParam - ������ ������

#define MAX_COLUMN_COUNT			16		// change this value if you need more than 16 columns
enum ItemType
{
	// �������� �� ������� �������� � ������������ ������ �����
	ITEM_TEXT = 0,
	ITEM_COLORPICKER, 
#ifdef _USE_HORSCALE
	ITEM_HORSCALE,
#endif // USE_HORSCALE
	ITEM_BUTTON,
	ITEM_PROGRESSBAR
};

enum ColumnSortType
{
	CLMNS_NOSORT = -1,
	CLMNS_STRING = 0,
	CLMNS_NUMERIC,
};

//#define _OWNER_DRAWN_TREE  // comment this line if you want to use standard drawing code
#ifdef _OWNER_DRAWN_TREE
#ifndef IDB_TREEBTNS
	#error You should insert IDB_TREEBTNS bitmap to project resources. See control documentation for more info.
#endif //IDB_TREEBTNS
#endif //_OWNER_DRAWN_TREE

typedef struct _CTVHITTESTINFO { 
  POINT pt; 
  UINT flags; 
  HTREEITEM hItem; 
  int iSubItem;
} CTVHITTESTINFO;

namespace NameSpaceID
{
	static int g_CurrentID = 36000;									// �������������� ��� ������� ��������
}

class CCustomTreeChildCtrl : public CTreeCtrl
{
	friend class CColumnTreeCtrl;

	DECLARE_DYNAMIC(CCustomTreeChildCtrl)

public:

	/*
	 *  Construction/destruction
	 */
	
	
	CCustomTreeChildCtrl();
	virtual ~CCustomTreeChildCtrl();

	/*
	 * Operations
	 */
	
	BOOL GetBkImage(LVBKIMAGE* plvbkImage) const;
	BOOL SetBkImage(LVBKIMAGE* plvbkImage);

public:
	struct ColumnData
	{
		CString			Name;			// �������� �������
		ColumnSortType	SortType;		// ��� ����������
		int	SortPriority;				// 0 - �� ���������	/ 1 - �� ����������� / -1 - �� ��������
		ColumnData()
			: Name			(_T(""))
			, SortType		(CLMNS_NOSORT)
			, SortPriority	(0)
		{}
	};

	struct CellData
	{
		BOOL			bExpanded;
		BOOL			bSelected;
		CWnd *			ControlParrent;
		INT				Index;			// ������ � ��������� 
		CWnd*			ControlCWnd;	// ���������� ������
		CRect			ControlRect;	// ������� �������������� ��������
		HTREEITEM		ParrenthItem;	// ���������� ��������
		HTREEITEM       DefaulthItem;	// ���������� ������� ��� ��� �������� ������
		HTREEITEM       CurrenthItem;	// ������� ����������
		CString			Name;			// ����� � ������
		ItemType		ControlType;	// ��� ��������
		COLORREF		Color;			// ���� (���� ��� �������� CLMN_COLORPICKER)
		CellData(INT _Index, CString _Name, HTREEITEM NewItem, HTREEITEM Parrent)
			: bExpanded		(FALSE)
			, bSelected		(FALSE)
			, Index			(_Index)
			, ParrenthItem	(Parrent)
			, DefaulthItem	(NewItem)
			, CurrenthItem	(NewItem)
			, Name			(_Name)
			, ControlType	(ITEM_TEXT)
			, Color			(RGB(255, 255, 255))
			, ControlParrent(NULL)
			, ControlCWnd	(nullptr)
		{
			ControlRect.SetRectEmpty();
		}
		CellData()
			: bExpanded		(FALSE)
			, bSelected     (FALSE)
			, Index			(NULL)
			, ParrenthItem	(NULL)
			, DefaulthItem	(NULL)
			, CurrenthItem	(NULL)
			, Name			(_T(""))
			, ControlType	(ITEM_TEXT)
			, Color			(RGB(255, 255, 255))
			, ControlParrent(NULL)
			, ControlCWnd	(nullptr)
		{
			ControlRect.SetRectEmpty();
		}
		~CellData()
		{
			DeleteControl();
		}

		CellData(const CellData & val)
			: bExpanded		(val.bExpanded)
			, bSelected		(val.bSelected)
			, ParrenthItem	(val.ParrenthItem)
			, DefaulthItem	(val.DefaulthItem)
			, CurrenthItem	(val.CurrenthItem)
			, Name			(val.Name)
			, Color			(val.Color)
			, Index			(val.Index)
			, ControlParrent(val.ControlParrent)
			, ControlCWnd	(nullptr)
			, ControlRect	(val.ControlRect)
			, ControlType	(val.ControlType)
		{
			CreateControl(val.ControlParrent);
		}

		CellData operator = (const CellData& Val)
		{
			bSelected		= Val.bSelected;
			ParrenthItem	= Val.ParrenthItem;
			DefaulthItem	= Val.DefaulthItem;
			CurrenthItem	= Val.CurrenthItem;

			ChangeStruct(Val.Index, Val.Name, Val.ControlType, Val.Color, Val.ControlParrent);
			MoveWindow(Val.ControlRect);
			
			return *this;
		}
		// �������� ��������
		void CreateControl(CWnd* Parent)
		{
			ControlParrent = Parent;
			switch (ControlType)
			{
				case ITEM_BUTTON:
				{
					ControlCWnd = new CMyButton;
					((CMyButton*)ControlCWnd)->Create(Name, WS_CHILD | WS_VISIBLE, ControlRect, Parent, NameSpaceID::g_CurrentID++);
					((CMyButton*)ControlCWnd)->m_ButtonID = Index;
					ControlCWnd->SetFont(Parent->GetFont());
					break;
				}
#ifdef _USE_HORSCALE
				case ITEM_HORSCALE:
				{
					ControlCWnd = new CGdiplushorscalectrl1;
					((CGdiplushorscalectrl1*)ControlCWnd)->Create(Name, WS_CHILD | WS_VISIBLE, ControlRect, Parent, NameSpaceID::g_CurrentID++);
					ControlCWnd->SetFont(Parent->GetFont());
					break;
				}
#endif // USE_HORSCALE
				case ITEM_PROGRESSBAR:
				{
#ifdef _USE_PROGRESSCTRLEX
					ControlCWnd = new CProgressCtrlEx;
					((CProgressCtrlEx*)ControlCWnd)->Create(WS_CHILD | WS_VISIBLE, ControlRect, Parent, NameSpaceID::g_CurrentID++);
#else
					ControlCWnd = new CProgressCtrl;
					((CProgressCtrl*)ControlCWnd)->Create(WS_CHILD | WS_VISIBLE, ControlRect, Parent, NameSpaceID::g_CurrentID++);
#endif
					ControlCWnd->SetFont(Parent->GetFont());
					break;
				}
			}
		}

		void ChangeID(_In_ int ID)
		{
			Index = ID;

			if (IsControlExist())
			{
				switch (ControlType)
				{
					case ITEM_TEXT:
						break;
					case ITEM_COLORPICKER:
						break;
					case ITEM_BUTTON:
						((CMyButton*)ControlCWnd)->m_ButtonID = ID;
						break;
#ifdef _USE_HORSCALE
					case ITEM_HORSCALE:
						break;
#endif // USE_HORSCALE
					case ITEM_PROGRESSBAR:
						break;
					default:
						break;
				}
			}
		}
		void ChangeStruct(_In_ int _ID, _In_ CString _Name, _In_ ItemType _ControlType, _In_ COLORREF _Color, CWnd *pParrent)
		{
			Color = _Color;
			ChangeID(_ID);

			if (_ControlType != ControlType)
			{
				DeleteControl();
				ControlType = _ControlType;
				CreateControl(pParrent);
			}

			Name = _Name;
			if (IsControlExist())
				ControlCWnd->SetWindowText(_Name);
		}
		void ShowWindow(_In_ int nCmdShow)
		{
			if (IsControlExist())
				ControlCWnd->ShowWindow(nCmdShow);
		}
		void MoveWindow(_In_ CRect NewRect)
		{
			if (IsControlExist())
			{
				if (ControlRect != NewRect)
				{
					ControlRect = NewRect;
					ControlCWnd->MoveWindow(ControlRect);
				}
			}
		}
		void DeleteControl()
		{
			if (ControlCWnd)
			{
				if (::IsWindow(ControlCWnd->m_hWnd))
				{
					if (::DestroyWindow(ControlCWnd->m_hWnd) == 0)
					{
						if (GetLastError() == 0x5)
							::MessageBox(ControlCWnd->m_hWnd, L"�������� �������� �� �� ��������� ������", L"������", MB_OK);
					}
				}

				delete ControlCWnd;
				ControlCWnd = nullptr;
				//ControlRect.SetRectEmpty();
			}
		}
		bool IsControlExist()
		{
			return ControlCWnd && IsWindow(ControlCWnd->m_hWnd);
		}
	};


	DECLARE_MESSAGE_MAP()

	int m_nFirstColumnWidth; // the width of the first column 
	int m_nOffsetX;      	 // offset of this window inside the parent 
	LVBKIMAGE m_bkImage;	 // information about background image
	CImageList m_imgBtns;	 // tree buttons images (IDB_TREEBTNS)

	int m_arrColWidths[MAX_COLUMN_COUNT];

	int m_SortByColumn;								// ����� ������� �� ������� ����� ����������
	std::vector<ColumnData>	m_ColumnData;			// ���������� � ������� [����� �������]
	std::vector<std::vector<CellData>> m_CellData;	// ���������� � ������	[����� �������][����� �������]
	

	BOOL CheckHit(CPoint point);

	void ShowChildControls(_In_ HTREEITEM hParent, _In_ int nCmdShow);

	// �������� ������ �������� � ������ �� ��� �����������
	UINT GetItemIndex(_In_ HTREEITEM hItem);
	HTREEITEM GetItemByIndex(_In_ UINT Index);

	// �������� ��������� ������� �� ������
	HTREEITEM NextItem(HTREEITEM hItem);

protected:
	void HideChildCtrls(_In_ HTREEITEM hParent);
	/*
	 * Message Handlers
	 */
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	BOOL OnToolTipNeedText(UINT id, NMHDR * pTTTStruct, LRESULT * pResult);
	void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	/*
	 * Custom drawing related methods
	 */

#ifdef _OWNER_DRAWN_TREE
	LRESULT CustomDrawNotify(LPNMTVCUSTOMDRAW lpnm);
	LRESULT OwnerDraw(CDC* pDC);
	int OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
#endif //_OWNER_DRAWN_TREE
};

class CColumnTreeCtrl : public CStatic
{
public:
	DECLARE_DYNCREATE(CColumnTreeCtrl)

	/*
	 * Construction/destruction
	 */
	 
	CColumnTreeCtrl();
	virtual ~CColumnTreeCtrl();

	// explicit construction 
	BOOL Create(DWORD dwStyle , const RECT& rect, CWnd* pParentWnd, UINT nID);

	virtual void PreSubclassWindow();

		/*
	 *  Operations
	 */

	virtual void AssertValid( ) const;

	CCustomTreeChildCtrl& GetTreeCtrl() { return m_Tree; }
	CHeaderCtrl& GetHeaderCtrl() { return m_Header; }
	//*********************************************************************************************
	/* *��������� ������� � ������������� ����������
	@param nCol	   - ����� �������
	@param lpszColumnHeading - �������� �������
	@param nFormat - ������ ������� LVCFMT_CENTER � ��
	@param nWidth  - ������ �������
	@param ColumnSortType - ��� ���������� �������*/
	int InsertColumn(_In_ int nCol, _In_ LPCTSTR lpszColumnHeading,
					 _In_opt_ int nFormat = LVCFMT_LEFT, _In_opt_ int nWidth = -1,
					 _In_opt_ ColumnSortType Type = CLMNS_NOSORT);
	//*********************************************************************************************
	/* *��������� ������� � ������������� �����������
	@param lpszItem - �������� ������
	@param hParent  - ������������ �������
	@param hInsertAfter - �������� ����� ������������� ��������
	@param bCheck   - ����� �� ������� ������� ����� ���������� ��������*/
	HTREEITEM InsertItem(_In_z_ LPCTSTR lpszItem,     
						 _In_opt_ HTREEITEM hParent = TVI_ROOT, 
						 _In_opt_ HTREEITEM hInsertAfter = TVI_LAST,
						 _In_opt_ BOOL bCheck = FALSE);
	//*********************************************************************************************
	/* *��������� ������� � ������������� �����������
	@param Index	- ������ ������
	@param lpszItem - �������� ������
	@param bCheck   - ����� �� ������� ������� ����� ���������� ��������*/
	int InsertItem(_In_ int Index, _In_ LPCTSTR lpszItem, _In_opt_ BOOL bCheck = FALSE);
	//*********************************************************************************************
	/* *��������� ����� � ������ �� ������������ ���������
	@param hItem	- ���������� �������� � ������� ����� ������������� �����
	@param nColumn	- ����� ������� � ������� ���������� �����
	@param lpszText - �����
	@param ControlType  - ��� �������� ������� ����� �������� � ������
	@param Color	- ���� ������*/
	void SetItemText(_In_ HTREEITEM hItem, _In_ int nColumn, _In_ LPCTSTR lpszText, _In_opt_ bool DefaultItem = true,
					 _In_opt_ ItemType ControlType = ITEM_TEXT, _In_opt_ COLORREF Color = RGB(255, 255, 255));
	//*********************************************************************************************
	/* *��������� ����� � ������ �� �� ������������ �������
	@param hItem	- ���������� �������� � ������� ����� ������������� �����
	@param nColumn	- ����� ������� � ������� ���������� �����
	@param lpszText - �����
	@param ControlType  - ��� �������� ������� ����� �������� � ������
	@param Color	- ���� ������*/
	void SetItemText(_In_ int Index, _In_ int nColumn, _In_ LPCTSTR lpszText, _In_opt_ bool DefaultItem = true,
					 _In_opt_ ItemType ControlType = ITEM_TEXT, _In_opt_ COLORREF Color = RGB(255, 255, 255));	
	//*********************************************************************************************	
	void FindItems(CString _SearchString);
	//*********************************************************************************************
	// Method:    �������� ������ �������� �������� � ������ �� ��� �����������
	// Returns:   UINT ������ �������� � ������
	// Parameter: _In_ HTREEITEM hItem ���������� �������� ������� ����� ������
	// Parameter: _In_opt_ bool CurrentItem true  - ����� ���� �� ������� ��������� ������, 
	//										false - �� �������� � ������ ������(�� ���������� � ������������� ������)
	UINT GetItemIndex(_In_ HTREEITEM hItem, _In_opt_ bool DefaultItem = true);
	//*********************************************************************************************
	// Method:    �������� ���������� �������� �������� � ������ �� ��� �������
	// Returns:   HTREEITEM ���������� ��������
	// Parameter: _In_ UINT Index ������ �������� ������� �� ����� �����
	// Parameter: _In_opt_ bool CurrentItem true  - ����� ���� �� ������� ��������� ������, 
	//										false - �� �������� � ������ ������(�� ���������� � ������������� ������)
	HTREEITEM GetItemByIndex(_In_ UINT Index,	_In_opt_ bool CurrentItem = false);
	//*********************************************************************************************
	// ������� ��� �������������� ���������, ����������� ���������� �������� �������� �� ����������� � ��������
	HTREEITEM GetCurrentItemFromDefault(_In_ HTREEITEM DefaultItem);
	HTREEITEM GetDefaultItemFromCurrent(_In_ HTREEITEM CurrentItem);
	//*********************************************************************************************
	// �������� ��������� �� ������� ������� ��������� � ��������� ������ ��� ��� ���������
	// ���� �������� ���. �� �������� nullptr
	CWnd* GetItemControl(_In_ HTREEITEM hItem,	_In_ int nColumn, _In_opt_ bool DefaultItem = true);
	CWnd* GetItemControl(_In_ int Index,		_In_ int nColumn, _In_opt_ bool DefaultItem = true);
	//*********************************************************************************************
	BOOL IsItemExpanded(HTREEITEM hItem);
	// Expands the children of the specified item.
	BOOL Expand(_In_ HTREEITEM hItem, _In_ UINT nCode);
	BOOL IsItemVisible(HTREEITEM hItem);
	//*********************************************************************************************
	BOOL ModifyStyle(DWORD dwRemove, DWORD dwAdd, UINT nFlags = 0);
	//*********************************************************************************************
	void SetFirstColumnMinWidth(UINT uMinWidth);
	int  GetColumnWidth(int nCol);
	void SetColumnWidth(_In_ int nCol, _In_ int cx);
	//*********************************************************************************************
	BOOL DeleteColumn(int nCol);

	void DeleteChildrens(HTREEITEM hItem);
	void DeleteAllChildrens();
	
	void DeleteAllItems();		// ������� ��� ����� �������
	void DeleteAll();			// ������� ���(� �������)

	BOOL DeleteItem(HTREEITEM hItem, _In_opt_ bool DefaultItem = true);
	//*********************************************************************************************
	void DisableAllChildrens();
	void ActivateChildrens(HTREEITEM hItem);				// �������� ������� ���� �������� ���������
	//*********************************************************************************************
	// ���������� 1 ���� �������� ������� �������, 0 ���� ���
	// param: HTREEITEM Parent, Index �������
	int IsChildChecked(HTREEITEM hItem, int ChildIndex);
	//*********************************************************************************************
	bool IsChecked(_In_ unsigned Index, _In_ HTREEITEM hParent = TVI_ROOT);
	BOOL GetCheck(_In_ unsigned Index, _In_ HTREEITEM hParent = TVI_ROOT);
	BOOL SetCheck(_In_ unsigned Index, _In_ BOOL Check = TRUE, _In_ HTREEITEM hParent = TVI_ROOT);
	//*********************************************************************************************	
	// �������� ����� � ������
	CString GetItemText(HTREEITEM hItem, int nSubItem, _In_opt_ bool CurrentItem = false);
	//*********************************************************************************************
	HTREEITEM HitTest(CPoint pt, UINT* pFlags = NULL) const;
	HTREEITEM HitTest(CPoint pt, int ind) const;
	HTREEITEM HitTest(CTVHITTESTINFO* pHitTestInfo) const;	
	HTREEITEM NextItem(HTREEITEM hItem);
	//*********************************************************************************************
	void AutoScaleColumns();	// ������������������� �������
	//*********************************************************************************************
	void StopSort();			// ������� ���������� � �������
	void Resort();				// ��������������� ������
private:
	void RestoreTreeItems();
	void RemoveDesiredElements(_In_ HTREEITEM _hStartItem, _In_ const CString &_SearchString);

	bool ChangeColumnSort(_In_ const unsigned &_Column);
	//*********************************************************************************************
	// ����������� ���������� �������
	int InsertNewColumn(_In_ int nCol, _In_ LPCTSTR lpszColumnHeading,
					 _In_opt_ int nFormat = 0, _In_opt_ int nWidth = -1, _In_opt_ int nSubItem = -1);
	// ��������� � ������� ����� �� �������� ���������
	void SetColumnText(_In_ HTREEITEM hItem, _In_ int nColumn, _In_ LPCTSTR lpszText);

	void CustomSortItem(HTREEITEM item, int Column);
	static int CALLBACK SortColumnWithText(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	static int CALLBACK SortColumnWithNumber(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
protected:
	// ���� ���� ��� ������������� ���������� ����� Create
	bool bInitializationFromCreate;
	DECLARE_MESSAGE_MAP()

	enum ChildrenIDs { HeaderID = 1, TreeID = 2, HScrollID = 3, Header2ID = 4};
	
	CCustomTreeChildCtrl m_Tree;
	CScrollBar m_horScroll;
	CButton m_CheckSelectAll;
	CCheckBoxedHeaderCtrl m_Header;
	CHeaderCtrl m_Header2;
		
//	CWnd* m_pParentWnd;

	int m_cyHeader;
	int m_cxTotal;
	int m_xPos;
	int m_xOffset;
	int m_uMinFirstColWidth;
	BOOL m_bHeaderChangesBlocked;

	DWORD m_arrColFormats[MAX_COLUMN_COUNT];
	
	virtual void Initialize();
	void UpdateColumns();
	void RepositionControls();

	virtual void OnDraw(CDC* pDC) {}
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnHeaderItemChanging(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnHeaderItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTreeItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCancelMode();
	afx_msg void OnEnable(BOOL bEnable);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
	afx_msg BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSelectAll();
	afx_msg LRESULT OnClickButton(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnHeaderClicked(WPARAM wParam, LPARAM lParam);
};
