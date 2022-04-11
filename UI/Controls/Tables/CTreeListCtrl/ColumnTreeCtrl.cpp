#include "stdafx.h"
#include "ColumnTreeCtrl.h"

#include <shlwapi.h>
#include <algorithm>

#define RESERVED_LINES_COUNT		4000	// количество зарезервированных строк

#ifdef _DEBUG
#define new DEBUG_NEW  
#endif

// IE 5.0 or higher required
#ifndef TVS_NOHSCROLL
	#error CColumnTreeCtrl requires IE 5.0 or higher; _WIN32_IE should be greater than 0x500.
#endif


//-------------------------------------------------------------------------------
// Helper drawing funtions.
// I tried standard GDI fucntion LineTo with PS_DOT pen style, 
// but that didn't have the effect I wanted, so I had to use these ones. 
//-------------------------------------------------------------------------------

// draws a dotted horizontal line
static void _DotHLine(HDC hdc, LONG x, LONG y, LONG w, COLORREF cr)
{
	for (; w>0; w-=2, x+=2)
		SetPixel(hdc, x, y, cr);
}

// draws a dotted vertical line
static void _DotVLine(HDC hdc, LONG x, LONG y, LONG w, COLORREF cr)
{
	for (; w>0; w-=2, y+=2)
		SetPixel(hdc, x, y, cr);
}

//--------------------------------------------------------------------------------
// CCustomTreeChildCtrl Implementation
//--------------------------------------------------------------------------------

IMPLEMENT_DYNAMIC(CCustomTreeChildCtrl, CTreeCtrl);

//--------------------------------------------------------------------------------
// Construction/Destruction
//--------------------------------------------------------------------------------

CCustomTreeChildCtrl::CCustomTreeChildCtrl()
	: m_SortByColumn(0)
{
	m_ColumnData.resize(MAX_COLUMN_COUNT);			// информация о колонке [номер колонки]
	m_CellData  .resize(MAX_COLUMN_COUNT);			// информация о колонке [номер колонки]
	for (auto &it : m_CellData)
		it.reserve(RESERVED_LINES_COUNT);

	m_nFirstColumnWidth = 0;
	m_nOffsetX = 0;
	
	for (int i = 0; i < MAX_COLUMN_COUNT; i++)
		m_arrColWidths[i] = 0;

	memset(&m_bkImage, 0, sizeof(m_bkImage));

#ifdef _OWNER_DRAWN_TREE // only if owner-drawn 
	// init bitmap image structure
	m_bkImage.hbm=NULL;
	m_bkImage.xOffsetPercent = 0;
	m_bkImage.yOffsetPercent = 0;

	// create imagelist for tree buttons
	m_imgBtns.Create (9, 9, ILC_COLOR32|ILC_MASK,2,1);
	CBitmap* pBmpBtns = CBitmap::FromHandle(LoadBitmap(AfxGetInstanceHandle(),
		MAKEINTRESOURCE(IDB_TREEBTNS)));
	ASSERT(pBmpBtns);
	m_imgBtns.Add(pBmpBtns,RGB(255,0,255));
#endif //_OWNER_DRAWN_TREE

}

CCustomTreeChildCtrl::~CCustomTreeChildCtrl()
{
}


BEGIN_MESSAGE_MAP(CCustomTreeChildCtrl, CTreeCtrl)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_WM_TIMER()
	ON_WM_KEYDOWN()	

	//ON_NOTIFY( TTN_NEEDTEXT, OnToolTipNeedText )
	//ON_WM_STYLECHANGED()
	ON_WM_VSCROLLCLIPBOARD()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
// Operations
//---------------------------------------------------------------------------

// gets control's background image
BOOL CCustomTreeChildCtrl::GetBkImage(LVBKIMAGE* plvbkImage) const
{
	memcpy_s(plvbkImage, sizeof(LVBKIMAGE), &m_bkImage, sizeof(LVBKIMAGE));
	return TRUE;
}

// sets background image for control
BOOL CCustomTreeChildCtrl::SetBkImage(LVBKIMAGE* plvbkImage)
{
	memcpy_s(&m_bkImage, sizeof(LVBKIMAGE), plvbkImage, sizeof(LVBKIMAGE));
	Invalidate();
	return TRUE;
}


//---------------------------------------------------------------------------
// Message Handlers
//---------------------------------------------------------------------------


void CCustomTreeChildCtrl::OnTimer(UINT_PTR nIDEvent)
{
	// Do nothing.
	// CTreeCtrl sends this message to scroll the bitmap in client area
	// which also causes background bitmap scrolling,
	// so we don't pass this message to the base class.
}

void CCustomTreeChildCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// CTreeCtrl may scroll the bitmap up or down in several cases,
	// so we need to invalidate entire client area
	Invalidate();

	//... and pass to the base class
	CTreeCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CCustomTreeChildCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	// mask left click if outside the real item's label
	if (CheckHit(point))
	{
		HTREEITEM hItem = HitTest(point);
				
		if(hItem)
		{
			// номер колонки по которой произошло нажатие
			int ColumnIndex(0);
			
			// ищем в какую колонку произошел "тык"
			int Offset(0);
			for (ColumnIndex; ColumnIndex < MAX_COLUMN_COUNT - 1; ColumnIndex++)
			{
				if ((point.x >= Offset) && (point.x <= Offset + m_arrColWidths[ColumnIndex]))
					break;

				Offset += m_arrColWidths[ColumnIndex];
			}


			auto it = std::find_if(m_CellData[ColumnIndex].begin(), m_CellData[ColumnIndex].end(), [&hItem](const CellData &Val)
			{
				return Val.CurrenthItem == hItem;
			});

			if (it != std::end(m_CellData[ColumnIndex]))
			{
				if (it->ControlType == ITEM_COLORPICKER)
				{
					CColorDialog dlg(it->Color);
					dlg.m_cc.Flags |= CC_FULLOPEN | CC_RGBINIT;
					if (dlg.DoModal() == IDOK)
					{
						it->Color = dlg.m_cc.rgbResult;
						GetParent()->GetParent()->PostMessage(WM_CHANGE_COLOR, (WPARAM)it->DefaulthItem, it->Color);

						RedrawWindow();
					}
				}

			}			
			
#ifdef _OWNER_DRAWN_TREE
			// if the clicked item is partially visible we won't process
			// the message to avoid background bitmap scrolling
			// TODO: need to avoid scrolling and process the message
			CRect rcItem, rcClient;
			GetClientRect(&rcClient);
			GetItemRect(hItem,&rcItem,FALSE);
			if(rcItem.bottom>rcClient.bottom)
			{
				Invalidate();
				EnsureVisible(hItem);
				SelectItem(hItem);
				//CTreeCtrl::OnLButtonDown(nFlags, point);
				return;
			}
#endif //_OWNER_DRAWN_TREE

			// select the clicked item
			SelectItem(hItem);
		}
		
		// call standard handler	
		CTreeCtrl::OnLButtonDown(nFlags, point);
	}
	else  
	{
		// if clicked outside the item's label
		// than set focus to contol window
		SetFocus(); 
	}
	
}

void CCustomTreeChildCtrl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	// process this message only if double-clicked the real item's label
	// mask double click if outside the real item's label
	if (CheckHit(point))
	{
		HTREEITEM hItem = HitTest(point);
		if(hItem)
		{
			if (GetItemStateEx(hItem) != 0)
				return;
#ifdef _OWNER_DRAWN_TREE
			// if the clicked item is partially visible we should invalidate
			// entire client area to avoid background bitmap scrolling
			CRect rcItem, rcClient;
			GetClientRect(&rcClient);
			GetItemRect(hItem,&rcItem,FALSE);
			if(rcItem.bottom>rcClient.bottom)
			{
				Invalidate();
				CTreeCtrl::OnLButtonDown(nFlags, point);
				return;
			}
#endif //_OWNER_DRAWN_TREE

			SelectItem(hItem);
		}

		// call standard message handler
		CTreeCtrl::OnLButtonDblClk(nFlags, point);
	}	
	else 
	{
		// if clicked outside the item's label
		// than set focus to contol window
		SetFocus();
	}
}

void CCustomTreeChildCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	// mask mouse move if outside the real item's label
	if (CheckHit(point))
	{
		// call standard handler
		CTreeCtrl::OnMouseMove(nFlags, point);
	}
	
}


#ifdef _OWNER_DRAWN_TREE // this code is only for owner-drawn contol

//----------------------------------------------------------------------
// Sends NM_CUSTOMDRAW notification to the parent (CColumnTreeCtrl)
// The idea is to use one custom drawing code for both custom-drawn and
// owner-drawn controls
//----------------------------------------------------------------------

LRESULT CCustomTreeChildCtrl::CustomDrawNotify(LPNMTVCUSTOMDRAW lpnm)
{
	lpnm->nmcd.hdr.hwndFrom = GetSafeHwnd();
	lpnm->nmcd.hdr.code = NM_CUSTOMDRAW;
	lpnm->nmcd.hdr.idFrom = GetWindowLong(m_hWnd, GWL_ID);
	return GetParent()->SendMessage(WM_NOTIFY,(WPARAM)0,(LPARAM)lpnm);
}

//---------------------------------------------------------------------------
// Performs painting in the client's area.
// The pDC parameter is the memory device context created in OnPaint handler.
//---------------------------------------------------------------------------

