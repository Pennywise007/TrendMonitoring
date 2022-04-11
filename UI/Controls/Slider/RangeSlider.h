#pragma once

#include <afx.h>
#include <afxcmn.h>
#include <vcruntime.h>

#include <optional>
#include <utility>

#include <Controls/ToolWindow/ToolWindow.h>

/*
* Slider with two thumbs. Allow to select range, also shows hint of current value when moving slider
*/
class CRangeSlider : public CWnd
{
public:
    // Sets and retrieves current range of slider
    void SetRange(_In_ std::pair<double, double> range, _In_ bool redraw = true);
    _NODISCARD std::pair<double, double> GetRange() const;

    // Sets increment step for thumb
    void SetIncrementStep(double step);

    // Sets the current logical position of the sliders in the trackbar control.
    void SetPositions(_In_ std::pair<double, double> positions, _In_ bool redraw = true);
    // Retrieves the current logical position of the sliders in the trackbar control.
    _NODISCARD std::pair<double, double> GetPositions() const;

    // Sets the width of the slider in the trackbar control.
    void SetThumbWidth(_In_ unsigned width);
    // Retrieves the width of the slider in the trackbar control.
    _NODISCARD unsigned GetThumbWidth() const;

    // Sets the width of the trackbar.
    void SetLineWidth(_In_ unsigned width);
    // Retrieves the width of the trackbar.
    _NODISCARD unsigned GetLineWidth() const;

    // set format for tooltip text, don`t forget about %lf
    void SetTooltipTextFormat(const wchar_t* format = L"0.02%lf");

protected:
    void PreSubclassWindow() override;
    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg UINT OnGetDlgCode();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()

private:
    void OnPaintHorizontal(CDC& dc);
    void OnPaintVertical(CDC& dc);

    void NormalizePositions();
    void SendChangePositionEvent() const;

    void ShowSliderTooltip(bool left, bool createTip = false);

    double ApplyIncrementStep(double oldValue, double newValue);

private:
    std::optional<double> m_incrementStep;                      // increment step of moving thumb
    std::pair<double, double> m_range = { 0., 1.};              // minimum and maximum positions for track bar
    std::pair<double, double> m_thumbsPosition = m_range;       // thumb positions on track bar
    std::pair<CRect, CRect> m_thumbsRects;                      // rectangles of the Left and Right thumb

    unsigned m_thumbWidth = 8;      // in pixels.
    unsigned m_lineWidth = 4;       // in pixels.

private:
    enum class TrackMode {
        TRACK_LEFT,             // Track left slider
        TRACK_RIGHT,            // Track right slider
        TRACK_MIDDLE,           // Track area between sliders
    };
    std::optional<TrackMode> m_trackMode;
    CPoint m_clickOffsetFormThumbCenter;

private:
    CToolWindow m_tooltip;
    const wchar_t* m_tooltipFormat = L"%lf";
};
