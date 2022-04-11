#include "RangeSlider.h"

#include <afxwin.h>
#include <algorithm>
#include <corecrt_math.h>
#include <string>

namespace {

constexpr COLORREF thumbColor = RGB(143, 143, 143);
constexpr COLORREF lineColor = RGB(157, 157, 157);
constexpr COLORREF selectionColor = RGB(0, 120, 215);

COLORREF darker(COLORREF Color, int Percent)
{
    int r = GetRValue(Color);
    int g = GetGValue(Color);
    int b = GetBValue(Color);

    r = r - MulDiv(r, Percent, 100);
    g = g - MulDiv(g, Percent, 100);
    b = b - MulDiv(b, Percent, 100);
    return RGB(r, g, b);
}

_NODISCARD std::wstring get_string(const wchar_t* format, double value)
{
    const int size_s = std::swprintf(nullptr, 0, format, value) + 1; // + '\0'
    if (size_s <= 0) { return L"format error"; }
    const auto size = static_cast<size_t>(size_s);
    std::wstring string(size, {});
    std::swprintf(string.data(), size, format, value);
    string.pop_back(); // - '\0'
    return string;
}
} // namespace

BEGIN_MESSAGE_MAP(CRangeSlider, CWnd)
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_KEYDOWN()
    ON_WM_GETDLGCODE()
    ON_WM_TIMER()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

void CRangeSlider::OnPaint()
{
    if (m_range.second - m_range.first == 0.)
    {
        ASSERT(FALSE);
        return;
    }

    ASSERT(m_range.first <= m_thumbsPosition.first);
    ASSERT(m_thumbsPosition.first <= m_thumbsPosition.second);
    ASSERT(m_thumbsPosition.second <= m_range.second);

    CPaintDC dc(this);

    if (GetStyle() & TBS_VERT)
        OnPaintVertical(dc);
    else
        OnPaintHorizontal(dc);
}

void CRangeSlider::OnPaintHorizontal(CDC& dc)
{
    ASSERT(!(GetStyle() & TBS_VERT));

    CRect clientRect;
    GetClientRect(&clientRect);

    dc.FillSolidRect(clientRect, GetSysColor(COLOR_3DFACE));

    const auto height = clientRect.Height();
    const double width  = clientRect.Width() - 2 * m_thumbWidth;

    const int leftPos = static_cast<int>(round((m_thumbsPosition.first - m_range.first) / (m_range.second - m_range.first) * width));
    const int rightPos = static_cast<int>(round((m_thumbsPosition.second - m_range.first) / (m_range.second - m_range.first) * width)) + m_thumbWidth;

    m_thumbsRects = std::make_pair(CRect(CPoint(leftPos, 0), CSize(m_thumbWidth, height)),
                                   CRect(CPoint(rightPos, 0), CSize(m_thumbWidth, height)));

    // draw common line and selection
    clientRect.top = clientRect.bottom = clientRect.CenterPoint().y;
    clientRect.InflateRect(-(int)m_thumbWidth, m_lineWidth / 2);
    if (m_lineWidth % 2 != 0)
        clientRect.bottom += 1;

    dc.FillSolidRect(clientRect, lineColor);

    if (GetStyle() & TBS_ENABLESELRANGE)
    {
        clientRect.left = m_thumbsRects.first.right;
        clientRect.right = m_thumbsRects.second.left;
        clientRect.InflateRect(1, 2);
        dc.FillSolidRect(clientRect, selectionColor);
    }

    // draw thumbs
    const CBrush thumbBrush(thumbColor);
    const CBrush thumbBrushTracking(darker(thumbColor, 20));

    dc.SelectStockObject(WHITE_PEN);
    dc.SelectObject((m_trackMode && m_trackMode != TrackMode::TRACK_RIGHT) ? thumbBrushTracking : thumbBrush);
    dc.RoundRect(m_thumbsRects.first, CPoint(6, 6));
    dc.SelectObject((m_trackMode && m_trackMode != TrackMode::TRACK_LEFT) ? thumbBrushTracking : thumbBrush);
    dc.RoundRect(m_thumbsRects.second, CPoint(6, 6));
}

