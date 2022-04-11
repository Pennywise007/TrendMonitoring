#include "ToolWindow.h"

#include <afxtoolbar.h>
#include <Afxglobals.h>

IMPLEMENT_DYNCREATE(CToolWindow, CWnd)

BEGIN_MESSAGE_MAP(CToolWindow, CWnd)
    ON_WM_PAINT()
    ON_WM_CREATE()
END_MESSAGE_MAP()

#define CLASS_NAME L"CToolWindow"

CToolWindow::CToolWindow(CMFCToolTipInfo* pParams/* = NULL*/)
    : m_drawProxy(pParams)
{
    WNDCLASS wndcls;
    HINSTANCE hInst = AfxGetInstanceHandle();

    if (!(::GetClassInfo(hInst, CLASS_NAME, &wndcls)))
    {
        // otherwise we need to register a new class
        wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wndcls.lpfnWndProc = ::DefWindowProc;
        wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
        wndcls.hInstance = hInst;
        wndcls.hIcon = NULL;
        wndcls.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
        wndcls.hbrBackground = NULL; // No Background brush (gives flicker)
        wndcls.lpszMenuName = NULL;
        wndcls.lpszClassName = CLASS_NAME;

        if (!AfxRegisterClass(&wndcls))
        {
            AfxThrowResourceException();
        }
    }
}

CToolWindow::~CToolWindow()
{
    if (::IsWindow(m_hWnd))
    {
        ::DestroyWindow(m_hWnd);
        ASSERT(::IsWindow(m_drawProxy.m_hWnd));
        m_drawProxy.DestroyWindow();
    }
}

BOOL CToolWindow::Create(HWND parent, bool topMost)
{
    ASSERT(!::IsWindow(m_hWnd));

    const auto res = CWnd::CreateEx(topMost ? WS_EX_TOPMOST : 0, CLASS_NAME, CLASS_NAME,
                          WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                          parent, NULL);
    m_drawProxy.Create(this);
    return res;
}

void CToolWindow::SetLabel(const CString& label)
{
    if (m_label == label)
        return;

    m_label = label;
    RedrawWindow();
}

void CToolWindow::SetParams(CMFCToolTipInfo* pParams, bool redraw /*= true*/)
{
    m_drawProxy.SetParams(pParams);
    if (redraw)
        RedrawWindow();
}

_NODISCARD const CMFCToolTipInfo& CToolWindow::GetParams() const
{
    return m_drawProxy.GetParams();
}
void CToolWindow::SetHotRibbonButton(CMFCRibbonButton* pRibbonButton, bool redraw /*= true*/)
{
    m_drawProxy.SetHotRibbonButton(pRibbonButton);
    if (redraw)
        RedrawWindow();
}

void CToolWindow::SetDescription(const CString& strDescription, bool redraw /*= true*/)
{
    m_drawProxy.SetDescription(strDescription);
    if (redraw)
        RedrawWindow();
}

_NODISCARD CSize CToolWindow::GetIconSize()
{
    return m_drawProxy.GetIconSize();
}

// get window size
_NODISCARD CSize CToolWindow::GetWindowSize()
{
    m_drawProxy.GetHotButton();

    const auto& params = m_drawProxy.GetParams();
    m_drawProxy.m_imageSize = params.m_bDrawIcon ? GetIconSize() : CSize(0, 0);

    CRect rectMargin;
    m_drawProxy.GetMargin(rectMargin);

    CRect rectText;
    GetClientRect(rectText);

    CClientDC dc(this);
    CSize sizeWindow = OnDrawLabel(&dc, rectText, TRUE);

    CSize sizeDescr(0, 0);

    if (!params.m_bDrawDescription || m_drawProxy.m_description.IsEmpty())
    {
        sizeWindow.cy = max(sizeWindow.cy, m_drawProxy.m_imageSize.cy);
    }
    else
    {
        sizeDescr = m_drawProxy.OnDrawDescription(&dc, rectText, TRUE);

        sizeWindow.cy += sizeDescr.cy + 2 * m_ptMargin.y;
        sizeWindow.cx = max(sizeWindow.cx, sizeDescr.cx);
        sizeWindow.cy = max(sizeWindow.cy, m_drawProxy.m_imageSize.cy);
    }

    if (m_drawProxy.m_imageSize.cx > 0 && params.m_bDrawIcon)
    {
        sizeWindow.cx += m_drawProxy.m_imageSize.cx + m_ptMargin.x;
    }

    sizeWindow.cx += 2 * m_ptMargin.x;
    sizeWindow.cy += 2 * m_ptMargin.y;

    const int nFixedWidth = m_drawProxy.GetFixedWidth();
    if (nFixedWidth > 0 && sizeDescr != CSize(0, 0))
    {
        sizeWindow.cx = max(sizeWindow.cx, nFixedWidth);
    }
    return sizeWindow;
}

