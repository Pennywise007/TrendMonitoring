#include "Layout.h"

#include <afxext.h>
#include <afxwin.h>
#include <cmath>
#include <list>

Layout::~Layout()
{
    ASSERT(m_defaultWindowProc.empty());

    for (auto&& [hWnd, defWindowProc] : m_defaultWindowProc)
    {
        ::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)defWindowProc);
    }
}

Layout& Layout::Instance()
{
    static Layout s;
    return s;
}

void Layout::TryAttachToWindowProc(HWND hWnd)
{
    if (m_defaultWindowProc.find(hWnd) == m_defaultWindowProc.end())
        m_defaultWindowProc[hWnd] = (WNDPROC)::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)WindowProc);
}

void Layout::CheckNecessityToHandleDefProc(HWND hWnd)
{
    if (const auto it = m_defaultWindowProc.find(hWnd); it != m_defaultWindowProc.end())
    {
        if (m_anchoredWindows.find(hWnd) == m_anchoredWindows.end() &&
            m_boundedWindows.find(hWnd) == m_boundedWindows.end() &&
            m_minimumSizeWindows.find(hWnd) == m_minimumSizeWindows.end())
        {
            ::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)it->second);
            m_defaultWindowProc.erase(it);
        }
    }
}

void Layout::AnchorWindow(const CWnd& who, const CWnd& anchorTarget, AnchoredSides&& anchoredSides, AnchorSide anchorTargetSide, int ratio)
{
    ASSERT(who && ::IsWindow(who.m_hWnd));
    ASSERT(anchoredSides.any());
    if (anchorTargetSide == AnchorSide::eLeft || anchorTargetSide == AnchorSide::eRight)
        ASSERT(!anchoredSides.test(static_cast<size_t>(AnchorSide::eTop)) && !anchoredSides.test(static_cast<size_t>(AnchorSide::eBottom)));
    else
        ASSERT(!anchoredSides.test(static_cast<size_t>(AnchorSide::eLeft)) && !anchoredSides.test(static_cast<size_t>(AnchorSide::eRight)));

    auto& anchor = Instance();

    anchor.m_anchoredWindows[anchorTarget.m_hWnd].anchoredWindows[who.m_hWnd].emplace_back(
        AnchorTargetInfo::AnchoredWindowInfo{ std::move(anchoredSides), anchorTargetSide,  ratio,
                                                GetWindowRect(who.m_hWnd), GetWindowRect(anchorTarget.m_hWnd, who.m_hWnd) });

    anchor.TryAttachToWindowProc(anchorTarget.m_hWnd);
}

void Layout::AnchorWindow(const CWnd& who, const CWnd& anchorTarget, const std::initializer_list<AnchorSide>& anchoredSides, AnchorSide anchorTargetSide, int ratio)
{
    AnchoredSides anchoredBits;
    for (const auto& side : anchoredSides)
    {
        anchoredBits.set(static_cast<size_t>(side));
    }
    AnchorWindow(who, anchorTarget, std::move(anchoredBits), anchorTargetSide, ratio);
}

void Layout::AnchorRemove(const CWnd& who, const CWnd& anchorTarget, const std::initializer_list<AnchorSide>& removingAnchoredForSides)
{
    ASSERT(who && ::IsWindow(who.m_hWnd));
    ASSERT(removingAnchoredForSides.size() > 0);

    auto& anchor = Instance();

    const auto targetWindow = anchor.m_anchoredWindows.find(anchorTarget.m_hWnd);
    if (targetWindow == anchor.m_anchoredWindows.end())
        return;

    const auto anchorWindow = targetWindow->second.anchoredWindows.find(who.m_hWnd);
    if (anchorWindow == targetWindow->second.anchoredWindows.end())
        return;

    // remove all anchors for given sides
    for (auto anchorInfo = anchorWindow->second.begin(); anchorInfo != anchorWindow->second.end();)
    {
        for (auto& removingSide : removingAnchoredForSides)
        {
            anchorInfo->anchoredSides.set((size_t)removingSide, false);
        }

        if (anchorInfo->anchoredSides.none())
            anchorInfo = anchorWindow->second.erase(anchorInfo);
        else
            ++anchorInfo;
    }

    if (anchorWindow->second.empty())
    {
        targetWindow->second.anchoredWindows.erase(anchorWindow);

        if (targetWindow->second.anchoredWindows.empty())
        {
            anchor.m_anchoredWindows.erase(targetWindow);
            anchor.CheckNecessityToHandleDefProc(anchorTarget.m_hWnd);
        }
    }
}