void CRangeSlider::OnPaintVertical(CDC& dc)
{
    ASSERT((GetStyle() & TBS_VERT));

    CRect clientRect;
    GetClientRect(&clientRect);

    const auto height = clientRect.Width();
    const double width = clientRect.Height() - 2 * m_thumbWidth;

    const int leftPos = int(round((m_thumbsPosition.first - m_range.first) / (m_range.second - m_range.first) * width));
    const int rightPos = int(round((m_thumbsPosition.second - m_range.first) / (m_range.second - m_range.first) * width) + m_thumbWidth);

    m_thumbsRects = std::make_pair(CRect(CPoint(0, leftPos), CSize(height, m_thumbWidth)),
                                   CRect(CPoint(0, rightPos), CSize(height, m_thumbWidth)));

    // draw common line and selection
    clientRect.left = clientRect.right = clientRect.CenterPoint().x;
    clientRect.InflateRect(m_lineWidth / 2, -(int)m_thumbWidth);
    if (m_lineWidth % 2 != 0)
        clientRect.right += 1;

    dc.FillSolidRect(clientRect, lineColor);

    if (GetStyle() & TBS_ENABLESELRANGE)
    {
        clientRect.top = m_thumbsRects.first.bottom;
        clientRect.bottom = m_thumbsRects.second.top;
        clientRect.InflateRect(2, 1);
        dc.FillSolidRect(clientRect, selectionColor);
    }

    // draw thumbs
    const CBrush thumbBrush(thumbColor);
    const CBrush thumbBrushTracking(darker(thumbColor, 20));

    dc.SelectStockObject(WHITE_PEN);
    dc.SelectObject((m_trackMode && m_trackMode != TrackMode::TRACK_RIGHT) ? thumbBrushTracking : thumbBrush);
    dc.RoundRect(m_thumbsRects.first, CPoint(6, 6));
    dc.SelectObject((m_trackMode && m_trackMode != TrackMode::TRACK_LEFT) ? thumbBrushTracking : thumbBrush);
    dc.RoundRect(m_thumbsRects.second, CPoint(6, 6));
}

void CRangeSlider::OnLButtonDown(UINT nFlags, CPoint point)
{
    TRACE("Down Point %d, %d\n", point.x, point.y);
    SetFocus();
    Invalidate();

    if (!m_trackMode.has_value())
    {
        if (m_thumbsRects.first.PtInRect(point))
        {
            m_trackMode = TrackMode::TRACK_LEFT;
            m_clickOffsetFormThumbCenter = point - m_thumbsRects.first.CenterPoint();

            ShowSliderTooltip(true, true);
        }
        else if (m_thumbsRects.second.PtInRect(point))
        {
            m_trackMode = TrackMode::TRACK_RIGHT;
            m_clickOffsetFormThumbCenter = point - m_thumbsRects.second.CenterPoint();
            ShowSliderTooltip(false, true);
        }
        else
        {
            CRect middleRect;
            if (GetStyle() & TBS_VERT)
                middleRect = CRect(0, m_thumbsRects.first.bottom + 1, m_thumbsRects.first.right, m_thumbsRects.second.top - 1);
            else
                middleRect = CRect(m_thumbsRects.first.right + 1, 0, m_thumbsRects.second.left - 1, m_thumbsRects.second.bottom);

            if (middleRect.PtInRect(point))
            {
                m_trackMode = TrackMode::TRACK_MIDDLE;
                m_clickOffsetFormThumbCenter = point - middleRect.CenterPoint();
            }
            else
            {
                SetTimer(0, 700, nullptr);
                OnTimer(NULL);
            }
        }

        SetCapture();
    }
    CWnd::OnLButtonDown(nFlags, point);
}