void CToolWindow::OnPaint()
{
    CPaintDC dcPaint(this);

    CMemDC memDC(dcPaint, this);
    CDC* pDC = &memDC.GetDC();

    CRect rect;
    GetClientRect(rect);

    pDC->FillSolidRect(rect, RGB(255, 0, 0));

    CRect rectMargin;
    m_drawProxy.GetMargin(rectMargin);

    CRect rectText = rect;

    rectText.DeflateRect(rectMargin);
    rectText.DeflateRect(m_ptMargin.x, m_ptMargin.y);

    const auto& params = m_drawProxy.GetParams();

    COLORREF clrLine = params.m_clrBorder == (COLORREF)-1 ? ::GetSysColor(COLOR_INFOTEXT) : params.m_clrBorder;
    COLORREF clrText = params.m_clrText == (COLORREF)-1 ? ::GetSysColor(COLOR_INFOTEXT) : params.m_clrText;

    // Fill background:
    m_drawProxy.OnFillBackground(pDC, rect, clrText, clrLine);

    CPen penLine(PS_SOLID, 1, clrLine);
    CPen* pOldPen = pDC->SelectObject(&penLine);

    // Draw border:
    m_drawProxy.OnDrawBorder(pDC, rect, clrLine);

    // Draw icon:
    if (m_drawProxy.m_imageSize != CSize(0, 0) && params.m_bDrawIcon)
    {
        CRect rectImage = rectText;
        rectImage.right = rectImage.left + m_drawProxy.m_imageSize.cx;
        rectImage.bottom = rectImage.top + m_drawProxy.m_imageSize.cy;

        m_drawProxy.OnDrawIcon(pDC, rectImage);

        rectText.left += m_drawProxy.m_imageSize.cx + m_ptMargin.x;
    }

    pDC->SetBkMode(TRANSPARENT);
    pDC->SetTextColor(clrText);

    // Draw label:
    int nTextHeight = OnDrawLabel(pDC, rectText, FALSE).cy;

    // Draw separator + description:
    if (!m_drawProxy.m_description.IsEmpty() && params.m_bDrawDescription)
    {
        CRect rectDescr = rectText;
        rectDescr.top += nTextHeight + 3 * m_ptMargin.y / 2;

        if (params.m_bDrawSeparator)
        {
            m_drawProxy.OnDrawSeparator(pDC, rectDescr.left, rectDescr.right, rectDescr.top - m_ptMargin.y / 2);
        }

        m_drawProxy.OnDrawDescription(pDC, rectDescr, FALSE);
    }

    pDC->SelectObject(pOldPen);
}

CSize CToolWindow::OnDrawLabel(CDC* pDC, CRect rect, BOOL bCalcOnly)
{
    ASSERT_VALID(pDC);

    CSize sizeText(0, 0);

    CString strText = m_label;
    strText.Replace(_T("\t"), _T("    "));

    const auto& params = m_drawProxy.GetParams();
    BOOL bDrawDescr = params.m_bDrawDescription && !m_drawProxy.m_description.IsEmpty();

    CFont* pOldFont = (CFont*)pDC->SelectObject(params.m_bBoldLabel && bDrawDescr ? &(GetGlobalData()->fontBold) : &(GetGlobalData()->fontTooltip));

    if (strText.Find(_T('\n')) >= 0) // Multi-line text
    {
        UINT nFormat = DT_NOPREFIX;
        if (bCalcOnly)
        {
            nFormat |= DT_CALCRECT;
        }

        if (m_drawProxy.RibbonButtonExists())
        {
            nFormat |= DT_NOPREFIX;
        }

        int nHeight = pDC->DrawText(strText, rect, nFormat);
        sizeText = CSize(rect.Width(), nHeight);
    }
    else
    {
        if (bCalcOnly)
        {
            sizeText = pDC->GetTextExtent(strText);
        }
        else
        {
            UINT nFormat = DT_LEFT | DT_NOCLIP | DT_SINGLELINE;

            if (!bDrawDescr)
            {
                nFormat |= DT_VCENTER;
            }

            if (m_drawProxy.RibbonButtonExists())
            {
                nFormat |= DT_NOPREFIX;
            }

            sizeText.cy = pDC->DrawText(strText, rect, nFormat);
            sizeText.cx = rect.Width();
        }
    }

    pDC->SelectObject(pOldFont);

    return sizeText;
}

int CToolWindow::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;
    ModifyStyle(WS_BORDER, 0);
    return 0;
}

#undef CLASS_NAME