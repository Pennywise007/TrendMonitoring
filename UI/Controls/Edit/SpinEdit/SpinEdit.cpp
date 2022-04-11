#include "stdafx.h"

#include <atltypes.h>
#include <cfloat>

#include "SpinEdit.h"

#define NOT_SPECIFIED			-1	// флаг того что не определен идентификатор
#define DEFAULT_SPIN_WIDTH		17	// ширина спина по умолчанию

BEGIN_MESSAGE_MAP(CMySpinCtrl, CSpinButtonCtrl)
    ON_NOTIFY_REFLECT_EX(UDN_DELTAPOS, &CMySpinCtrl::OnDeltapos)
END_MESSAGE_MAP()

CMySpinCtrl::CMySpinCtrl()
    : m_SpinRange			(std::make_pair(-FLT_MAX, FLT_MAX))
    , m_bNeedCustomDeltaPos	(false)
{
}

template<class T>
long GetOrder(_In_ const T &Value)
{
    long Order(0);

    if (Value != 0)
        Order = (long)floor(log10((T)abs(Value)));	// порядок текущего числа

    return Order;
}

template<class T>
// функция позволяет увеличить или уменьшите значение какого-либо числа
T SetNewValBySpinCtrl(_In_ bool bPositiveIncrement, _In_ T CurrentVal)
{
    long Order(0);	// степень числа
    Order = GetOrder(CurrentVal);

    // если число будет уменьшаться
    if ((!bPositiveIncrement && (CurrentVal > 0)) || (bPositiveIncrement && (CurrentVal < 0)))
    {
        T NextVal(CurrentVal);	// получаемое значение на выходе
        if (bPositiveIncrement)
            NextVal += (T)pow((T)10, (T)Order);
        else
            NextVal -= (T)pow((T)10, (T)Order);

        // проверяем не изменился ли порядок нового числа
        if (Order != GetOrder(NextVal))
            Order--;	// если изменился уменьшаем порядок
    }
    // число которое будет добавлено к текущему
    T Increment = (T)pow((T)10, (T)Order);

    if (!bPositiveIncrement)
        Increment = -Increment;

    return CurrentVal + Increment;
}

CString CalculateFormat(_In_ const float &Val)
{
    CString Format;
    // вычисляем сколько знаков после запятой показывать
    float FractionalPart = Val - (long)Val;

    if (FractionalPart != 0)
    {
        // если есть дробная частьвычисляем сколько там знаков
        long NewOrder = GetOrder(FractionalPart);
        Format.Format(L"%%.%df", -NewOrder);
    }
    else
        Format = L"%.0f";

    return Format;
}

BOOL CMySpinCtrl::OnDeltapos(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

    // если пользователь будет сам обрабатывать сообщения от контрола
    if (m_bNeedCustomDeltaPos)
        return FALSE;

    CString EditText;
    this->GetBuddy()->GetWindowTextW(EditText);

    float OldVal = (float)_wtof(EditText);
    float NewVal = SetNewValBySpinCtrl(pNMUpDown->iDelta > 0, OldVal);

    CString FormatStr;

    // проверяем не вышли ли мы за допустимые границы контрола
    if (NewVal < m_SpinRange.first)
    {
        NewVal = m_SpinRange.first;

        if (OldVal == NewVal)
        {
            *pResult = 1;
            return TRUE;
        }

        FormatStr = CalculateFormat(NewVal);
    }
    else if (NewVal > m_SpinRange.second)
    {
        NewVal = m_SpinRange.second;

        if (OldVal == NewVal)
        {
            *pResult = 1;
            return TRUE;
        }

        FormatStr = CalculateFormat(NewVal);
    }
    else
    {
        int bIndex_e = EditText.Find(L"e");
        int bIndex_E = EditText.Find(L"E");
        int bIndex_Point = EditText.Find(L".");

        if (bIndex_e == -1 && bIndex_E == -1)
        {
            size_t Length = EditText.GetLength();

            int CountBeforePoint(-1), CountAfterPoint(-1);


            // если нету точки
            if (bIndex_Point == -1)
            {
                // если нету точки то отображаем простое число
                CountAfterPoint = 0;
            }
            else
            {
                // если есть точка то отображаем простое число
                CountBeforePoint = bIndex_Point - 1;
                CountAfterPoint = Length - bIndex_Point - 1;
            }
            // вычисляем что изменилось в числе
            // смотрим изменилась ли степень числа
            long OldOrder = GetOrder(OldVal);
            long NewOrder = GetOrder(NewVal);

            if (OldOrder != NewOrder)
            {
                // проверяем различаются ли знаки у этих чисел
                if ((OldOrder * NewOrder) < 0)
                {
                    // если изменился знак числа
                    if (NewOrder < 0)
                    {
                        // если число стало дробным
                        CountBeforePoint = -1;
                        CountAfterPoint = NewOrder;
                    }
                    else
                    {
                        // если число стало целым
                        CountBeforePoint = NewOrder;
                        CountAfterPoint = 0;
                    }
                }
                else
                {
                    if (NewOrder < 0)
                        CountAfterPoint = CountAfterPoint + (OldOrder - NewOrder);		// если они оба дробные
                    else
                        CountBeforePoint = CountBeforePoint + (NewOrder - OldOrder);	// если они оба целые
                }
            }

            CString BeforePoint(_T("")), AfterPoint(_T("")), PointStr(_T(""));
            if (CountBeforePoint == -1)
                BeforePoint.Empty();
            else
                BeforePoint.Format(L"%d", CountBeforePoint);

            if (CountAfterPoint == -1)
                AfterPoint.Empty();
            else
            {
                PointStr = L".";
                AfterPoint.Format(L"%d", CountAfterPoint);
            }

            FormatStr.Format(L"%%%s%s%sf",
                             BeforePoint.GetString(),
                             PointStr.GetString(),
                             AfterPoint.GetString());
        }
        else
        {
            if (bIndex_e != -1)
                FormatStr = L"%g";
            else
                FormatStr = L"%G";
        }
    }

    EditText.Format(FormatStr, NewVal);	
    this->GetBuddy()->SetWindowTextW(EditText);
    *pResult = 1;

    return TRUE;
}