void CRangeSlider::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_trackMode.has_value())
    {
        CRect clientRect;
        GetClientRect(&clientRect);

        const int position = GetStyle() & TBS_VERT ? (point.y - m_clickOffsetFormThumbCenter.y) : (point.x - m_clickOffsetFormThumbCenter.x);

        const double controlWidthInPixels = (GetStyle() & TBS_VERT ? clientRect.Height() : clientRect.Width()) - 2 * (int)m_thumbWidth;
        const double countDataInOnePixel = (m_range.second - m_range.first) / std::max<double>(controlWidthInPixels, 1.);

        const auto previousThumbPositions = m_thumbsPosition;

        switch (*m_trackMode)
        {
        case TrackMode::TRACK_LEFT:
            {
                const double newValue = countDataInOnePixel * double(position - (int)m_thumbWidth / 2) + m_range.first;

                m_thumbsPosition.first = std::clamp(ApplyIncrementStep(m_thumbsPosition.first, newValue),
                                                    m_range.first, m_thumbsPosition.second);
                ShowSliderTooltip(true);
            }
            break;
        case TrackMode::TRACK_RIGHT:
            {
                const double newValue = countDataInOnePixel * double(position - (int)m_thumbWidth * 3 / 2) + m_range.first;

                m_thumbsPosition.second = std::clamp(ApplyIncrementStep(m_thumbsPosition.second, newValue),
                                                     m_thumbsPosition.first, m_range.second);
                ShowSliderTooltip(false);
            }
            break;
        case TrackMode::TRACK_MIDDLE:
            {
                const double delta = m_thumbsPosition.second - m_thumbsPosition.first;
                ASSERT(delta >= 0.0);
                const double newValue = countDataInOnePixel * double(position - (int)m_thumbWidth) + m_range.first - delta / 2.0;

                m_thumbsPosition.first = ApplyIncrementStep(m_thumbsPosition.first, newValue);
                m_thumbsPosition.second = m_thumbsPosition.first + delta;
                if (m_thumbsPosition.first <= m_range.first)
                {
                    m_thumbsPosition.first = m_range.first;
                    m_thumbsPosition.second = m_thumbsPosition.first + delta;
                }
                if (m_thumbsPosition.second >= m_range.second)
                {
                    m_thumbsPosition.second = m_range.second;
                    m_thumbsPosition.first = m_thumbsPosition.second - delta;
                }
            }
            break;
        default:
            TRACE("Unknown Track Mode\n");
            ASSERT(FALSE);
            break;
        }

        if (previousThumbPositions != m_thumbsPosition)
        {
            SendChangePositionEvent();
            RedrawWindow();
        }
    }

    CWnd::OnMouseMove(nFlags, point);
}

void CRangeSlider::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_trackMode.has_value())
    {
        m_tooltip.ShowWindow(SW_HIDE);

        m_trackMode.reset();
        Invalidate();
    }
    else
        KillTimer(NULL);

    ::ReleaseCapture();

    CWnd::OnLButtonUp(nFlags, point);
}

void CRangeSlider::SetRange(_In_ std::pair<double, double> range, _In_ bool redraw)
{
    ASSERT(range.first <= range.second);
    if (range.first > range.second)
        std::swap(range.first, range.second);

    m_range = std::move(range);

    NormalizePositions();
    if (redraw)
        Invalidate();
}

std::pair<double, double> CRangeSlider::GetRange() const
{
    return m_range;
}

void CRangeSlider::SetIncrementStep(double step)
{
    m_incrementStep = step;
}

void CRangeSlider::SetPositions(_In_ std::pair<double, double> positions, _In_ bool redraw)
{
    ASSERT(positions.first <= positions.second);
    if (positions.first > positions.second)
        std::swap(positions.first, positions.second);

    m_thumbsPosition = std::move(positions);

    NormalizePositions();
    if (redraw)
        Invalidate();
}

std::pair<double, double> CRangeSlider::GetPositions() const
{
    return m_thumbsPosition;
}

void CRangeSlider::SetThumbWidth(_In_ unsigned width)
{
    m_thumbWidth = width;
    Invalidate();
}

unsigned CRangeSlider::GetThumbWidth() const
{
    return m_thumbWidth;
}

void CRangeSlider::SetLineWidth(_In_ unsigned width)
{
    m_lineWidth = width;
    Invalidate();
}

unsigned CRangeSlider::GetLineWidth() const
{
    return m_lineWidth;
}