LRESULT CCustomTreeChildCtrl::OwnerDraw(CDC* pDC)
{
	NMTVCUSTOMDRAW nm;	// this structure is used by NM_CUSTOMDRAW message
	DWORD dwFlags;		// custom-drawing flags

	DWORD dwRet;		// current custom-drawing operation's return value
	CRect rcClient;		// client's area rectangle

	// get client's rectangle
	GetClientRect(&rcClient);

	// initialize the structure
	memset(&nm,0,sizeof(NMTVCUSTOMDRAW));

	// set current drawing stage to pre-paint
	nm.nmcd.dwDrawStage = CDDS_PREPAINT; 
	nm.nmcd.hdc = pDC->m_hDC;
	nm.nmcd.rc = rcClient;
	
	// notify the parent (CColumnTreeCtrl) about pre-paint stage
	dwFlags = (DWORD)CustomDrawNotify(&nm); // CDDS_PREPAINT
	
	//
	// draw control's background
	//

	// set control's background color
	COLORREF crBkgnd = IsWindowEnabled()?pDC->GetBkColor():GetSysColor(COLOR_BTNFACE);
	// ... and fill the background rectangle
	pDC->FillSolidRect( rcClient, crBkgnd ); 

	if(m_bkImage.hbm && IsWindowEnabled())
	{
		// draw background bitmap

		int xOffset = m_nOffsetX;
		int yOffset = rcClient.left;
		int yHeight = rcClient.Height();

		SCROLLINFO scroll_info;
		// Determine window viewport to draw into taking into account
		// scrolling position
		if ( GetScrollInfo( SB_VERT, &scroll_info, SIF_POS | SIF_RANGE ) )
		{
			yOffset = -scroll_info.nPos;
			yHeight = max( scroll_info.nMax+1, rcClient.Height());
		}
	
		// create temporary memory DC for background bitmap
		CDC dcTemp;
		dcTemp.CreateCompatibleDC(pDC);
		BITMAP bm;
		::GetObject(m_bkImage.hbm,sizeof(BITMAP),&bm);
		CBitmap* pOldBitmap = 
			dcTemp.SelectObject( CBitmap::FromHandle(m_bkImage.hbm) ); 
		
		// copy the background bitmap from temporary DC to painting DC
		float x = (float)m_bkImage.xOffsetPercent/100*(float)rcClient.Width();
		float y = (float)m_bkImage.yOffsetPercent/100*(float)rcClient.Height();
		pDC->BitBlt(/*xOffset*/+(int)x, 
			/*yOffset+*/(int)y, 
			bm.bmWidth, bm.bmHeight, &dcTemp, 0, 0, SRCCOPY);
		
		// clean up
		dcTemp.SelectObject(pOldBitmap);
			
	}

	// notify the parent about post-erase drawing stage
	if(dwFlags&CDRF_NOTIFYPOSTERASE)
	{
		nm.nmcd.dwDrawStage = CDDS_POSTERASE;
		dwRet = (DWORD)CustomDrawNotify(&nm); // CDDS_POSTERASE
	}
	
	// select correct font
	CFont* pOldFont = pDC->SelectObject(GetFont());
	
	// get control's image lists
	CImageList* pstateList = GetImageList(TVSIL_STATE);
	CImageList* pimgList = GetImageList(TVSIL_NORMAL);
	
	// here we will store dimensions of the images
	CSize iconSize, stateImgSize;

	// retreive information about item images
	if(pimgList)
	{
		// get icons dimensions
		IMAGEINFO ii;
		if(pimgList->GetImageInfo(0, &ii))
			iconSize = CSize(ii.rcImage.right-ii.rcImage.left,
			ii.rcImage.bottom-ii.rcImage.top);
	}		
	
	// retrieve information about state images
	if(pstateList)
	{
		// get icons dimensions
		IMAGEINFO ii;
		if(pstateList->GetImageInfo(0, &ii))
			stateImgSize = CSize(ii.rcImage.right-ii.rcImage.left,
			ii.rcImage.bottom-ii.rcImage.top);
	}

	//
	// draw all visible items
	//

	HTREEITEM hItem = GetFirstVisibleItem();

	while(hItem)
	{
		// set transparent background mode
		int nOldBkMode = pDC->SetBkMode(TRANSPARENT);

		// get CTreeCtrl's style
		DWORD dwStyle = GetStyle();

		// get current item's state
		DWORD dwState = GetItemState(hItem,0xFFFF);

		BOOL bEnabled = IsWindowEnabled();
		BOOL bSelected = dwState&TVIS_SELECTED;
		BOOL bHasFocus = (GetFocus() && GetFocus()->m_hWnd==m_hWnd)?TRUE:FALSE;
			

		// Update NMCUSTOMDRAW structure. 
		// We won't draw entire items here (only item icons and lines), 
		// all other work will be done in parent's notifications handlers.
		// This allows to use one code for both custom-drawn and owner-drawn controls.

		nm.nmcd.dwItemSpec = (DWORD_PTR)hItem;

		// set colors for item's background and text
		if(bEnabled)
		{	
			if(bHasFocus)
			{
				nm.clrTextBk = bSelected?GetSysColor(COLOR_HIGHLIGHT):crBkgnd;
				nm.clrText = ::GetSysColor(bSelected?COLOR_HIGHLIGHTTEXT:COLOR_MENUTEXT);
				nm.nmcd.uItemState = dwState | (bSelected?CDIS_FOCUS:0);
			}
			else
			{
				if(GetStyle()&TVS_SHOWSELALWAYS)
				{
					nm.clrTextBk = bSelected?GetSysColor(COLOR_INACTIVEBORDER):crBkgnd;
					nm.clrText = ::GetSysColor(COLOR_MENUTEXT);
				}
				else
				{
					nm.clrTextBk = crBkgnd;
					nm.clrText = ::GetSysColor(COLOR_MENUTEXT);
				}
			}
		}
		else
		{
			nm.clrTextBk = bSelected?GetSysColor(COLOR_BTNSHADOW):crBkgnd;
			nm.clrText = ::GetSysColor(COLOR_GRAYTEXT);
		}
		
		
		// set item's rectangle
		GetItemRect(hItem,&nm.nmcd.rc,0);

		// set clipping rectangle
		CRgn rgn;
		rgn.CreateRectRgn(nm.nmcd.rc.left, nm.nmcd.rc.top, 
			nm.nmcd.rc.left+m_nFirstColumnWidth, nm.nmcd.rc.bottom);
		pDC->SelectClipRgn(&rgn);

		dwRet = CDRF_DODEFAULT;

		// notify the parent about item pre-paint drawing stage
		if(dwFlags&CDRF_NOTIFYITEMDRAW)
		{
			nm.nmcd.dwDrawStage = CDDS_ITEMPREPAINT;
			dwRet = (DWORD)CustomDrawNotify(&nm); // CDDS_ITEMPREPAINT

		}

		if(!(dwFlags&CDRF_SKIPDEFAULT))
		{
			//
			// draw item's icons and dotted lines
			//

			CRect rcItem;
			int nImage,nSelImage;
			
			GetItemRect(hItem,&rcItem,TRUE);
			GetItemImage(hItem,nImage,nSelImage);
				
			int x = rcItem.left-3;
			int yCenterItem = rcItem.top + (rcItem.bottom - rcItem.top)/2; 

			// draw item icon
			if(pimgList)
			{
				int nImageIndex = dwState&TVIS_SELECTED?nImage:nSelImage;
				x-=iconSize.cx+1;
				pimgList->Draw(pDC,nImageIndex,
					CPoint(x, yCenterItem-iconSize.cy/2),ILD_TRANSPARENT);
			}
			
			// draw item state icon
			if(GetStyle()&TVS_CHECKBOXES && pstateList!=NULL)
			{
				// get state image index
				DWORD dwStateImg = GetItemState(hItem,TVIS_STATEIMAGEMASK)>>12;
				
				x-=stateImgSize.cx;

				pstateList->Draw(pDC,dwStateImg,
					CPoint(x, yCenterItem-stateImgSize.cy/2),ILD_TRANSPARENT);
			}

			
			if(dwStyle&TVS_HASLINES)
			{
				//
				// draw dotted lines 
				//

				// create pen
				CPen pen;
				pen.CreatePen(PS_DOT,1,GetLineColor());
				CPen* pOldPen = pDC->SelectObject(&pen);
				
				HTREEITEM hItemParent = GetParentItem(hItem);

				if(hItemParent!=NULL ||dwStyle&TVS_LINESATROOT)
				{
					_DotHLine(pDC->m_hDC,x-iconSize.cx/2-2,yCenterItem,
						iconSize.cx/2+2,RGB(128,128,128));
				}
				
				if(hItemParent!=NULL ||	dwStyle&TVS_LINESATROOT && GetPrevSiblingItem(hItem)!=NULL)
				{
					_DotVLine(pDC->m_hDC,x-iconSize.cx/2-2,rcItem.top,
						yCenterItem-rcItem.top, RGB(128,128,128));
				}
				
				if(GetNextSiblingItem(hItem)!=NULL)
				{
					_DotVLine(pDC->m_hDC,x-iconSize.cx/2-2,yCenterItem,
						rcItem.bottom-yCenterItem,RGB(128,128,128));
				}

				int x1 = x-iconSize.cx/2-2;
				
				while(hItemParent!=NULL )
				{
					x1-=iconSize.cx+3;
					if(GetNextSiblingItem(hItemParent)!=NULL)
					{
						_DotVLine(pDC->m_hDC,x1,rcItem.top,rcItem.Height(),RGB(128,128,128));
					}
					hItemParent = GetParentItem(hItemParent);
				}
			
				// clean up
				pDC->SelectObject(pOldPen);
				
			}

			if(dwStyle&TVS_HASBUTTONS && ItemHasChildren(hItem))
			{
				// draw buttons
				int nImg = GetItemState(hItem,TVIF_STATE)&TVIS_EXPANDED?1:0;
				m_imgBtns.Draw(pDC, nImg, CPoint(x-iconSize.cx/2-6,yCenterItem-4), 
					ILD_TRANSPARENT);
			}

		}

		pDC->SelectClipRgn(NULL);

		// notify parent about item post-paint stage
		if(dwRet&CDRF_NOTIFYPOSTPAINT)
		{
			nm.nmcd.dwDrawStage = CDDS_ITEMPOSTPAINT;
			dwRet = (DWORD)CustomDrawNotify(&nm); // CDDS_ITEMPOSTPAINT
		}

		// clean up
		pDC->SetBkMode(nOldBkMode);

		// get the next visible item
		hItem = GetNextVisibleItem(hItem);
	};

	//clean up

	pDC->SelectObject(pOldFont);
	
	return 0;
}

int CCustomTreeChildCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// inavlidate entire client's area to avoid bitmap scrolling
	Invalidate();

	// ... and call standard message handler
	return CTreeCtrl::OnMouseWheel(nFlags, zDelta, pt);
}

#endif //_OWNER_DRAWN_TREE

void CCustomTreeChildCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	// необходимо спрятать/показать 
	CRect ControlRect;
	CTreeCtrl::GetClientRect(ControlRect);

	std::for_each(m_CellData.front().begin(), m_CellData.front().end(), [&](CCustomTreeChildCtrl::CellData &Element)
	{
		CRect rect;
		CTreeCtrl::GetItemRect(Element.CurrenthItem, &rect, FALSE);
		if ((rect.bottom < 0) || (rect.top > ControlRect.bottom))
		{
			for (auto &it : m_CellData)
			{
				it[Element.Index].MoveWindow(CRect());
			}
		}
	});







// 			// прячем все контролы которые относятся к этой строчке
// 			auto it = std::find_if(m_Tree.m_CellData.front().begin(), m_Tree.m_CellData.front().end(), [&hItem](const CCustomTreeChildCtrl::CellData &Param)
// 			{
// 				return Param.CurrenthItem == hItem;
// 			});
// 
// 			if (it != m_Tree.m_CellData.front().end())
// 			{
// 				std::for_each(m_Tree.m_CellData.begin(), m_Tree.m_CellData.end(), [&](std::vector<CCustomTreeChildCtrl::CellData> &Element)
// 				{
// 					Element[it->Index].ShowWindow(SW_HIDE);
// 				});
// 			}



//  HTREEITEM hItem1 =  CTreeCtrl::GetFirstVisibleItem();
//  HTREEITEM hItem2 = CTreeCtrl::GetLastVisibleItem();

	
	// inavlidate entire client's area to avoid bitmap scrolling
//	Invalidate();
// 
// 	CString tt;
// 	tt.Format(L"nSBCode = %d, nPos = %d\n", nSBCode, nPos);
// 	OutputDebugString(tt);

	// ... and call standard message handler
	CTreeCtrl::OnVScroll(nSBCode, nPos, pScrollBar);
}