void Layout::SetWindowBounds(CWnd& wnd, BoundsType type, std::optional<LONG> left, std::optional<LONG> top, std::optional<LONG> right, std::optional<LONG> bottom)
{
    ASSERT(::IsWindow(wnd.m_hWnd));
    ASSERT(type != BoundsType::eOffsetFromParentBounds || ::IsWindow(wnd.GetParent()->m_hWnd));

    auto& anchor = Instance();

    if (!left.has_value() && !top.has_value() && !right.has_value() && !bottom.has_value())
    {
        const auto it = anchor.m_boundedWindows.find(wnd.m_hWnd);
        if (it != anchor.m_boundedWindows.end())
        {
            anchor.m_boundedWindows.erase(it);
            anchor.CheckNecessityToHandleDefProc(wnd.m_hWnd);
        }
        return;
    }

    BoundInfo& boundInfo = anchor.m_boundedWindows[wnd.m_hWnd];
    boundInfo.type = type;
    boundInfo.left = std::move(left);
    boundInfo.top = std::move(top);
    boundInfo.right = std::move(right);
    boundInfo.bottom = std::move(bottom);

    CRect rect = GetWindowRect(wnd.m_hWnd);
    if (boundInfo.ApplyBoundsToRect(wnd.m_hWnd, rect))
        wnd.MoveWindow(rect);

    anchor.TryAttachToWindowProc(wnd.m_hWnd);
}

void Layout::SetWindowMinimumSize(CWnd& wnd, std::optional<UINT> width, std::optional<UINT> height)
{
    ASSERT(::IsWindow(wnd.m_hWnd));

    auto& anchor = Instance();

    if (!width.has_value() && !height.has_value())
    {
        const auto it = anchor.m_minimumSizeWindows.find(wnd.m_hWnd);
        if (it != anchor.m_minimumSizeWindows.end())
        {
            anchor.m_minimumSizeWindows.erase(it);
            anchor.CheckNecessityToHandleDefProc(wnd.m_hWnd);
        }
        return;
    }

    MinimumSize& sizeInfo = anchor.m_minimumSizeWindows[wnd.m_hWnd];
    sizeInfo.width = std::move(width);
    sizeInfo.height = std::move(height);

    CRect rect;
    ::GetWindowRect(wnd.m_hWnd, rect);
    if (const HWND parent = ::GetParent(wnd.m_hWnd); ::IsWindow(parent))
        CWnd::FromHandle(parent)->ScreenToClient(rect);

    const bool correctWidth = sizeInfo.width.has_value() && rect.Width() < (int)*sizeInfo.width;
    const bool correctHeight = sizeInfo.height.has_value() && rect.Height() < (int)*sizeInfo.height;
    if (correctWidth || correctHeight)
    {
        ::SetWindowPos(wnd.m_hWnd, NULL, NULL, NULL,
                       correctWidth ? *sizeInfo.width : rect.Width(),
                       correctHeight ? *sizeInfo.height : rect.Height(),
                       SWP_NOACTIVATE | SWP_NOMOVE | SWP_DRAWFRAME | SWP_NOZORDER | SWP_NOCOPYBITS);
    }

    anchor.TryAttachToWindowProc(wnd.m_hWnd);
}