void CMySpinCtrl::SetRange32(_In_ int Left, _In_ int Right)
{
    SetRange64((float)Left, (float)Right);
}

void CMySpinCtrl::SetRange64(_In_ float Left, _In_ float Right)
{
    m_SpinRange = std::make_pair(Left, Right);

    if (Left < UD_MINVAL)
        Left = UD_MINVAL;
    if (Right > UD_MAXVAL)
        Right = UD_MAXVAL;

    CSpinButtonCtrl::SetRange32((int)Left, (int)Right);
}

void CMySpinCtrl::GetRange64(_Out_ float &Left, _Out_ float &Right)
{
    Left  = m_SpinRange.first;
    Right = m_SpinRange.second;
}

void CMySpinCtrl::SetUseCustomDeltaPos(_In_opt_ bool bUseCustom /*= true*/)
{
    m_bNeedCustomDeltaPos = bUseCustom;
}

bool CMySpinCtrl::GetUseCustomDeltaPos()
{
    return m_bNeedCustomDeltaPos;
}

BEGIN_MESSAGE_MAP(CSpinEdit, CEditBase)
    ON_WM_WINDOWPOSCHANGED()
    ON_WM_CREATE()
    ON_WM_SHOWWINDOW()
    ON_WM_WINDOWPOSCHANGING()
END_MESSAGE_MAP()

CSpinEdit::CSpinEdit()
    : CEditBase			(true)
    , m_SpinAlign		(SpinEdit::RIGHT)
    , m_SpinWidth		(DEFAULT_SPIN_WIDTH)
    , m_bFromCreate		(false)
    , m_SpinCtrlID		(NOT_SPECIFIED)
    , m_SpinCtrl        (std::make_unique<CMySpinCtrl>())
{
    m_SpinRange = std::make_pair(-FLT_MAX, FLT_MAX);
}

CSpinEdit::CSpinEdit(_In_ UINT SpinID)
    : CSpinEdit()
{
    m_SpinCtrlID = SpinID;
}

CSpinEdit::~CSpinEdit()
{
    if (IsWindow(*m_SpinCtrl))
    {
        m_SpinCtrl->DestroyWindow();
    }
}

void CSpinEdit::InitEdit()
{
    if (IsWindow(*m_SpinCtrl))
        m_SpinCtrl->DestroyWindow();

    DWORD Style = CEditBase::GetStyle() & WS_VISIBLE ? WS_VISIBLE : 0;
    Style |= WS_CHILD | UDS_SETBUDDYINT | UDS_HOTTRACK | UDS_NOTHOUSANDS;
    // создаем спинконтрол
    m_SpinCtrl->Create(Style, CRect(), CEditBase::GetParent(),
                       m_SpinCtrlID == NOT_SPECIFIED ? CEditBase::GetDlgCtrlID() : m_SpinCtrlID);
    m_SpinCtrl->SetBuddy(this);
    m_SpinCtrl->SetRange64(m_SpinRange.first, m_SpinRange.second);

    // сдвигаем основное окно чтобы размер окна со спином остался прежним
    CRect Rect;
    CEditBase::GetWindowRect(Rect);
    GetParent()->ScreenToClient(Rect);
    if (m_SpinAlign == SpinEdit::RIGHT)
        Rect.right	-= m_SpinWidth;
    else
        Rect.left	+= m_SpinWidth;
    CEditBase::MoveWindow(Rect);

    ReposCtrl(Rect);
}