void CCustomTreeChildCtrl::OnPaint()
{
	CRect rcClient;
	GetClientRect(&rcClient);

	CPaintDC dc(this);
	
	CDC dcMem;
	CBitmap bmpMem;

	// use temporary bitmap to avoid flickering
	dcMem.CreateCompatibleDC(&dc);
	if (bmpMem.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height()))
	{
		CBitmap* pOldBmp = dcMem.SelectObject(&bmpMem);

		// paint the window onto the memory bitmap

#ifdef _OWNER_DRAWN_TREE	// if owner-drawn
		OwnerDraw(&dcMem);	// use our code
#else						// else use standard code
		CWnd::DefWindowProc(WM_PAINT, (WPARAM)dcMem.m_hDC, 0);
#endif //_OWNER_DRAWN_TREE

		// copy it to the window's DC
		dc.BitBlt(0, 0, rcClient.right, rcClient.bottom, &dcMem, 0, 0, SRCCOPY);

		dcMem.SelectObject(pOldBmp);

		bmpMem.DeleteObject();
	}
	dcMem.DeleteDC();

}

BOOL CCustomTreeChildCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;	// do nothing
}

BOOL CCustomTreeChildCtrl::CheckHit(CPoint point)
{
	// return TRUE if should pass the message to CTreeCtrl

	UINT fFlags(0);
	HTREEITEM hItem = HitTest(point, &fFlags);

	CRect rcItem,rcClient;
	GetClientRect(rcClient);
	GetItemRect(hItem, &rcItem, TRUE);

	if (fFlags & TVHT_ONITEMICON ||
		fFlags & TVHT_ONITEMBUTTON ||
		fFlags & TVHT_ONITEMSTATEICON)
		return TRUE;

	if(GetStyle()&TVS_FULLROWSELECT)
	{
		rcItem.right = rcClient.right;
		if(rcItem.PtInRect(point)) 
			return TRUE;
		
		return FALSE;
	}


	// verify the hit result
	if (fFlags & TVHT_ONITEMLABEL)
	{
		rcItem.right = m_nFirstColumnWidth;
		// check if within the first column
		if (!rcItem.PtInRect(point))
			return FALSE;
		
		CString strSub;
		AfxExtractSubString(strSub, GetItemText(hItem), 0, '\t');

		CDC* pDC = GetDC();
		pDC->SelectObject(GetFont());
		rcItem.right = rcItem.left + pDC->GetTextExtent(strSub).cx + 6;
		ReleaseDC(pDC);

		// check if inside the label's rectangle
		if (!rcItem.PtInRect(point))
			return FALSE;
		
		return TRUE;
	}

	return FALSE;
}

BOOL CCustomTreeChildCtrl::OnToolTipNeedText( UINT id, NMHDR * pTTTStruct, LRESULT * pResult )
{
	return FALSE;
}

UINT CCustomTreeChildCtrl::GetItemIndex(_In_ HTREEITEM hItem)
{
	UINT Index(0);
	HTREEITEM hRes = GetRootItem();
	do
	{
		if (hRes == hItem)
			break;
		Index++;
	} while ((hRes = NextItem(hRes)) != NULL);

	return Index;
}

HTREEITEM CCustomTreeChildCtrl::GetItemByIndex(_In_ UINT Index)
{
	UINT CurIndex(0);
	HTREEITEM hRes = GetRootItem();
	do
	{
		if (CurIndex == Index)
			break;
		CurIndex++;
	} while ((hRes = NextItem(hRes)) != NULL);

	return hRes;
}

HTREEITEM CCustomTreeChildCtrl::NextItem(HTREEITEM hItem) 
{
	HTREEITEM hRes;

	if (ItemHasChildren(hItem))
		return GetChildItem(hItem);
	while ((hRes = GetNextSiblingItem(hItem)) == NULL)
	{
		if ((hItem = GetParentItem(hItem)) == NULL)
			return NULL;
	}

	return hRes;
}

void CCustomTreeChildCtrl::ShowChildControls(_In_ HTREEITEM hParent, _In_ int nCmdShow)
{
	HTREEITEM hItem = GetChildItem(hParent);
	
	if (hItem != NULL)
	{
		do
		{
			unsigned Index = GetItemIndex(hItem);
			if (m_CellData.front().size() > Index)
			{
				for (auto &it : m_CellData)
				{
					it[Index].ShowWindow(nCmdShow);
				}
			}

			if (ItemHasChildren(hItem) != FALSE)
				ShowChildControls(hItem, nCmdShow);
		} while ((hItem = GetNextSiblingItem(hItem)) != NULL);
	}
}

void CCustomTreeChildCtrl::HideChildCtrls(_In_ HTREEITEM hParent)
{
	HTREEITEM hItem = GetChildItem(hParent);

	if (hItem != NULL)
	{
		do
		{
			unsigned Index = GetItemIndex(hItem);
			if (m_CellData.front().size() > Index)
			{
				for (auto &it : m_CellData)
				{
					it[Index].MoveWindow(CRect());
				}
			}

			if (ItemHasChildren(hItem) != FALSE)
				HideChildCtrls(hItem);
		} while ((hItem = GetNextSiblingItem(hItem)) != NULL);
	}
}

#define COLUMN_MARGIN		1		// 1 pixel between coumn edge and text bounding rectangle

// default minimum width for the first column
#ifdef _OWNER_DRAWN_TREE
	#define DEFMINFIRSTCOLWIDTH 0 // we use clipping rgn, so we can have zero-width column		
#else
	#define DEFMINFIRSTCOLWIDTH 1	// here we need to avoid zero-width first column	
#endif

IMPLEMENT_DYNCREATE(CColumnTreeCtrl, CStatic)

BEGIN_MESSAGE_MAP(CColumnTreeCtrl, CStatic)
	//ON_MESSAGE(WM_CREATE,OnCreateWindow)
	
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_SETTINGCHANGE()
	ON_WM_ENABLE()
	ON_MESSAGE_VOID	(WM_USER + 100, OnSelectAll)
	ON_MESSAGE		(WM_USER + 101, &CColumnTreeCtrl::OnClickButton)
	ON_MESSAGE		(WM_USER + 102, &CColumnTreeCtrl::OnHeaderClicked)
	ON_WM_VSCROLL()
END_MESSAGE_MAP()


CColumnTreeCtrl::CColumnTreeCtrl()
	: 
	m_uMinFirstColWidth(DEFMINFIRSTCOLWIDTH),
	m_bHeaderChangesBlocked(FALSE),
	m_xOffset(0),
	m_cyHeader(0),
	m_cxTotal(0),
	m_xPos(0),
	bInitializationFromCreate(false)
{
	// initialize members
	memset(&m_arrColFormats, 0, sizeof(m_arrColFormats));
}

CColumnTreeCtrl::~CColumnTreeCtrl()
{
}

BOOL CColumnTreeCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	bInitializationFromCreate = true;
	CStatic::Create(_T(""), dwStyle, rect, pParentWnd, nID);
	bInitializationFromCreate = false;
	Initialize();
	return TRUE;
}

void CColumnTreeCtrl::PreSubclassWindow()
{
	if (!bInitializationFromCreate)
		Initialize();
}

void CColumnTreeCtrl::AssertValid( ) const
{
	// validate control state
	ASSERT( m_hWnd );
	ASSERT( m_Tree.m_hWnd ); 
	ASSERT( m_Header.m_hWnd );
	ASSERT( m_Header2.m_hWnd );
}

void CColumnTreeCtrl::Initialize()
{
	if (m_Tree.m_hWnd) 
		return; // do not initialize twice

	CRect rcClient;
	GetClientRect(&rcClient);
	
	// create tree and header controls as children
	m_Tree.Create(WS_CHILD | WS_VISIBLE  | TVS_NOHSCROLL | TVS_NOTOOLTIPS, CRect(), this, TreeID);
	m_Header.Create(WS_CHILD | HDS_BUTTONS | WS_VISIBLE | HDS_FULLDRAG, rcClient, this, HeaderID);
	m_Header2.Create(WS_CHILD , rcClient, this, Header2ID);

	m_CheckSelectAll.Create(_T(""), WS_CHILD | WS_TABSTOP | BS_3STATE, CRect(0, 0, 0, 0), &m_Header, CCheckBoxedHeaderCtrl::CheckAll);
	CRect rc;
	m_Header.GetItemRect(0, &rc);
	m_CheckSelectAll.SetWindowPos(&CWnd::wndTop, rc.left + 4, rc.top + 4, 13, 13, SWP_SHOWWINDOW);

	if (GetStyle() & TVS_CHECKBOXES)
		m_CheckSelectAll.ShowWindow(SW_SHOW);
	else
		m_CheckSelectAll.ShowWindow(SW_HIDE);

	// create horisontal scroll bar
	m_horScroll.Create(SBS_HORZ|WS_CHILD|SBS_BOTTOMALIGN,rcClient,this,HScrollID);
	
	// set correct font for the header
	CFont* pFont = m_Tree.GetFont();
	m_Header.SetFont(pFont);
	m_Header2.SetFont(pFont);

	// check if the common controls library version 6.0 is available
	BOOL bIsComCtl6 = FALSE;

	HMODULE hComCtlDll = LoadLibrary(_T("comctl32.dll"));

	if (hComCtlDll)
	{
		typedef HRESULT (CALLBACK *PFNDLLGETVERSION)(DLLVERSIONINFO*);

		PFNDLLGETVERSION pfnDllGetVersion = (PFNDLLGETVERSION)GetProcAddress(hComCtlDll, "DllGetVersion");

		if (pfnDllGetVersion)
		{
			DLLVERSIONINFO dvi;
			ZeroMemory(&dvi, sizeof(dvi));
			dvi.cbSize = sizeof(dvi);

			HRESULT hRes = (*pfnDllGetVersion)(&dvi);

			if (SUCCEEDED(hRes) && dvi.dwMajorVersion >= 6)
				bIsComCtl6 = TRUE;
		}

		FreeLibrary(hComCtlDll);
	}

	// get header layout
	WINDOWPOS wp = { 0 };
	HDLAYOUT hdlayout = { 0 };
	hdlayout.prc = &rcClient;
	hdlayout.pwpos = &wp;
	m_Header.Layout(&hdlayout);
	m_cyHeader = hdlayout.pwpos->cy;
	
	// offset from column start to text start
	m_xOffset = bIsComCtl6 ? 9 : 6;

	m_xPos = 0;

	UpdateColumns();
}


void CColumnTreeCtrl::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	m_Tree.SendMessage(WM_SETTINGCHANGE);
	m_horScroll.SendMessage(WM_SETTINGCHANGE);

	// set correct font for the header
	CRect rcClient;
	GetClientRect(&rcClient);
	
	//CFont* pFont = m_Tree.GetFont();
	//m_Header.SetFont(pFont);
	//m_Header2.SetFont(pFont);
	
	m_Header.SendMessage(WM_SETTINGCHANGE);
	m_Header2.SendMessage(WM_SETTINGCHANGE);
	
	m_Header.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
	m_Header2.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));

	// get header layout
	WINDOWPOS wp = { 0 };
	HDLAYOUT hdlayout;
	hdlayout.prc = &rcClient;
	hdlayout.pwpos = &wp;
	m_Header.Layout(&hdlayout);
	m_cyHeader = hdlayout.pwpos->cy;

	RepositionControls();
}