LRESULT Layout::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Layout& instance = Instance();

    const auto defaultWndProcIt = instance.m_defaultWindowProc.find(hWnd);
    if (defaultWndProcIt == instance.m_defaultWindowProc.end())
    {
        ASSERT(TRUE);
        return S_FALSE;
    }

    WNDPROC defaultWindowProc = defaultWndProcIt->second;

    switch (uMsg)
    {
    case WM_WINDOWPOSCHANGED:
        {
            WINDOWPOS* lpwndpos = (WINDOWPOS*)lParam;
            if ((lpwndpos->flags & SWP_NOMOVE) && (lpwndpos->flags & SWP_NOSIZE) &&
                !(lpwndpos->flags & SWP_SHOWWINDOW) && !(lpwndpos->flags & SWP_FRAMECHANGED))
                break;

            const auto anchorTargetIt = instance.m_anchoredWindows.find(hWnd);
            if (anchorTargetIt == instance.m_anchoredWindows.end())
                break;

            CWnd::FromHandle(hWnd)->RedrawWindow();

            HDWP hDWP = ::BeginDeferWindowPos(anchorTargetIt->second.anchoredWindows.size());

            for (auto&& [attachedHwnd, anchoredInfo] : anchorTargetIt->second.anchoredWindows)
            {
                ASSERT(!anchoredInfo.empty());

#ifdef DEBUG
                CString str;
                CWnd::FromHandle(attachedHwnd)->GetWindowText(str);
#endif

                const CRect currentTargetRect = GetWindowRect(hWnd, attachedHwnd);
                std::optional<double> deltaLeft, deltaTop, deltaRight, deltaBottom;

                for (auto& anchor : anchoredInfo)
                {
                    ASSERT(anchor.anchoredSides.any());
                    anchor.ApplyAnchors(currentTargetRect, deltaLeft, deltaTop, deltaRight, deltaBottom);
                }

                const auto currentWindowRect = GetWindowRect(attachedHwnd);
                auto getInitialRectForSide = [anchors = &anchoredInfo](AnchorSide side) -> const CRect&
                {
                    const auto it = std::find_if(anchors->cbegin(), anchors->cend(),
                                                 [&side](const AnchorTargetInfo::AnchoredWindowInfo& info)
                                                 {
                                                     return info.anchoredSides.test((size_t)side);
                                                 });
                    ASSERT(it != anchors->cend());
                    return it->initialWindowRect;
                };

                CRect newRect = currentWindowRect;
                if (deltaLeft.has_value())
                    newRect.left = getInitialRectForSide(AnchorSide::eLeft).left + (LONG)round(*deltaLeft);
                if (deltaTop.has_value())
                    newRect.top = getInitialRectForSide(AnchorSide::eTop).top + (LONG)round(*deltaTop);
                if (deltaRight.has_value())
                    newRect.right = getInitialRectForSide(AnchorSide::eRight).right + (LONG)round(*deltaRight);
                if (deltaBottom.has_value())
                    newRect.bottom = getInitialRectForSide(AnchorSide::eBottom).bottom + (LONG)round(*deltaBottom);

                if (newRect != currentWindowRect)
                {
                   /* ::SetWindowPos(attachedHwnd, HWND_TOP, newRect.left, newRect.top,
                                   newRect.Width(), newRect.Height(),
                                   SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS);*/
                    ::DeferWindowPos(hDWP, attachedHwnd, HWND_TOP, newRect.left, newRect.top,
                                     newRect.Width(), newRect.Height(),
                                     SWP_NOZORDER | SWP_NOREPOSITION | SWP_NOACTIVATE | SWP_NOCOPYBITS);
                }
            }

            ::EndDeferWindowPos(hDWP);

        }
        break;
    case WM_WINDOWPOSCHANGING:
        {
            WINDOWPOS* lpwndpos = (WINDOWPOS*)lParam;
            if ((lpwndpos->flags & SWP_NOMOVE) && (lpwndpos->flags & SWP_NOSIZE))
                break;

            const auto boundIt = instance.m_boundedWindows.find(hWnd);
            if (boundIt == instance.m_boundedWindows.end())
                break;

            CRect newControlRect = GetWindowRect(hWnd);
            if (!(lpwndpos->flags & SWP_NOMOVE) && ::IsWindow(GetParent(hWnd)))
            {
                newControlRect.OffsetRect(-newControlRect.TopLeft());
                newControlRect.OffsetRect(lpwndpos->x, lpwndpos->y);
            }
            if (!(lpwndpos->flags & SWP_NOSIZE))
            {
                newControlRect.right = newControlRect.left + lpwndpos->cx;
                newControlRect.bottom = newControlRect.top + lpwndpos->cy;
            }

            if (boundIt->second.ApplyBoundsToRect(hWnd, newControlRect))
            {
                lpwndpos->x = newControlRect.left;
                lpwndpos->y = newControlRect.top;
                lpwndpos->cx = newControlRect.Width();
                lpwndpos->cy = newControlRect.Height();
            }
        }
        break;
    case WM_GETMINMAXINFO:
        {
            if (const auto it = instance.m_minimumSizeWindows.find(hWnd); it != instance.m_minimumSizeWindows.end())
            {
                auto* info = (MINMAXINFO*)lParam;
                POINT minSize = { it->second.width.value_or(MAXLONG64), it->second.height.value_or(MAXLONG64) };
                info->ptMinTrackSize = std::move(minSize);
            }
        }
        break;
    case WM_DESTROY:
        {
            // restore def window proc
            ::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)defaultWindowProc);
            instance.m_defaultWindowProc.erase(defaultWndProcIt);
        }
        break;
    }

    return ::CallWindowProc(defaultWindowProc, hWnd, uMsg, wParam, lParam);
}