void CRangeSlider::SetTooltipTextFormat(const wchar_t* format)
{
    m_tooltipFormat = format;
}

void CRangeSlider::NormalizePositions()
{
    bool send = false;
    if (m_thumbsPosition.first < m_range.first)
    {
        m_thumbsPosition.first = m_range.first;
        if (m_thumbsPosition.second < m_thumbsPosition.first)
            m_thumbsPosition.second = m_thumbsPosition.first;

        send = true;
    }
    if (m_thumbsPosition.second > m_range.second)
    {
        m_thumbsPosition.second = m_range.second;
        if (m_thumbsPosition.first > m_thumbsPosition.second)
            m_thumbsPosition.first = m_thumbsPosition.second;

        send = true;
    }
    if (send)
        SendChangePositionEvent();
}

void CRangeSlider::SendChangePositionEvent() const
{
    ::SendMessage(GetParent()->GetSafeHwnd(), GetStyle() & TBS_VERT ? WM_VSCROLL : WM_HSCROLL, NULL, NULL);
}

void CRangeSlider::ShowSliderTooltip(bool left, bool createTip)
{
    CRect windowRect;
    GetWindowRect(&windowRect);

    const double controlWidthInPixels = (GetStyle() & TBS_VERT ? windowRect.Height() : windowRect.Width()) - 2 * (int)m_thumbWidth;
    const double countDataInOnePixel = (m_range.second - m_range.first) / std::max<double>(controlWidthInPixels, 1.);
    const double& sliderValue = left ? m_thumbsPosition.first : m_thumbsPosition.second;

    const LONG thumbDelta = left ? (LONG)m_thumbWidth * 2 / 3 : (LONG)m_thumbWidth * 4 / 3;

    m_tooltip.SetLabel(get_string(m_tooltipFormat, sliderValue).c_str());

    const CSize windowSize = m_tooltip.GetWindowSize();
    CRect toolWindowRect;
    if (GetStyle() & TBS_VERT)
    {
        toolWindowRect = CRect({ windowRect.right - 1, windowRect.top + thumbDelta + (LONG)(abs(sliderValue - m_range.first) / countDataInOnePixel) }, windowSize);
        toolWindowRect.OffsetRect(0, -windowSize.cy / 2);
    }
    else
    {
        toolWindowRect = CRect({ windowRect.left + thumbDelta + (LONG)(abs(sliderValue - m_range.first) / countDataInOnePixel), windowRect.top - 1 }, windowSize);
        toolWindowRect.OffsetRect(-windowSize.cx / 2, -windowSize.cy);
    }

    m_tooltip.SetWindowPos(NULL, toolWindowRect.left, toolWindowRect.top, toolWindowRect.Width(), toolWindowRect.Height(),
                           SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
}

double CRangeSlider::ApplyIncrementStep(double oldValue, double newValue)
{
    if (!m_incrementStep.has_value())
        return newValue;
    return oldValue + *m_incrementStep * round((newValue - oldValue) / *m_incrementStep);
}

UINT CRangeSlider::OnGetDlgCode()
{
    return DLGC_WANTARROWS;
}

void CRangeSlider::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);

    const bool ctrlPressed = ::GetKeyState(VK_CONTROL) < 0;

    const UINT keyMoveLeft = GetStyle() & TBS_VERT ? VK_UP : VK_LEFT;
    const UINT keyMoveRight = GetStyle() & TBS_VERT ? VK_DOWN : VK_RIGHT;

    if ((nChar == keyMoveLeft || nChar == keyMoveRight) && !ctrlPressed)
    {
        CRect clientRect;
        GetClientRect(&clientRect);

        const double controlWidthInPixels = (GetStyle() & TBS_VERT ? clientRect.Height() : clientRect.Width()) - 2 * m_thumbWidth;
        if (controlWidthInPixels == 0.0)
            return;
        double countDataInOnePixel = (m_range.second - m_range.first) / std::max<double>(controlWidthInPixels, 1.);
        if (m_incrementStep.has_value())
        {
            if (countDataInOnePixel <= *m_incrementStep)
                countDataInOnePixel = *m_incrementStep;
            else
                countDataInOnePixel = ApplyIncrementStep(countDataInOnePixel, *m_incrementStep);
        }

        const bool shiftPressed = ::GetKeyState(VK_SHIFT) < 0;
        if (nChar == keyMoveLeft)
        {
            if (!shiftPressed) // Shift not pressed => move interval
                m_thumbsPosition.first = std::max<double>(m_thumbsPosition.first - countDataInOnePixel, m_range.first);

            m_thumbsPosition.second = std::max<double>(m_thumbsPosition.second - countDataInOnePixel, m_range.first);
        }
        else
        {
            if (!shiftPressed) // Shift not pressed => move interval
                m_thumbsPosition.first = std::min<double>(m_thumbsPosition.first + countDataInOnePixel, m_range.second);

            m_thumbsPosition.second = std::min<double>(m_thumbsPosition.second + countDataInOnePixel, m_range.second);
        }

        SendChangePositionEvent();
        Invalidate();
    }
}