void CColumnTreeCtrl::OnPaint()
{
	// do not draw entire background to avoid flickering
	// just fill right-bottom rectangle when it is visible
	
	CPaintDC dc(this);
	
	CRect rcClient;
	GetClientRect(&rcClient);
	
	int cyHScroll = GetSystemMetrics(SM_CYHSCROLL);
	int cxVScroll = GetSystemMetrics(SM_CXVSCROLL);
	CBrush brush;
	brush.CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));
	
	CRect rc;

	// determine if vertical scroll bar is visible
	SCROLLINFO scrinfo;
	scrinfo.cbSize = sizeof(scrinfo);
	m_Tree.GetScrollInfo(SB_VERT,&scrinfo,SIF_ALL);
	BOOL bVScrollVisible = scrinfo.nMin!=scrinfo.nMax?TRUE:FALSE;
	
	if(bVScrollVisible)
	{
		// fill the right-bottom rectangle
		rc.SetRect(rcClient.right-cxVScroll, rcClient.bottom-cyHScroll,
				rcClient.right, rcClient.bottom);
		dc.FillRect(rc,&brush);
	}

	
}

BOOL CColumnTreeCtrl::OnEraseBkgnd(CDC* pDC)
{
	// do nothing, all work is done in OnPaint()
	return FALSE;
	
}

void CColumnTreeCtrl::OnSize(UINT nType, int cx, int cy)
{
	RepositionControls();
}

void CColumnTreeCtrl::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO scrinfo;
	scrinfo.cbSize = sizeof(scrinfo);
	
	m_Tree.GetScrollInfo(SB_VERT,&scrinfo,SIF_ALL);
	
	BOOL bVScrollVisible = scrinfo.nMin!=scrinfo.nMax?TRUE:FALSE;
	
	// determine full header width
	int cxTotal = m_cxTotal+(bVScrollVisible?GetSystemMetrics(SM_CXVSCROLL):0);
	
	CRect rcClient;
	GetClientRect(&rcClient);
	int cx = rcClient.Width();

	int xLast = m_xPos;

	switch (nSBCode)
	{
	case SB_LINELEFT:
		m_xPos -= 15;
		break;
	case SB_LINERIGHT:
		m_xPos += 15;
		break;
	case SB_PAGELEFT:
		m_xPos -= cx;
		break;
	case SB_PAGERIGHT:
		m_xPos += cx;
		break;
	case SB_LEFT:
		m_xPos = 0;
		break;
	case SB_RIGHT:
		m_xPos = cxTotal - cx;
		break;
	case SB_THUMBTRACK:
		m_xPos = nPos;
		break;
	}

	if (m_xPos < 0)
		m_xPos = 0;
	else if (m_xPos > cxTotal - cx)
		m_xPos = cxTotal - cx;

	if (xLast == m_xPos)
		return;

	m_Tree.m_nOffsetX = m_xPos;

	SetScrollPos(SB_HORZ, m_xPos);
	CWnd::OnHScroll(nSBCode,nPos,pScrollBar);
	RepositionControls();
	
}

void CColumnTreeCtrl::OnHeaderItemChanging(NMHDR* pNMHDR, LRESULT* pResult)
{
	// do not allow user to set zero width to the first column.
	// the minimum width is defined by m_uMinFirstColWidth;

	if(m_bHeaderChangesBlocked)
	{
		// do not allow change header size when moving it
		// but do not prevent changes the next time the header will be changed
		m_bHeaderChangesBlocked = FALSE;
		*pResult = TRUE; // prevent changes
		return;
	}

	*pResult = FALSE;

	LPNMHEADER pnm = (LPNMHEADER)pNMHDR;
	if(pnm->iItem==0)
	{
		CRect rc;
		m_Header.GetItemRect(0,&rc);
		if(pnm->pitem->cxy<m_uMinFirstColWidth)
		{
			// do not allow sizing of the first column 
			pnm->pitem->cxy=m_uMinFirstColWidth+1;
			*pResult = TRUE; // prevent changes
		}
		return;
	}
	
}

void CColumnTreeCtrl::OnHeaderItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	UpdateColumns();

	m_Tree.Invalidate();
}

void CColumnTreeCtrl::OnTreeCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	// We use custom drawing to correctly display subitems.
	// Standard drawing code is used only for displaying icons and dotted lines
	// The rest of work is done here.

	NMCUSTOMDRAW* pNMCustomDraw = (NMCUSTOMDRAW*)pNMHDR;
	NMTVCUSTOMDRAW* pNMTVCustomDraw = (NMTVCUSTOMDRAW*)pNMHDR;

	switch (pNMCustomDraw->dwDrawStage)
	{
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;

	case CDDS_ITEMPREPAINT:
		if (m_Tree.GetStyle() & TVS_SHOWSELALWAYS)
		{
			if (pNMCustomDraw->uItemState != 0)
			{
				pNMTVCustomDraw->clrTextBk = 0xFF9933;
				pNMTVCustomDraw->clrText = 0xFFFFFF;
			}
			else
			{
				pNMTVCustomDraw->clrTextBk = 0xFFFFFF;
				pNMTVCustomDraw->clrText = 0;
			}
		}
		*pResult = CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT;
		break;
	case CDDS_ITEMPOSTPAINT:
	{
		HTREEITEM hItem = (HTREEITEM)pNMCustomDraw->dwItemSpec;
		CRect rcItem = pNMCustomDraw->rc;
		bool bNeedDrowColor(false);
		
		CRect TempRect;
		
		if (rcItem.IsRectEmpty())
		{
			*pResult = CDRF_DODEFAULT;
			break;
		}

		CDC dc;
		dc.Attach(pNMCustomDraw->hdc);

		CRect rcLabel;
		m_Tree.GetItemRect(hItem, &rcLabel, TRUE);

		COLORREF crTextBk = pNMTVCustomDraw->clrTextBk;
		COLORREF crWnd = GetSysColor((IsWindowEnabled() ? COLOR_WINDOW : COLOR_BTNFACE));

#ifndef _OWNER_DRAWN_TREE
		// clear the original label rectangle
		dc.FillSolidRect(&rcLabel, crWnd);
#endif //_OWNER_DRAWN_TREE


		// номер строчки отрисовываемого элемента 
		UINT Index = GetItemIndex(hItem, true);
		int xOffset = 0;

		TempRect = rcItem;
		int nColsCnt = m_Header.GetItemCount();
		// draw horizontal lines...
		for (int i = 0; i < nColsCnt; i++)
		{
			xOffset += m_Tree.m_arrColWidths[i];
			rcItem.right = xOffset;
			dc.DrawEdge(&rcItem, BDR_SUNKENINNER, BF_RIGHT);

			// такой элемент уже есть 
			if (m_Tree.m_CellData[i].size() > Index)
			{
				// запоминаем координаты второй колонки
				if (i == 0)
					TempRect.left = rcLabel.left;
				else
					TempRect.left = xOffset - m_Tree.m_arrColWidths[i];

				TempRect.right = xOffset - 1;

				// Ищем ячейку с таким дескриптором
				auto it = std::find_if(m_Tree.m_CellData[i].begin(), m_Tree.m_CellData[i].end(), [&hItem](const CCustomTreeChildCtrl::CellData &Param)
				{
					return Param.CurrenthItem == hItem;
				});

				if (it != std::end(m_Tree.m_CellData[i]))
				{
					dc.FillSolidRect(&TempRect, it->Color);
					it->MoveWindow(TempRect);
				}
			}
		}

		// ...and the vertical ones
		dc.DrawEdge(&rcItem, BDR_SUNKENINNER, BF_BOTTOM);
				
		CString strText = m_Tree.GetItemText(hItem);
		CString strSub;
		AfxExtractSubString(strSub, strText, 0, '\t');

		CRect rcText(0, 0, 0, 0);
		dc.DrawText(strSub, &rcText, DT_NOPREFIX | DT_CALCRECT);
		rcLabel.right = min(rcLabel.left + rcText.right + 4, m_Tree.m_arrColWidths[0] - 4);

		BOOL bFullRowSelect = m_Tree.GetStyle()&TVS_FULLROWSELECT;
	
		if (rcLabel.Width() < 0)
			crTextBk = crWnd;
		if (crTextBk != crWnd)	// draw label's background
		{
			CRect rcSelect = rcLabel;
			if (bFullRowSelect) 
				rcSelect.right = rcItem.right;
			
			dc.FillSolidRect(&rcSelect, crTextBk);
			
			// draw focus rectangle if necessary
			if (pNMCustomDraw->uItemState & CDIS_FOCUS)
				dc.DrawFocusRect(&rcSelect);

		}

		// draw main label

		CFont* pOldFont = NULL;
		if (m_Tree.GetStyle()&TVS_TRACKSELECT && pNMCustomDraw->uItemState & CDIS_HOT)
		{
			LOGFONT lf;
			pOldFont = m_Tree.GetFont();
			pOldFont->GetLogFont(&lf);
			lf.lfUnderline = 1;
			CFont newFont;
			newFont.CreateFontIndirect(&lf);
			dc.SelectObject(newFont);
		}

		rcText = rcLabel;
		rcText.DeflateRect(2, 1);

		xOffset = m_Tree.m_arrColWidths[0];
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(pNMTVCustomDraw->clrText);
		dc.DrawText(strSub, &rcText, DT_VCENTER | DT_SINGLELINE |
			DT_NOPREFIX | DT_END_ELLIPSIS);
		
		if (IsWindowEnabled() && !bFullRowSelect)
			dc.SetTextColor(::GetSysColor(COLOR_MENUTEXT));

		// set not underlined text for subitems
		if (pOldFont &&  !(m_Tree.GetStyle()& TVS_FULLROWSELECT))
			dc.SelectObject(pOldFont);

		// draw other columns text
		for (int i = 1; i < nColsCnt; i++)
		{
			if (AfxExtractSubString(strSub, strText, i, '\t'))
			{
				rcText = rcLabel;
				rcText.left = xOffset + COLUMN_MARGIN;
				rcText.right = xOffset + m_Tree.m_arrColWidths[i] - COLUMN_MARGIN;
				rcText.DeflateRect(m_xOffset, 1, 2, 1);
				if (rcText.left < 0 || rcText.right < 0 || rcText.left >= rcText.right)
				{
					xOffset += m_Tree.m_arrColWidths[i];
					continue;
				}
				DWORD dwFormat = m_arrColFormats[i] & HDF_RIGHT ?
				DT_RIGHT : (m_arrColFormats[i] & HDF_CENTER ? DT_CENTER : DT_LEFT);


				dc.DrawText(strSub, &rcText, DT_SINGLELINE | DT_VCENTER
					| DT_NOPREFIX | DT_END_ELLIPSIS | dwFormat);
				
			}

			xOffset += m_Tree.m_arrColWidths[i];
		}
		
		if (pOldFont) dc.SelectObject(pOldFont);
		dc.Detach();

		*pResult = CDRF_DODEFAULT;
		break;
	}
	default:
		*pResult = CDRF_DODEFAULT;
	}
}