CRect Layout::GetWindowRect(HWND hWnd, HWND hAttachingWindow /*= nullptr*/)
{
    CRect rect;
    if (const HWND parent = ::GetParent(hWnd); ::IsWindow(parent) && (!hAttachingWindow || ::GetParent(hAttachingWindow) != hWnd))
    {
        ::GetWindowRect(hWnd, rect);
        CWnd::FromHandle(parent)->ScreenToClient(rect);
    }
    else
    {
        CWnd* pWnd = CWnd::FromHandle(hWnd);
        pWnd->GetClientRect(rect);

        /*if (DYNAMIC_DOWNCAST(CFormView, pWnd) != NULL)
        {
            CPoint ptScroll(((CFormView*)pWnd)->GetScrollPos(SB_HORZ), ((CFormView*)pWnd)->GetScrollPos(SB_VERT));

            rect.InflateRect(0, 0, ptScroll.x, ptScroll.y);
            rect.OffsetRect(-ptScroll.x, -ptScroll.y);
        }*/

        if ((pWnd->GetStyle() & WS_HSCROLL) > 0)
        {
            int nMin, nMax;
            pWnd->GetScrollRange(SB_HORZ, &nMin, &nMax);
            rect.right = rect.left + nMax;
            rect.OffsetRect(-pWnd->GetScrollPos(SB_HORZ), 0);
        }
        if ((pWnd->GetStyle() & WS_VSCROLL) > 0)
        {
            int nMin, nMax;
            pWnd->GetScrollRange(SB_VERT, &nMin, &nMax);
            rect.bottom = rect.top + nMax;
            rect.OffsetRect(0, -pWnd->GetScrollPos(SB_VERT));
        }
    }

    return rect;
}

void Layout::AnchorTargetInfo::AnchoredWindowInfo::ApplyAnchors(const CRect& currentTargetRect,
                                                                std::optional<double>& deltaLeft, std::optional<double>& deltaTop,
                                                                std::optional<double>& deltaRight, std::optional<double>& deltaBottom) const
{
    double changingValue = 0.;
    switch (anchorSide)
    {
    case AnchorSide::eLeft:
        changingValue = currentTargetRect.left - initialTargetRect.left;
        break;
    case AnchorSide::eTop:
        changingValue = currentTargetRect.top - initialTargetRect.top;
        break;
    case AnchorSide::eRight:
        changingValue = currentTargetRect.right - initialTargetRect.right;
        break;
    case AnchorSide::eBottom:
        changingValue = currentTargetRect.bottom - initialTargetRect.bottom;
        break;
    }
    changingValue = changingValue * (double)ratio / 100.;

    if (anchoredSides.test(static_cast<size_t>(AnchorSide::eLeft)))
        deltaLeft = deltaLeft.value_or(0.) + changingValue;
    if (anchoredSides.test(static_cast<size_t>(AnchorSide::eTop)))
        deltaTop = deltaTop.value_or(0.) + changingValue;
    if (anchoredSides.test(static_cast<size_t>(AnchorSide::eRight)))
        deltaRight = deltaRight.value_or(0.) + changingValue;
    if (anchoredSides.test(static_cast<size_t>(AnchorSide::eBottom)))
        deltaBottom = deltaBottom.value_or(0.) + changingValue;
}

bool Layout::BoundInfo::ApplyBoundsToRect(HWND window, CRect& newRect)
{
    CRect controlBounds;
    controlBounds.top = top.value_or(newRect.top);
    controlBounds.left = left.value_or(newRect.left);
    controlBounds.bottom = bottom.value_or(newRect.bottom);
    controlBounds.right = right.value_or(newRect.right);

    if (type == BoundsType::eOffsetFromParentBounds && (right.has_value() || bottom.has_value()))
    {
        CRect parentRect;
        ASSERT(::IsWindow(GetParent(window)));
        ::GetClientRect(GetParent(window), parentRect);
        controlBounds.bottom = bottom.has_value() ? parentRect.bottom - *bottom : newRect.bottom;
        controlBounds.right = right.has_value() ? parentRect.right - *right : newRect.right;
    }

    const CRect currentRect = newRect;

    // validating new rect bounds
    if (newRect.left < controlBounds.left)
        newRect.OffsetRect(controlBounds.left - newRect.left, 0);
    if (newRect.right > controlBounds.right)
        newRect.OffsetRect(controlBounds.right - newRect.right, 0);
    if (newRect.top < controlBounds.top)
        newRect.OffsetRect(0, controlBounds.top - newRect.top);
    if (newRect.bottom > controlBounds.bottom)
        newRect.OffsetRect(0, controlBounds.bottom - newRect.bottom);

    // after the shift, the border can crawl out from other side, reduce the width to the required
    if (newRect.left < controlBounds.left)
        newRect.left = controlBounds.left;
    if (newRect.top < controlBounds.top)
        newRect.top = controlBounds.top;

    return currentRect != newRect;
}

