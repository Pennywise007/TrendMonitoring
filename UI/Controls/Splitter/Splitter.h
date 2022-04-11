#pragma once

#include <functional>
#include <map>

#include <afx.h>
#include <afxtempl.h>
#include <afxlayout.h>
#include <afxwin.h>

#include <optional>

/**************************************************************************************************
// Using:
// Add Button/Static on form, set CSplitter as object

splitter.AttachSplitterToWindow(m_hWnd, CMFCDynamicLayout::MoveHorizontal(50), CMFCDynamicLayout::SizeVertical(100));
splitter.AttachWindow(m_groupCutFile.m_hWnd, CMFCDynamicLayout::MoveHorizontal(100), CMFCDynamicLayout::SizeHorizontal(-100));
**************************************************************************************************/

class CSplitter : public CWnd
{
public:
    // orientation of control
    enum class Orientation
    {
        eHorizontal,  // --
        eVertical     // |
    };
    CSplitter(_In_opt_ Orientation controlOrientation = Orientation::eVertical);
    // Setting callbacks for handling drug splitter events,
    // if onStartDruggingSplitter return false - cancel moving control
    void SetCallbacks(_In_opt_ const std::function<bool(CSplitter* splitter, CRect& newRect)>& onPosChanging = nullptr,
                      _In_opt_ const std::function<void(CSplitter* splitter)>& onEndDruggingSplitter = nullptr);
    // Setting the cursor from resource file that will be displayed on the buttons when you hover over it
    void SetMovingCursor(_In_ UINT cursorResourceId);
    // Setting changing layout settings on resizing specified window
    void AttachSplitterToWindow(HWND hWnd, CMFCDynamicLayout::MoveSettings moveSettings, CMFCDynamicLayout::SizeSettings sizeSettings);

public: // Managing attached windows, attached windows will be moved/sized with their rules on splitter pos changed
    void AttachWindow(HWND hWnd, CMFCDynamicLayout::MoveSettings moveSettings, CMFCDynamicLayout::SizeSettings sizeSettings);
    void DetachWindow(HWND hWnd);
    void ClearAttachedWindowsList();
public: // Settings control moving bounds
    static constexpr LONG kNotSpecified = -1;
    enum class BoundsType
    {
        eControlBounds,             // setting work area beyond which the control cannot go
        eOffsetFromParentBounds     // setting minimum offset from parent control bounds
    };
    // Setting bounds for moving control, if kNotSpecified then the border is not taken into account
    void SetControlBounds(_In_ BoundsType type, _In_opt_ CRect bounds = CRect(kNotSpecified, kNotSpecified, kNotSpecified, kNotSpecified));
protected:
    // Check and correct new control bounds, move window if necessary
    void ApplyNewRect(_In_ CRect& newRect, _In_opt_ std::optional<CRect> currentRect = std::nullopt);
    // Detaching from window proc to which the splitter is attached
    void UnhookFromAttachedWindow();
    // The callback received when the window is resized to which the splitter is attached
    void OnAttachedWindowSizeChanged(int cx, int cy);

    // changing layout settings for attached window
    struct AttachedWindowsSettings
    {
        AttachedWindowsSettings(CSplitter* splitter, HWND hWnd, CMFCDynamicLayout::MoveSettings&& moveSettings, CMFCDynamicLayout::SizeSettings&& sizeSettings);
        HWND m_hWnd;
        CRect m_initialWindowRect;
        CRect m_initialSlitterRect;
        CMFCDynamicLayout::MoveSettings m_moveSettings;
        CMFCDynamicLayout::SizeSettings m_sizeSettings;
    };
    // apply layout rules to rect
    static void ApplyLayout(CRect& rect, const AttachedWindowsSettings& settings, const CSize& positionDiff);

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg BOOL OnNcCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnPaint();
    afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
    afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

protected:
    std::optional<CPoint> m_drugStartMousePoint;
    const Orientation m_orientation;
    // bounds for control position
    CRect m_bounds = { kNotSpecified, kNotSpecified, kNotSpecified, kNotSpecified };
    BoundsType m_boundsType = BoundsType::eControlBounds;
    std::map<HWND, AttachedWindowsSettings> m_attachedWindowsSettings;
    // callbacks
    std::function<bool(CSplitter* splitter, CRect& newRect)> m_onPosChanging;
    std::function<void(CSplitter* splitter)> m_onEndDruggingSplitter;

private: // splitter layout settings
    WNDPROC m_pfnWndProc;   // default window proc of window which splitter is attached
    // splitter attached window proc
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    std::optional<AttachedWindowsSettings> m_splitterLayoutSettings;
    bool m_movingWithParentBounds = false;

private:
    HCURSOR m_hCursor;
};