void CSpinEdit::PreSubclassWindow()
{
    if (!m_bFromCreate)
        InitEdit();
    m_bFromCreate = false;
    CEditBase::PreSubclassWindow();
}

void CSpinEdit::SetSpinAlign(_In_opt_ SpinEdit::SpinAligns Align /*= SpinEdit::RIGHT*/)
{
    if (m_SpinAlign != Align)
    {
        CRect EditRect;
        GetWindowRect(EditRect);
        CEditBase::GetParent()->ScreenToClient(EditRect);

        m_SpinAlign = Align;
        CEditBase::MoveWindow(EditRect);
    }
}

void CSpinEdit::SetRange32(_In_ int Left, _In_ int Right)
{
    if (IsWindow(*m_SpinCtrl))
        m_SpinCtrl->SetRange32(CEditBase::m_bUsePositivesDigitsOnly ? max(0, Left) : Left, Right);
    m_SpinRange = std::make_pair((float)Left, (float)Right);

    CEditBase::SetUseCtrlLimits(true);
    CEditBase::SetMinMaxLimits(CEditBase::m_bUsePositivesDigitsOnly ? (float)max(0, Left) : (float)Left, (float)Right);
}

void CSpinEdit::SetRange64(_In_ float Left, _In_ float Right)
{
    if (IsWindow(*m_SpinCtrl))
        m_SpinCtrl->SetRange64(CEditBase::m_bUsePositivesDigitsOnly ? max(0, Left) : Left, Right);
    m_SpinRange = std::make_pair(Left, Right);

    CEditBase::SetUseCtrlLimits(true);
    CEditBase::SetMinMaxLimits(CEditBase::m_bUsePositivesDigitsOnly ? max(0, Left) : Left, Right);
}

void CSpinEdit::GetRange(_Out_ float &Left, _Out_ float &Right)
{
    Left = m_SpinRange.first;
    Right = m_SpinRange.second;
}

void CSpinEdit::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
    lpwndpos->cx -= m_SpinWidth;

    if (m_SpinAlign == SpinEdit::LEFT)
        lpwndpos->x += m_SpinWidth;

    CEditBase::OnWindowPosChanging(lpwndpos);
}

void CSpinEdit::OnWindowPosChanged(WINDOWPOS* lpwndpos)
{
    CEditBase::OnWindowPosChanged(lpwndpos);

    // масштабируем контрол и спин
    ReposCtrl(CRect(lpwndpos->x, lpwndpos->y, lpwndpos->x + lpwndpos->cx, lpwndpos->y + lpwndpos->cy));
}

void CSpinEdit::SetSpinWidth(_In_ int NewWidth)
{
    CRect Rect;
    GetWindowRect(Rect);
    CEditBase::GetParent()->ScreenToClient(Rect);

    m_SpinWidth = NewWidth;
    CEditBase::MoveWindow(Rect);
}

void CSpinEdit::ReposCtrl(_In_opt_ const CRect& EditRect)
{
    CRect SpinRect = EditRect;

    if (m_SpinAlign == SpinEdit::LEFT)
    {
        SpinRect.right = SpinRect.left;
        SpinRect.left -= m_SpinWidth;
    }
    else
    {
        SpinRect.left = SpinRect.right;
        SpinRect.right += m_SpinWidth;
    }
    m_SpinCtrl->MoveWindow(SpinRect);
    m_SpinCtrl->RedrawWindow();
}

BOOL CSpinEdit::PreCreateWindow(CREATESTRUCT& cs)
{
    m_bFromCreate = true;
    return CEditBase::PreCreateWindow(cs);
}

int CSpinEdit::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CEditBase::OnCreate(lpCreateStruct) == -1)
        return -1;

    InitEdit();
    return 0;
}

void CSpinEdit::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CEditBase::OnShowWindow(bShow, nStatus);
    m_SpinCtrl->ShowWindow(bShow);
}

void CSpinEdit::GetWindowRect(LPRECT lpRect) const
{
    CEditBase::GetWindowRect(lpRect);
    if (m_SpinAlign == SpinEdit::LEFT)
        lpRect->left -= m_SpinWidth;
    else
        lpRect->right += m_SpinWidth;
}

void CSpinEdit::GetClientRect(LPRECT lpRect) const
{
    CEditBase::GetClientRect(lpRect);
    lpRect->right += m_SpinWidth;
}

void CSpinEdit::UsePositiveDigitsOnly(_In_opt_ bool bUsePositiveDigitsOnly /*= true*/)
{
    if (bUsePositiveDigitsOnly)
        m_SpinCtrl->SetRange64(0, m_SpinRange.second);
    else
        m_SpinCtrl->SetRange64(m_SpinRange.first, m_SpinRange.second);

    CEditBase::UsePositiveDigitsOnly(bUsePositiveDigitsOnly);
}