void CColumnTreeCtrl::OnTreeItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTVITEMCHANGE* tvItemChange = reinterpret_cast<NMTVITEMCHANGE*>(pNMHDR);

	*pResult = FALSE;

	if (0 == tvItemChange->uStateOld && 0 == tvItemChange->uStateNew)
		return; // не было изменений

	// Старое состояние чекбокса
	BOOL bPrevState = (BOOL)(((tvItemChange->uStateOld & TVIS_STATEIMAGEMASK) >> 12) - 1);
	if (bPrevState < 0)    // При первом запуске состояние может быть не определено
		bPrevState = 0;	   // поэтому ставим его по умолчанию в false

	// Считать новое состояние
	BOOL bChecked = (BOOL)(((tvItemChange->uStateNew & LVIS_STATEIMAGEMASK) >> 12) - 1);
	if (bChecked < 0) // Новое состояние может быть неопределено.
		bChecked = 0; // По умочанию тоже ставим false

	if (bPrevState == bChecked) // Нет изменений
		return;
	
	// заменяем в векторе с информацией о каждой ячейке состояние чекбокса
	for (unsigned Column = 0; Column < MAX_COLUMN_COUNT; Column++)
	{
		auto it = std::find_if(m_Tree.m_CellData[Column].begin(), m_Tree.m_CellData[Column].end(), [&](const CCustomTreeChildCtrl::CellData &Val)
		{
			return Val.CurrenthItem == tvItemChange->hItem;
		});

		if (it != std::end(m_Tree.m_CellData[Column]))
			it->bSelected = bChecked;
	}	

	// включаем/отключаем дочерние записи
	HTREEITEM hRes;
	if ((hRes = m_Tree.GetChildItem(tvItemChange->hItem)) != NULL)
	{
		do
		{
			m_Tree.SetCheck(hRes, bChecked);
		} while ((hRes = m_Tree.GetNextSiblingItem(hRes)) != NULL);
	}

	// при необходимости меняем состояние родителя
	if ((hRes = m_Tree.GetParentItem(tvItemChange->hItem)) != NULL)
	{
		hRes = m_Tree.GetChildItem(hRes);
		bool bCheckPrevElement(true);
		BOOL AllItemsChecked = m_Tree.GetCheck(hRes);
		do
		{
			if (m_Tree.GetCheck(hRes) != AllItemsChecked)
			{
				bCheckPrevElement = false;
				break;
			}

		} while ((hRes = m_Tree.GetNextSiblingItem(hRes)) != NULL);

		if (bCheckPrevElement)
			m_Tree.SetCheck(m_Tree.GetParentItem(tvItemChange->hItem), AllItemsChecked);
	}
	
	GetParent()->PostMessage(WM_CHANGE_CHECK, (WPARAM)GetDefaultItemFromCurrent(tvItemChange->hItem), bChecked);
			
	// Изменения произошли
	UINT state = (UINT)0;
	HTREEITEM hItem = m_Tree.GetRootItem();
	while (hItem != NULL) 
	{
		state += m_Tree.GetCheck(hItem);
		hItem = NextItem(hItem);
	}

	if (!state)
	{	// Всё выключено
		m_CheckSelectAll.SetCheck(0);
	}
	else if (state == m_Tree.GetCount())
	{	// Всё включено
		m_CheckSelectAll.SetCheck(1);
	}
	else
	{	// Хотя бы что-то включено
		m_CheckSelectAll.SetCheck(2);
	}
}

void CColumnTreeCtrl::UpdateColumns()
{
	m_cxTotal = 0;

	HDITEM hditem;
	hditem.mask = HDI_WIDTH;
	int nCnt = m_Header.GetItemCount();
	
	ASSERT(nCnt<=MAX_COLUMN_COUNT);
	
	// get column widths from the header control
	for (int i=0; i<nCnt; i++)
	{
		if (m_Header.GetItem(i, &hditem))
		{
			m_Tree.m_arrColWidths[i] = hditem.cxy;
			m_cxTotal += m_Tree.m_arrColWidths[i];
			if (i==0)
				m_Tree.m_nFirstColumnWidth = hditem.cxy;
		}
	}
	m_bHeaderChangesBlocked = TRUE;
	RepositionControls();
}

void CColumnTreeCtrl::RepositionControls()
{
	// reposition child controls
	if (m_Tree.m_hWnd)
	{	
		CRect rcClient;
		GetClientRect(&rcClient);
		int cx = rcClient.Width();
		int cy = rcClient.Height();

		// get dimensions of scroll bars
		int cyHScroll = GetSystemMetrics(SM_CYHSCROLL);
		int cxVScroll = GetSystemMetrics(SM_CXVSCROLL);
	
		// сначала масштабируем без гор скрола чтобы правильно определить необходимость в вертикальном
		int x = 0;
		if (cx < m_cxTotal)
			x = m_horScroll.GetScrollPos();
		m_Tree.MoveWindow(-x, m_cyHeader, cx + x, cy - m_cyHeader);
		
		// determine if vertical scroll bar is visible
		SCROLLINFO scrinfo;
		scrinfo.cbSize = sizeof(scrinfo);
		m_Tree.GetScrollInfo(SB_VERT,&scrinfo,SIF_ALL);
		BOOL bVScrollVisible = scrinfo.nMin!=scrinfo.nMax?TRUE:FALSE;
	
		// determine full header width
		int cxTotal = m_cxTotal+(bVScrollVisible?cxVScroll:0);
	
		if (m_xPos > cxTotal - cx) m_xPos = cxTotal - cx;
		if (m_xPos < 0)	m_xPos = 0;
	
		scrinfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		scrinfo.nPage = cx;
		scrinfo.nMin = 0;
		scrinfo.nMax = cxTotal;
		scrinfo.nPos = m_xPos;
		m_horScroll.SetScrollInfo(&scrinfo);

		CRect rcTree;
		m_Tree.GetClientRect(&rcTree);
		
		// move to a negative offset if scrolled horizontally
		x = 0;
		if (cx < cxTotal)
		{
			x = m_horScroll.GetScrollPos();
			cx += x;
		}
	
		// show horisontal scroll only if total header width is greater
		// than the client rect width and cleint rect height is big enough
		BOOL bHScrollVisible = rcClient.Width() < cxTotal
			&& rcClient.Height()>=cyHScroll+m_cyHeader;
			
		m_horScroll.ShowWindow(bHScrollVisible?SW_SHOW:SW_HIDE);
	
		m_Header.MoveWindow(-x, 0, cx  - (bVScrollVisible?cxVScroll:0), m_cyHeader);
		
		m_Header2.MoveWindow(rcClient.Width()-cxVScroll, 0, cxVScroll, m_cyHeader);

		m_Tree.MoveWindow(-x, m_cyHeader, cx, cy-m_cyHeader-(bHScrollVisible?cyHScroll:0));
		
		m_horScroll.MoveWindow(0, rcClient.Height()-cyHScroll,
			rcClient.Width() - (bVScrollVisible?cxVScroll:0), cyHScroll);

		
		// show the second header at the top-right corner 
		// only when vertical scroll bar is visible
		m_Header2.ShowWindow(bVScrollVisible?SW_SHOW:SW_HIDE);

		RedrawWindow();
	}
}

int CColumnTreeCtrl::InsertNewColumn(_In_ int nCol, _In_ LPCTSTR lpszColumnHeading,
									 _In_opt_ int nFormat/* = 0*/, _In_opt_ int nWidth/* = -1*/, _In_opt_ int nSubItem/* = -1*/)
{
	// update the header control in upper-right corner
	// to make it look the same way as main header

	CHeaderCtrl& header = GetHeaderCtrl();

	HDITEM hditem;
	hditem.mask = HDI_TEXT | HDI_WIDTH | HDI_FORMAT;
	hditem.fmt = nFormat;
	hditem.cxy = nWidth;
	hditem.pszText = (LPTSTR)lpszColumnHeading;
	m_arrColFormats[nCol] = nFormat;
	int indx =  header.InsertItem(nCol, &hditem);

	if(m_Header.GetItemCount()>0) 
	{
		// if the main header contains items, 
		// insert an item to m_Header2
		hditem.pszText = _T("");
		hditem.cxy = GetSystemMetrics(SM_CXVSCROLL)+5;
		m_Header2.InsertItem(0,&hditem);
	};
	UpdateColumns();
	
	return indx;
}

int CColumnTreeCtrl::InsertColumn(_In_ int nCol, _In_ LPCTSTR lpszColumnHeading, _In_opt_ int nFormat /*= 0*/, _In_opt_ int nWidth /*= -1*/, _In_opt_ ColumnSortType Type /*= CLMNS_NOSORT*/)
{
	m_Tree.m_ColumnData[nCol].Name = lpszColumnHeading;
	m_Tree.m_ColumnData[nCol].SortType = Type;

	return InsertNewColumn(nCol, lpszColumnHeading, nFormat, nWidth);
}

BOOL  CColumnTreeCtrl::DeleteColumn(int nCol)
{
	// update the header control in upper-right corner
	// to make it look the same way as main header

	BOOL bResult = m_Header.DeleteItem(nCol);
	if(m_Header.GetItemCount()==0) 
	{
		m_Header2.DeleteItem(0);
	}

	UpdateColumns();
	return bResult;
}

int CColumnTreeCtrl::GetColumnWidth(int nCol)
{
	HDITEM item;
	item.mask = HDI_WIDTH;
	if (m_Header.GetItem(nCol, &item))
		return item.cxy;
	return 0;
}

CString CColumnTreeCtrl::GetItemText(HTREEITEM hItem, int nSubItem, _In_opt_ bool DefaultItem /*= true*/)
{
	if (hItem == nullptr)
		return _T("");

	// если подали изначального элемента вычисляем дескриптор текущего элемента
	if (DefaultItem)
		hItem = GetCurrentItemFromDefault(hItem);

	CString sText = m_Tree.GetItemText(hItem);
	CString sSubItem;
	AfxExtractSubString(sSubItem, sText, nSubItem, '\t');

	return sSubItem;
}

void CColumnTreeCtrl::SetItemText(_In_ HTREEITEM hItem, _In_ int nColumn, _In_ LPCTSTR lpszText, _In_opt_ bool DefaultItem /*= false*/,
								  _In_opt_ ItemType ControlType /*= CLMN_TEXT*/, _In_opt_ COLORREF Color /*= RGB(255, 255, 255)*/)
{
	// если подали изначального элемента вычисляем дескриптор текущего элемента
	if (DefaultItem)
		hItem = GetCurrentItemFromDefault(hItem);

	SetColumnText(hItem, nColumn, lpszText);

	// ищем по какому индексу устанавливаются данные
	auto it = std::find_if(m_Tree.m_CellData[nColumn].begin(), m_Tree.m_CellData[nColumn].end(), [&hItem](const CCustomTreeChildCtrl::CellData &Val)
	{
		return Val.CurrenthItem == hItem;
	});

	// если элемент небыл найден, ищем его в текущих элементаз
	if (it != m_Tree.m_CellData[nColumn].end())
		it->ChangeStruct(it->Index, lpszText, ControlType, Color, &m_Tree);		
}

void CColumnTreeCtrl::SetItemText(_In_ int Index, _In_ int nColumn, _In_ LPCTSTR lpszText, _In_opt_ bool DefaultItem /*= true*/,
								  _In_opt_ ItemType ControlType /*= CLMN_TEXT*/, _In_opt_ COLORREF Color /*= RGB(255, 255, 255)*/)
{
	SetItemText(GetItemByIndex(Index, !DefaultItem), nColumn, lpszText, DefaultItem, ControlType, Color);
}