void CRangeSlider::OnTimer(UINT_PTR nIDEvent)
{
    CWnd::OnTimer(nIDEvent);

    if (nIDEvent != NULL)
        return;

    CPoint mousePos;
    if (!GetCursorPos(&mousePos))
        return;
    ScreenToClient(&mousePos);

    CRect clientRect;
    GetClientRect(&clientRect);
    if (!clientRect.PtInRect(mousePos))
        return;

    // Move cursor to mouse on line click
    const int controlWidthInPixels = (GetStyle() & TBS_VERT ? clientRect.Height() : clientRect.Width()) - 2 * m_thumbWidth;
    const double mousePoint = GetStyle() & TBS_VERT ? mousePos.y : mousePos.x;

    double valueAtMousePoint;
    if (mousePoint <= m_thumbWidth)
        valueAtMousePoint = m_range.first;
    else if (mousePoint >= controlWidthInPixels + m_thumbWidth)
        valueAtMousePoint = m_range.second;
    else
    {
        const double countDataInOnePixel = (m_range.second - m_range.first) / std::max<double>(controlWidthInPixels, 1.);
        valueAtMousePoint = m_range.first + double(mousePoint - (int)m_thumbWidth / 2) * countDataInOnePixel;
    }

    double moveCursorDistance = (m_range.second - m_range.first) / 10;
    if (m_incrementStep.has_value() && moveCursorDistance < *m_incrementStep)
        moveCursorDistance = *m_incrementStep;

    const auto oldPositions = m_thumbsPosition;
    if (valueAtMousePoint < m_thumbsPosition.first)
    {
        const auto newValue = ApplyIncrementStep(m_thumbsPosition.first, m_thumbsPosition.first - moveCursorDistance);
        if (newValue <= valueAtMousePoint)
        {
            m_thumbsPosition.first = ApplyIncrementStep(m_thumbsPosition.first, valueAtMousePoint);

            m_clickOffsetFormThumbCenter = { 0, 0 };
            m_trackMode = TrackMode::TRACK_LEFT;
            KillTimer(NULL);
            ShowSliderTooltip(true, true);
        }
        else
            m_thumbsPosition.first = newValue;
    }
    else if (valueAtMousePoint > m_thumbsPosition.second)
    {
        const auto newValue = ApplyIncrementStep(m_thumbsPosition.second, m_thumbsPosition.second + moveCursorDistance);
        if (newValue >= valueAtMousePoint)
        {
            m_thumbsPosition.second = ApplyIncrementStep(m_thumbsPosition.second, valueAtMousePoint);

            m_clickOffsetFormThumbCenter = { 0, 0 };
            m_trackMode = TrackMode::TRACK_RIGHT;
            KillTimer(NULL);
            ShowSliderTooltip(false, true);
        }
        else
            m_thumbsPosition.second = newValue;
    }

    if (oldPositions != m_thumbsPosition)
    {
        SendChangePositionEvent();
        Invalidate();
    }
}

BOOL CRangeSlider::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE; // remove flickering
}

void CRangeSlider::PreSubclassWindow()
{
    CWnd::PreSubclassWindow();

    m_tooltip.Create(GetSafeHwnd(), true);
}