bool LayoutLoader::ApplyLayoutFromResource(CFormView& formView, LPCTSTR lpszResourceName)
{
    return ApplyLayoutFromResourceForWindow(formView, lpszResourceName);
}

bool LayoutLoader::ApplyLayoutFromResource(CDialog& dialog, LPCTSTR lpszResourceName)
{
    // copied from CMFCDynamicLayout::AddItem
    if (CFormView* pFormView = DYNAMIC_DOWNCAST(CFormView, &dialog))
    {
        ASSERT_VALID(pFormView);

        if (pFormView->IsInitDlgCompleted())
        {
            TRACE0("LayoutLoader::ApplyLayoutFromResource failed! Please call this method after calling of CDialog::OnInitDialog().\n");
            ASSERT(FALSE);
            return false;
        }
    }

    return ApplyLayoutFromResourceForWindow(dialog, lpszResourceName);
}

bool LayoutLoader::ApplyLayoutFromResourceForWindow(CWnd& wnd, LPCTSTR lpszResourceName)
{
    ASSERT(wnd.GetSafeHwnd() != NULL);
    ASSERT(::IsWindow(wnd.GetSafeHwnd()));
    ASSERT(lpszResourceName);

    wnd.EnableDynamicLayout(FALSE);

    // copied from CWnd::LoadDynamicLayoutResource

    // find resource handle
    DWORD dwSize = 0;
    LPVOID lpResource = nullptr;
    HGLOBAL hResource = nullptr;
    if (lpszResourceName != nullptr)
    {
        HINSTANCE hInst = AfxFindResourceHandle(lpszResourceName, RT_DIALOG_LAYOUT);
        HRSRC hDlgLayout = ::FindResource(hInst, lpszResourceName, RT_DIALOG_LAYOUT);
        if (hDlgLayout != nullptr)
        {
            // load it
            dwSize = SizeofResource(hInst, hDlgLayout);
            hResource = LoadResource(hInst, hDlgLayout);
            if (hResource == nullptr)
                return false;
            // lock its
            lpResource = LockResource(hResource);
            ASSERT(lpResource != nullptr);
        }
    }

    LayoutLoader loader;
    // Use lpResource
    const BOOL bResult = loader.ReadResource(lpResource, dwSize);

    // cleanup
    if (lpResource != nullptr && hResource != nullptr)
    {
        UnlockResource(hResource);
        FreeResource(hResource);
    }

    if (bResult)
    {
        // copied from CMFCDynamicLayoutData::ApplyLayoutDataTo
        CWnd* pChild = wnd.GetWindow(GW_CHILD);
        POSITION pos = loader.m_listCtrls.GetHeadPosition();
        while (pChild != nullptr && pos != nullptr)
        {
#ifdef DEBUG
            CString str;
            pChild->GetWindowText(str);
#endif
            const Item& item = loader.m_listCtrls.GetNext(pos);

            if (item.m_moveSettings.IsHorizontal())
                Layout::AnchorWindow(*pChild, wnd, { AnchorSide::eLeft, AnchorSide::eRight }, AnchorSide::eRight, item.m_moveSettings.m_nXRatio);
            if (item.m_moveSettings.IsVertical())
                Layout::AnchorWindow(*pChild, wnd, { AnchorSide::eTop, AnchorSide::eBottom}, AnchorSide::eBottom, item.m_moveSettings.m_nYRatio);

            if (item.m_sizeSettings.IsHorizontal())
                Layout::AnchorWindow(*pChild, wnd, { AnchorSide::eRight }, AnchorSide::eRight, item.m_sizeSettings.m_nXRatio);
            if (item.m_sizeSettings.IsVertical())
                Layout::AnchorWindow(*pChild, wnd, { AnchorSide::eBottom }, AnchorSide::eBottom, item.m_sizeSettings.m_nYRatio);

            pChild = pChild->GetNextWindow();
        }
    }

    return true;
}