CWnd* CColumnTreeCtrl::GetItemControl(_In_ HTREEITEM hItem, _In_ int nColumn, _In_opt_ bool DefaultItem /*= true*/)
{
	// если подали изначального элемента вычисляем дескриптор текущего элемента
	if (DefaultItem)
		hItem = GetCurrentItemFromDefault(hItem);

	// ищем по какому индексу устанавливаются данные
	auto it = std::find_if(m_Tree.m_CellData[nColumn].begin(), m_Tree.m_CellData[nColumn].end(), [&hItem](const CCustomTreeChildCtrl::CellData &Val)
	{
		return Val.CurrenthItem == hItem;
	});

	// если элемент небыл найден, ищем его в текущих элементаз
	if (it != m_Tree.m_CellData[nColumn].end())
	{
		if (it->IsControlExist())
			return it->ControlCWnd;
		else
			return nullptr;
	}
	else
		return nullptr;
}

CWnd* CColumnTreeCtrl::GetItemControl(_In_ int Index, _In_ int nColumn, _In_opt_ bool DefaultItem /*= true*/)
{
	return GetItemControl(GetItemByIndex(Index, !DefaultItem), nColumn, DefaultItem);
}

void CColumnTreeCtrl::SetColumnText(_In_ HTREEITEM hItem, _In_ int nColumn, _In_ LPCTSTR lpszText)
{
	if (hItem == nullptr)
		return;

	CString sText = m_Tree.GetItemText(hItem);
	CString sNewText(_T("")), sSubItem(_T(""));
	for (int32_t i = 0; i < m_Header.GetItemCount(); ++i)
	{
		AfxExtractSubString(sSubItem, sText, i, '\t');
		if (i != nColumn)
			sNewText += sSubItem + _T("\t");
		else
			sNewText += CString(lpszText) + _T("\t");
	}
	m_Tree.SetItemText(hItem, sNewText);
}

void CColumnTreeCtrl::SetFirstColumnMinWidth(UINT uMinWidth)
{
	// set minimum width value for the first column
	m_uMinFirstColWidth = uMinWidth;
}

// Call this function to determine the location of the specified point 
// relative to the client area of a tree view control.
HTREEITEM CColumnTreeCtrl::HitTest(CPoint pt, UINT* pFlags) const
{
	CTVHITTESTINFO htinfo = {pt, 0, NULL, 0};
	HTREEITEM hItem = HitTest(&htinfo);
	if(pFlags)
	{
		*pFlags = htinfo.flags;
	}
	return hItem;
}

// Call this function to determine the location of the specified point 
// relative to the client area of a tree view control.
HTREEITEM CColumnTreeCtrl::HitTest(CTVHITTESTINFO* pHitTestInfo) const
{
	// We should use our own HitTest() method, because our custom tree
	// has different layout than the original CTreeCtrl.

	UINT uFlags = 0;
	HTREEITEM hItem = NULL;
	
	CPoint point = pHitTestInfo->pt;
	point.x += m_xPos;
	point.y -= m_cyHeader;

	hItem = m_Tree.HitTest(point, &uFlags);

	// Fill the CTVHITTESTINFO structure
	pHitTestInfo->hItem = hItem;
	pHitTestInfo->flags = uFlags;
	pHitTestInfo->iSubItem = 0;
		
	if (! (uFlags&TVHT_ONITEMLABEL || uFlags&TVHT_ONITEMRIGHT) )
		return hItem;

	// Additional check for item's label.
	// Determine correct subitem's index.

	int i;
	int x = 0;
	for(i=0; i<m_Header.GetItemCount(); i++)
	{
		if(point.x>=x && point.x<=x+m_Tree.m_arrColWidths[i])
		{
			pHitTestInfo->iSubItem = i;
			pHitTestInfo->flags = TVHT_ONITEMLABEL;
			return hItem;
		}
		x += m_Tree.m_arrColWidths[i];
	}	
	
	pHitTestInfo->hItem = NULL;
	pHitTestInfo->flags = TVHT_NOWHERE;
	
	return NULL;
}

BOOL CColumnTreeCtrl::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult)
{
	// we need to forward all notifications to the parent window,
	// so use OnNotify() to handle all notifications in one step
	
	LPNMHDR pHdr = (LPNMHDR)lParam;
	
	// there are several notifications we need to precess
	if(pHdr->code==NM_CUSTOMDRAW && pHdr->idFrom == TreeID)
	{
		OnTreeCustomDraw(pHdr,pResult);
		return TRUE; // do not forward 
	}

	if (pHdr->code == TVN_ITEMCHANGED && pHdr->idFrom == TreeID)
	{
		OnTreeItemChanged(pHdr, pResult);
	}

	if(pHdr->code==HDN_ITEMCHANGING && pHdr->idFrom == HeaderID)
	{
		OnHeaderItemChanging(pHdr,pResult);
		return TRUE; // do not forward
	}
	
	if(pHdr->code==HDN_ITEMCHANGED && pHdr->idFrom == HeaderID)
	{
		OnHeaderItemChanged(pHdr,pResult);
		return TRUE; // do not forward
	}

//#ifdef _OWNER_DRAWN_TREE
	
	if(pHdr->code==TVN_ITEMEXPANDING && pHdr->idFrom == TreeID)
	{
		// avoid bitmap scrolling 
	//	Invalidate(); // ... and forward
	}

//#endif //_OWNER_DRAWN_TREE

	if(pHdr->code==TVN_ITEMEXPANDED && pHdr->idFrom == TreeID)
	{
		RepositionControls(); // ... and forward

		TVITEMW tvItemChange = reinterpret_cast<LPNMTREEVIEW>(pHdr)->itemNew;
		if (!IsItemExpanded(tvItemChange.hItem))
			m_Tree.HideChildCtrls(tvItemChange.hItem);
	}

	// forward notifications from children to the control owner
	pHdr->hwndFrom = GetSafeHwnd();
	pHdr->idFrom = GetWindowLong(GetSafeHwnd(),GWL_ID);


//	if (this->m_hWnd->unused != 0/ *nullptr* /)
//	this->SendMessage(WM_NOTIFY, wParam, lParam);

	return (BOOL)GetParent()->SendMessage(WM_NOTIFY, wParam, lParam);	
}

void CColumnTreeCtrl::OnCancelMode()
{
	m_Header.SendMessage(WM_CANCELMODE);
	m_Header2.SendMessage(WM_CANCELMODE);
	m_Tree.SendMessage(WM_CANCELMODE);
	m_horScroll.SendMessage(WM_CANCELMODE);	
}

void CColumnTreeCtrl::OnEnable(BOOL bEnable)
{
	m_Header.EnableWindow(bEnable);
	m_Header2.EnableWindow(bEnable);
	m_Tree.EnableWindow(bEnable);
	m_horScroll.EnableWindow(bEnable);
}

HTREEITEM CColumnTreeCtrl::NextItem(HTREEITEM hItem)
{
	return m_Tree.NextItem(hItem);
}

LRESULT CColumnTreeCtrl::OnClickButton(WPARAM wParam, LPARAM lParam)
{
	GetParent()->PostMessageW(WM_BUTTON_PRESSED, (WPARAM)GetItemByIndex(wParam), wParam);
	return 0;
}

void CColumnTreeCtrl::OnSelectAll()
{
	int lCheck = m_CheckSelectAll.GetCheck();
	BOOL nCheck = 0;

	switch (lCheck)
	{
		// Всё включено, всё выключаем
		case 1: nCheck = FALSE; break;
			// По крайней мере один элемент включен, всё включаем
		case 2: nCheck = TRUE; break;
			// Всё выключено, включаем
		default: nCheck = TRUE; break;
	}

	HTREEITEM hItem = m_Tree.GetRootItem();
	do
	{
		m_Tree.SetCheck(hItem, nCheck);
	} while ((hItem = NextItem(hItem)) != NULL);

	RedrawWindow();
}

afx_msg LRESULT CColumnTreeCtrl::OnHeaderClicked(WPARAM wParam, LPARAM lParam)
{
	if (m_Tree.m_ColumnData[lParam].SortType == CLMNS_NOSORT)
		return 0;

	// устанавливаем всем колонкам такст по умолчанию
	StopSort();

	// устанавливаем что сортироваться будет выбранная колонка
	m_Tree.m_SortByColumn = lParam;
	ChangeColumnSort(lParam);
	Resort();
	return 0;
}

void CColumnTreeCtrl::StopSort()
{
	for (unsigned int i = 0; i < m_Tree.m_ColumnData.size(); i++)
	{
		HDITEM hditem;
#ifdef DEBUG
		hditem.mask = HDI_TEXT | HDI_WIDTH | HDI_FORMAT;
		m_Header.GetItem(i, &hditem);
#else
		hditem.mask = HDI_TEXT;
#endif // DEBUG

		m_bHeaderChangesBlocked = FALSE;
		hditem.pszText = (LPWSTR)m_Tree.m_ColumnData[i].Name.GetString();
		m_Header.SetItem(i, &hditem);
	}
}

bool CColumnTreeCtrl::ChangeColumnSort(_In_ const unsigned &_Column)
{
	for (unsigned int Column = 0, Count_Columns = m_Tree.m_ColumnData.size(); Column < Count_Columns; Column++)
	{
		if (Column == _Column)
			m_Tree.m_ColumnData[Column].SortPriority = m_Tree.m_ColumnData[Column].SortPriority == 1 ? -1 : m_Tree.m_ColumnData[Column].SortPriority + 1;
		else
			m_Tree.m_ColumnData[Column].SortPriority = 0;
	}

	m_bHeaderChangesBlocked = FALSE;

	HDITEM hditem;
#ifdef DEBUG
	hditem.mask = HDI_TEXT | HDI_WIDTH | HDI_FORMAT;
	m_Header.GetItem(_Column, &hditem);
#else
	hditem.mask = HDI_TEXT;
#endif // DEBUG

	CString TempString;
	switch (m_Tree.m_ColumnData[_Column].SortPriority)
	{
		case 1:
		{
			TempString = L"v " + m_Tree.m_ColumnData[_Column].Name;
			hditem.pszText = (LPWSTR)TempString.GetString();
			m_Header.SetItem(_Column, &hditem);
			break;
		}
		case -1:
		{
			TempString = L"^ " + m_Tree.m_ColumnData[_Column].Name;
			hditem.pszText = (LPWSTR)TempString.GetString();
			m_Header.SetItem(_Column, &hditem);
			break;
		}
	}

	if (m_Tree.m_ColumnData[_Column].SortPriority == 1)
		return true;
	else
		return false;
}

void CColumnTreeCtrl::CustomSortItem(HTREEITEM item, int Column)
{
	if (item != NULL)
	{
		if (item == TVI_ROOT || m_Tree.ItemHasChildren(item))
		{
			HTREEITEM child = m_Tree.GetChildItem(item);
			while (child != NULL)
			{
				CustomSortItem(child, Column);
				child = m_Tree.GetNextItem(child, TVGN_NEXT);
			}

			TVSORTCB tvs;
			tvs.hParent = item;

			switch (m_Tree.m_ColumnData[Column].SortType)
			{
				case CLMNS_STRING:
					tvs.lpfnCompare = SortColumnWithText;
					break;
				case CLMNS_NUMERIC:
					tvs.lpfnCompare = SortColumnWithNumber;
					break;
				default:
					break;
			}

			tvs.lParam = reinterpret_cast<LPARAM>(&m_Tree);
			m_Tree.SortChildrenCB(&tvs);
		}
	}
}

int CALLBACK CColumnTreeCtrl::SortColumnWithText(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CCustomTreeChildCtrl* pmyTreeCtrl = (CCustomTreeChildCtrl*)lParamSort;

	CString strItem1(_T(""));
	CString strItem2(_T(""));

	HTREEITEM hRoot = pmyTreeCtrl->GetRootItem();
	while (hRoot)
	{
		LPARAM temp = pmyTreeCtrl->GetItemData(hRoot);
		if (temp == lParam1)
			strItem1 = pmyTreeCtrl->GetItemText(hRoot);

		if (temp == lParam2)
			strItem2 = pmyTreeCtrl->GetItemText(hRoot);

		if (!strItem1.IsEmpty() && !strItem2.IsEmpty())
			break;

		hRoot = pmyTreeCtrl->NextItem(hRoot);
	}

	if (pmyTreeCtrl->m_ColumnData[pmyTreeCtrl->m_SortByColumn].SortPriority != 0)
	{
		CString firstItem;
		CString secondItem;
		AfxExtractSubString(firstItem,  strItem1, pmyTreeCtrl->m_SortByColumn, '\t');
		AfxExtractSubString(secondItem, strItem2, pmyTreeCtrl->m_SortByColumn, '\t');

		switch (pmyTreeCtrl->m_ColumnData[pmyTreeCtrl->m_SortByColumn].SortPriority)
		{
			case 1:
			{
				return firstItem.CompareNoCase(secondItem);
				break;
			}
			case -1:
			{
				return -1 * firstItem.CompareNoCase(secondItem);
				break;
			}
			default:
				return 0;
		}
	}
	else
	{
		// возвращаем элементы на их места
		if (lParam1 >= lParam2)
			return 1;
		else
			return -1;
	}
}

int CALLBACK CColumnTreeCtrl::SortColumnWithNumber(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CCustomTreeChildCtrl* pmyTreeCtrl = (CCustomTreeChildCtrl*)lParamSort;

	CString strItem1(_T(""));
	CString strItem2(_T(""));

	HTREEITEM hRoot = pmyTreeCtrl->GetRootItem();
	while (hRoot)
	{
		LPARAM temp = pmyTreeCtrl->GetItemData(hRoot);
		if (temp == lParam1)
			strItem1 = pmyTreeCtrl->GetItemText(hRoot);

		if (temp == lParam2)
			strItem2 = pmyTreeCtrl->GetItemText(hRoot);

		if (!strItem1.IsEmpty() && !strItem2.IsEmpty())
			break;

		hRoot = pmyTreeCtrl->NextItem(hRoot);
	}

	if (pmyTreeCtrl->m_ColumnData[pmyTreeCtrl->m_SortByColumn].SortPriority != 0)
	{
		CString firstItem;
		CString secondItem;
		AfxExtractSubString(firstItem,  strItem1, pmyTreeCtrl->m_SortByColumn, '\t');
		AfxExtractSubString(secondItem, strItem2, pmyTreeCtrl->m_SortByColumn, '\t');

		int res;
		if (_wtoi(firstItem) < _wtoi(secondItem))
			res = -1;
		else if (_wtoi(firstItem) > _wtoi(secondItem))
			res = 1;
		else
			res = 0;

		switch (pmyTreeCtrl->m_ColumnData[pmyTreeCtrl->m_SortByColumn].SortPriority)
		{
			case 1:
			{
				return res;
				break;
			}
			case -1:
			{
				return -1 * res;
				break;
			}
			default:
				return 0;
		}
	}
	else
	{
		// возвращаем элементы на их места
		if (lParam1 >= lParam2)
			return 1;
		else
			return -1;
	}
}

void CColumnTreeCtrl::FindItems(CString _SearchString)
{
	// загружаем все элементы дерева, если они уже удалялись
	RestoreTreeItems();		

	// удаляем элементы которые не совпали с критериями поиска
	if (!_SearchString.IsEmpty())
	{
		_SearchString = _SearchString.MakeLower();
		RemoveDesiredElements(m_Tree.GetRootItem(), _SearchString);
	}

	Resort();
}

HTREEITEM FindItem(const CString& name, CTreeCtrl& tree, HTREEITEM hRoot)
{
	// check whether the current item is the searched one
	CString text = tree.GetItemText(hRoot);
	if (text.MakeLower().Find(name) != -1)
		return hRoot;

	// get a handle to the first child item
	HTREEITEM hSub = tree.GetChildItem(hRoot);
	// iterate as long a new item is found
	while (hSub)
	{
		// check the children of the current item
		HTREEITEM hFound = FindItem(name, tree, hSub);
		if (hFound)
			return /*tree.GetParentItem*/hFound;

		// get the next sibling of the current item
		hSub = tree.GetNextSiblingItem(hSub);
	}

	// return NULL if nothing was found
	return NULL;
}

void CColumnTreeCtrl::RemoveDesiredElements(_In_ HTREEITEM _hStartItem, _In_ const CString &_SearchString)
{
	HTREEITEM hItemToDelete;
	HTREEITEM hFound;

	while (_hStartItem)
	{
		hFound = FindItem(_SearchString, m_Tree, _hStartItem);

		if (!hFound)
		{
			hItemToDelete = _hStartItem;
			_hStartItem = m_Tree.GetNextSiblingItem(_hStartItem);
			m_Tree.DeleteItem(hItemToDelete);
			
			// заменяем в векторе с информацией о каждой ячейке состояние чекбокса
			for (unsigned Column = 0; Column < MAX_COLUMN_COUNT; Column++)
			{
				auto it = std::find_if(m_Tree.m_CellData[Column].begin(), m_Tree.m_CellData[Column].end(), [&hItemToDelete](const CCustomTreeChildCtrl::CellData &Val)
				{
					return Val.CurrenthItem == hItemToDelete;
				});

				if (it != m_Tree.m_CellData[Column].end())
					it->ShowWindow(SW_HIDE);				
			}
			continue;
		}
		else if (hFound == _hStartItem)
		{
			_hStartItem = m_Tree.GetNextSiblingItem(_hStartItem);
			continue;
		}

		if (m_Tree.ItemHasChildren(_hStartItem))
			RemoveDesiredElements(m_Tree.GetChildItem(_hStartItem), _SearchString);

		_hStartItem = m_Tree.GetNextSiblingItem(_hStartItem);
	}
}

void CColumnTreeCtrl::RestoreTreeItems()
{
	m_Tree.DeleteAllItems();

	for (unsigned Column = 0; Column < MAX_COLUMN_COUNT; Column++)
	{
		for (unsigned Index = 0, MaxSize = m_Tree.m_CellData[Column].size(); Index < MaxSize; Index++)
		{			
			// вставляем элементы
			if (Column == 0)
			{
				// если элемент был основным то ставим его на место
				if (m_Tree.m_CellData[Column][Index].ParrenthItem == TVI_ROOT)
				{
					m_Tree.m_CellData[Column][Index].CurrenthItem = m_Tree.InsertItem(m_Tree.m_CellData[Column][Index].Name);
					m_Tree.SetCheck(m_Tree.m_CellData[Column][Index].CurrenthItem, m_Tree.m_CellData[Column][Index].bSelected);
				}
				else
				{
					// элемент был дочерним элементу к предыдущему, ищем к какому элементу его сейчас вставить
					for (int NewIndex = Index - 1; NewIndex >= 0; NewIndex--)
					{
						if (m_Tree.m_CellData[Column][NewIndex].DefaulthItem == m_Tree.m_CellData[Column][Index].ParrenthItem)
						{
							m_Tree.m_CellData[Column][Index].CurrenthItem = m_Tree.InsertItem(m_Tree.m_CellData[Column][Index].Name, m_Tree.m_CellData[Column][NewIndex].CurrenthItem);
							m_Tree.SetCheck(m_Tree.m_CellData[Column][Index].CurrenthItem, m_Tree.m_CellData[Column][Index].bSelected);
							break;
						}
					}
				}
			}
			else      // заносим в ячейки текст
			{
				m_Tree.m_CellData[Column][Index].CurrenthItem = m_Tree.m_CellData.front()[Index].CurrenthItem;
				SetColumnText(m_Tree.m_CellData[Column][Index].CurrenthItem, Column, m_Tree.m_CellData[Column][Index].Name);
			}
			
			// если родитель свернут то прячем контрол
			if (!IsItemExpanded(m_Tree.GetParentItem(m_Tree.m_CellData[Column][Index].CurrenthItem)))
				m_Tree.m_CellData[Column][Index].MoveWindow(CRect());
			// если был вставлен контрол, то показываем его
			m_Tree.m_CellData[Column][Index].ShowWindow(SW_SHOW);
		}		
	}
}

void CColumnTreeCtrl::DeleteChildrens(HTREEITEM hItem)
{
	HTREEITEM hRes;

	if (m_Tree.ItemHasChildren(hItem))
	{
		hRes = m_Tree.GetChildItem(hItem);
		while ((hItem = m_Tree.GetNextSiblingItem(hRes)) != NULL)
			m_Tree.DeleteItem(hItem);			

		m_Tree.DeleteItem(hRes);
	}
}

void CColumnTreeCtrl::DeleteAllChildrens()
{
	HTREEITEM hItem = m_Tree.GetRootItem();
	
	while (hItem != NULL)
	{
		if (m_Tree.ItemHasChildren(hItem))
			DeleteChildrens(hItem);

		hItem = m_Tree.GetNextSiblingItem(hItem);
	}
}

void CColumnTreeCtrl::DisableAllChildrens()
{
	HTREEITEM hItem = m_Tree.GetRootItem();
	HTREEITEM hRes;
	while (hItem != NULL)
	{
		if (m_Tree.ItemHasChildren(hItem))
		{
			hRes = m_Tree.GetChildItem(hItem);
			m_Tree.SetItemStateEx(hRes, TVIS_EX_DISABLED);
			while ((hRes = m_Tree.GetNextSiblingItem(hRes)) != NULL)
			{
				m_Tree.SetItemStateEx(hRes, TVIS_EX_DISABLED);
			}
		}

		hItem = m_Tree.GetNextSiblingItem(hItem);
	}
}

void CColumnTreeCtrl::ActivateChildrens(HTREEITEM hItem)
{
	HTREEITEM hRes;

	if (m_Tree.ItemHasChildren(hItem))
	{
		hRes = m_Tree.GetChildItem(hItem);
		m_Tree.SetItemStateEx(hRes, TVIS_EX_FLAT);
		while ((hRes = m_Tree.GetNextSiblingItem(hRes)) != NULL)
		{
			m_Tree.SetItemStateEx(hRes, TVIS_EX_FLAT);
		}
	}
}

int CColumnTreeCtrl::IsChildChecked(HTREEITEM hItem, int ChildIndex)
{
	int CountChildElements;
	if (m_Tree.ItemHasChildren(hItem))
	{
		HTREEITEM hRes;
		hRes = m_Tree.GetChildItem(hItem);
		CountChildElements = 0;
		while (CountChildElements != ChildIndex)
		{
			if ((hRes = m_Tree.GetNextSiblingItem(hRes)) != NULL)
				CountChildElements++;
			else
				return 0;
		}
		return m_Tree.GetCheck(hRes);
	}
	return 0;
}

void CColumnTreeCtrl::DeleteAllItems()
{
	GetTreeCtrl().DeleteAllItems();

	for (auto &it : m_Tree.m_CellData)
		it.clear();
	NameSpaceID::g_CurrentID = 36000;
}

void CColumnTreeCtrl::DeleteAll()
{
	int nColsCnt = m_Header.GetItemCount();

	for (int i = nColsCnt - 1; i >= 0; i--)
		DeleteColumn(i);

	DeleteAllItems();
}

BOOL CColumnTreeCtrl::DeleteItem(HTREEITEM hItem, _In_opt_ bool DefaultItem /*= true*/)
{
	// если подали изначального элемента вычисляем дескриптор текущего элемента
	if (DefaultItem)
		hItem = GetCurrentItemFromDefault(hItem);

	for (auto &Column : m_Tree.m_CellData)
	{
		auto it = std::find_if(Column.begin(), Column.end(), [&hItem](const CCustomTreeChildCtrl::CellData &Val)
		{
			return Val.CurrenthItem == hItem;
		});

		if (it != std::end(Column))
			Column.erase(it);
	}
	
	return m_Tree.DeleteItem(hItem);
}

HTREEITEM CColumnTreeCtrl::InsertItem(_In_z_ LPCTSTR lpszItem,
									  _In_opt_ HTREEITEM hParent /*= TVI_ROOT*/,
									  _In_opt_ HTREEITEM hInsertAfter /*= TVI_LAST*/,
									  _In_opt_ BOOL bCheck /*= FALSE*/)
{
	if (hParent == NULL)
		hParent = TVI_ROOT;
	if (hInsertAfter == NULL)
		hInsertAfter = TVI_LAST;
	HTREEITEM hItemNew = m_Tree.InsertItem(lpszItem, hParent, hInsertAfter);
	size_t CountElements = m_Tree.m_CellData.front().size();

	// вычисляем на какой индекс встал новый элемент
	int CurrentIndex = GetItemIndex(hItemNew, true);
	// экземпляр ячейки
	CCustomTreeChildCtrl::CellData NewCellInfo(CurrentIndex, lpszItem, hItemNew, hParent);
	for (unsigned Column = 0; Column < MAX_COLUMN_COUNT; Column++)
	{
		if (Column != 0)	// для всех колонок кроме первой убираем название
			NewCellInfo.Name.Empty();

		// вставляем не в конце
		if (CountElements != CurrentIndex)
		{
			// вставляем элемент на его текущее место
			auto it = m_Tree.m_CellData[Column].insert(m_Tree.m_CellData[Column].begin() + CurrentIndex, NewCellInfo);

			unsigned Index = CountElements;
			// меняем идентификаторы текущим элементам
			for (auto Iter = --m_Tree.m_CellData[Column].end(); Iter != it; --Iter)
			{
				Iter->ChangeID(Index);
				Index--;
			}
		}
		else
			m_Tree.m_CellData[Column].push_back(NewCellInfo);
	}

	m_Tree.SetCheck(hItemNew, bCheck);
	return hItemNew;
}

int CColumnTreeCtrl::InsertItem(_In_ int Index, _In_ LPCTSTR lpszItem, _In_opt_ BOOL bCheck /*= FALSE*/)
{
	HTREEITEM hItemNew;

	if (Index <= 0)
	{
		Index = 0;
		hItemNew = TVI_LAST;
	}
	else
		hItemNew = GetItemByIndex(Index - 1, true);

	InsertItem(lpszItem, TVI_ROOT, hItemNew, bCheck);
	return Index;
}

void CColumnTreeCtrl::SetColumnWidth(_In_ int nCol, _In_ int cx)
{
	HDITEM hditem;
	hditem.mask = HDI_WIDTH;

	if (m_Header.GetItem(nCol, &hditem))
	{
		m_bHeaderChangesBlocked = FALSE;
		hditem.cxy = cx;
		m_Header.SetItem(nCol, &hditem);
	}
}

bool CColumnTreeCtrl::IsChecked(_In_ unsigned Index, _In_ HTREEITEM hParent /*= TVI_ROOT*/)
{
	HTREEITEM hRes;
	
	if ((hRes = m_Tree.GetChildItem(hParent)) != NULL)
	{
		unsigned CurrIndex(0);
		do
		{
			if (CurrIndex == Index)
				return m_Tree.GetCheck(hRes) != FALSE? true : false;

			CurrIndex++;
		} while ((hRes = m_Tree.GetNextSiblingItem(hRes)) != NULL);
	}
	return false;
}

BOOL CColumnTreeCtrl::GetCheck(_In_ unsigned Index, _In_ HTREEITEM hParent /*= TVI_ROOT*/)
{
	return IsChecked(Index, hParent) ? TRUE : FALSE;
}

BOOL CColumnTreeCtrl::SetCheck(_In_ unsigned Index, _In_ BOOL Check, _In_ HTREEITEM hParent /*= TVI_ROOT*/)
{
	HTREEITEM hRes;

	if ((hRes = m_Tree.GetChildItem(hParent)) != NULL)
	{
		unsigned CurrIndex(0);
		do 
		{
			if (CurrIndex == Index)
				return m_Tree.SetCheck(hRes, Check);
		
			CurrIndex++;
		} while ((hRes = m_Tree.GetNextSiblingItem(hRes)) != NULL);

	}

	return FALSE;
}

BOOL CColumnTreeCtrl::IsItemExpanded(HTREEITEM hItem)
{
	if (hItem == TVI_ROOT)
		return TRUE;
	if (hItem == NULL)
		return FALSE;

	if (m_Tree.GetItemState(hItem, TVIS_EXPANDED) & TVIS_EXPANDED)
		return TRUE;
	else
		return FALSE;
}

UINT CColumnTreeCtrl::GetItemIndex(_In_ HTREEITEM hItem, _In_opt_ bool CurrentItem /*= false*/)
{
	UINT Index;
	if (CurrentItem)
		Index = m_Tree.GetItemIndex(hItem);
	else
	{
		// ищем такой элементв сохраненных начальных дескрипторах
		auto it = std::find_if(m_Tree.m_CellData.front().begin(), m_Tree.m_CellData.front().end(), [&hItem](const CCustomTreeChildCtrl::CellData &Val)
		{
			return Val.DefaulthItem == hItem;
		});

		// если элемент небыл найден, ищем его в текущих элементаз
		if (it == m_Tree.m_CellData.front().end())
		{
			it = std::find_if(m_Tree.m_CellData.front().begin(), m_Tree.m_CellData.front().end(), [&hItem](const CCustomTreeChildCtrl::CellData &Val)
			{
				return Val.CurrenthItem == hItem;
			});
		}

		if (it != m_Tree.m_CellData.front().end())
			Index = it->Index;
		else
			Index = m_Tree.GetItemIndex(hItem);
	}

	return Index;
}

HTREEITEM CColumnTreeCtrl::GetItemByIndex(_In_ UINT Index, _In_opt_ bool CurrentItem /*= false*/)
{
	HTREEITEM Res;
	if (CurrentItem)
		Res = m_Tree.GetItemByIndex(Index);
	else
	{
		// ищем такой элементв сохраненных начальных дескрипторах
		auto it = std::find_if(m_Tree.m_CellData.front().begin(), m_Tree.m_CellData.front().end(), [&Index](const CCustomTreeChildCtrl::CellData &Val)
		{
			return Val.Index == Index;
		});

		if (it != m_Tree.m_CellData.front().end())
			Res = it->DefaulthItem;
		else
			Res = m_Tree.GetItemByIndex(Index);
	}

	return Res;
}

BOOL CColumnTreeCtrl::ModifyStyle(DWORD dwRemove, DWORD dwAdd, UINT nFlags /*= 0*/)
{
	BOOL Res = m_Tree.ModifyStyle(dwRemove, dwAdd);

	if (m_Tree.GetStyle() & TVS_CHECKBOXES)
		m_CheckSelectAll.ShowWindow(SW_SHOW);
	else
		m_CheckSelectAll.ShowWindow(SW_HIDE);

	return Res;
}

void CColumnTreeCtrl::AutoScaleColumns()
{
	CRect Rect;
	GetClientRect(Rect);

	int CountColumns = m_Header.GetItemCount();

	if (CountColumns != 0)
	{
		int Width = Rect.Width() / CountColumns;
		for (int Column = 0; Column < CountColumns; Column++)
			SetColumnWidth(Column, Width);
	}
}

void CColumnTreeCtrl::Resort()
{
	// восстанавливаем сортировку
	// заносим номера сравниваемых колонок 
	HTREEITEM hRoot = m_Tree.GetRootItem();
	int iIndex(0);
	while (hRoot)
	{
		m_Tree.SetItemData(hRoot, iIndex);
		hRoot = m_Tree.NextItem(hRoot);
		iIndex++;
	}

	CustomSortItem(TVI_ROOT, m_Tree.m_SortByColumn);
}

BOOL CColumnTreeCtrl::IsItemVisible(HTREEITEM hItem)
{
	CRect ControlRect;
	m_Tree.GetClientRect(ControlRect);
	CRect rect;
	m_Tree.GetItemRect(hItem, &rect, TRUE);
	return  (rect.bottom > 0 && rect.bottom < ControlRect.bottom) ||
			(rect.top	 > 0 && rect.top < ControlRect.bottom);	
}

BOOL CColumnTreeCtrl::Expand(_In_ HTREEITEM hItem, _In_ UINT nCode)
{
	if (nCode == TVE_COLLAPSE)
		m_Tree.HideChildCtrls(hItem);
	return m_Tree.Expand(hItem, nCode);
}

HTREEITEM CColumnTreeCtrl::GetCurrentItemFromDefault(_In_ HTREEITEM DefaultItem)
{
	// ищем такой элемент в сохраненных начальных дескрипторах
	auto it = std::find_if(m_Tree.m_CellData.front().begin(), m_Tree.m_CellData.front().end(), [&DefaultItem](const CCustomTreeChildCtrl::CellData &Val)
	{
		return Val.DefaulthItem == DefaultItem;
	});

	HTREEITEM Res;
	if (it != m_Tree.m_CellData.front().end())
		Res = it->CurrenthItem;
	else
		Res = DefaultItem;
	return Res;
}

HTREEITEM CColumnTreeCtrl::GetDefaultItemFromCurrent(_In_ HTREEITEM CurrentItem)
{
	// ищем такой элемент в сохраненных начальных дескрипторах
	auto it = std::find_if(m_Tree.m_CellData.front().begin(), m_Tree.m_CellData.front().end(), [&CurrentItem](const CCustomTreeChildCtrl::CellData &Val)
	{
		return Val.CurrenthItem == CurrentItem;
	});

	HTREEITEM Res;
	if (it != m_Tree.m_CellData.front().end())
		Res = it->DefaulthItem;
	else
		Res = CurrentItem;
	return Res;
}
