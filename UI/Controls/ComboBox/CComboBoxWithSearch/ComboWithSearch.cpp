#include <assert.h>
#include <regex>
#include <algorithm>

#include "ComboWithSearch.h"

////////////////////////////////////////////////////////////////////////////////
// константы
// идентификатор таймера подбора
const UINT_PTR kTextChangeTimerId = 0;

// время через которое должен сработать таймер подбора, мс
const UINT kSearchTime = 200;

BEGIN_MESSAGE_MAP(ComboWithSearch, CComboBox)
    ON_CONTROL_REFLECT_EX(CBN_KILLFOCUS,       &ComboWithSearch::OnCbnKillfocus)
    ON_CONTROL_REFLECT_EX(CBN_SELENDOK,        &ComboWithSearch::OnCbnSelendok)
    ON_CONTROL_REFLECT_EX(CBN_SELENDCANCEL,    &ComboWithSearch::OnCbnSelendcancel)
    ON_CONTROL_REFLECT_EX(CBN_EDITCHANGE,      &ComboWithSearch::OnCbnEditchange)
    ON_WM_TIMER()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////
// класс сохраняющий текущий выделенный элемент
class CurSelSaver
{
public:
    CurSelSaver(ISelectionSaver* pSaverClass);
    ~CurSelSaver();

private:
    // указатель на класс с комбобоксом
    ISelectionSaver* m_saverClass;
    // сохраненное выделение
    int m_savedRealSel;
};
//----------------------------------------------------------------------------//
LRESULT ComboWithSearch::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case CB_SETCURSEL:
        {
            auto res = BaseCtrl::WindowProc(message, wParam, lParam);

            updateRealCurSelFromCurrent();

            return res;
        }
    }

    return BaseCtrl::WindowProc(message, wParam, lParam);
}

//----------------------------------------------------------------------------//
BOOL ComboWithSearch::OnCbnEditchange()
{
    BaseCtrl::KillTimer(kTextChangeTimerId);
    BaseCtrl::SetTimer(kTextChangeTimerId, kSearchTime, nullptr);

    return FALSE;
}

//----------------------------------------------------------------------------//
BOOL ComboWithSearch::OnCbnSelendok()
{
    // если нет выбранной строки, но есть строки в списке
    // ставим выделение на первую существующую
    if (BaseCtrl::GetCurSel() == -1 && BaseCtrl::GetCount() > 0)
        BaseCtrl::SetCurSel(1);

    updateRealCurSelFromCurrent();

    // убираем фильтр, возвращаем отфильтрованные строки в контрол
    resetSearch();

    return FALSE;
}

//----------------------------------------------------------------------------//
BOOL ComboWithSearch::OnCbnSelendcancel()
{
    // убираем фильтр, возвращаем отфильтрованные строки в контрол
    resetSearch();

    // восстанавливаем предыдущий выделенный элемент, делаем принудительно
    // потому что в поле может быть текст не соответствующий выделению(пустой например)
    BaseCtrl::SetCurSel(m_realSel);

    return FALSE;
}

//----------------------------------------------------------------------------//
BOOL ComboWithSearch::OnCbnKillfocus()
{
    // убираем фильтр, возвращаем отфильтрованные строки в контрол
    resetSearch();

    return FALSE;
}

//----------------------------------------------------------------------------//
void ComboWithSearch::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    // по эскейпу стираем текст и убираем поиск
    if (nChar == VK_ESCAPE && nFlags == 0)
    {
        // запоминаем теущее состояние ыпадающего списка
        bool dropShow = !!BaseCtrl::GetDroppedState();

        // получаем текущий текст
        CString curWindowText;
        BaseCtrl::GetWindowText(curWindowText);

        // если у нас открыт выпадающий список и в нем есть текст
        if (dropShow && !curWindowText.IsEmpty())
        {
            // убираем фильтр, возвращаем отфильтрованные строки в контрол
            if (resetSearch())
                adjustComboBoxToContent();

            // убираем текст
            BaseCtrl::SetWindowText(CString());

            return;
        }
    }
    BaseCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}

//----------------------------------------------------------------------------//
void ComboWithSearch::executeSearch()
{
    ::SetCursor(LoadCursor(NULL, IDC_WAIT));

    // запоминаем теущее состояние ыпадающего списка
    bool dropShow = !!BaseCtrl::GetDroppedState();

    // получаем текущий текст
    CString curWindowText;
    BaseCtrl::GetWindowText(curWindowText);

    // получаем текущее выделение
    DWORD curSel = BaseCtrl::GetEditSel();

    bool bChangeInStrings = !curWindowText.IsEmpty();
    {
        // сохраняем выделение если возможно
        CurSelSaver selSaver(static_cast<ISelectionSaver*>(this));

        if (!curWindowText.IsEmpty())
        {
            try
            {
                // экранируем спец символы регекса
                const std::wregex re_regexEscape(_T("[.^$|()\\[\\]{}*+?\\\\]"));
                const std::wstring rep(_T("\\\\&"));
                CString regexStr = std::regex_replace(curWindowText.GetString(),
                                                      re_regexEscape,
                                                      rep,
                                                      std::regex_constants::format_sed |
                                                      std::regex_constants::match_default).c_str();
                // формируем строку поиска для регекса
                regexStr.Replace(L" ", L".*?");

                const std::wregex xRegEx(regexStr, std::regex::icase);

                int existElementsIndex = 0;
                auto currentFilteredElementIt = m_filteredItems.begin();
                // проходим по всем строкам и применяем к ним фильтр
                for (int i = 0, count = BaseCtrl::GetCount() + (int)m_filteredItems.size(); i < count; ++i)
                {
                    std::wsmatch xResults;

                    // если текущая строка находится в списке удаленных элементов
                    if (currentFilteredElementIt != m_filteredItems.end() &&
                        i == currentFilteredElementIt->first)
                    {
                        // проверяем надо ли оставить эту строку
                        // если с новым фильтром строка удовлетворяет поиску - восстанавливем её
                        if (std::regex_search(currentFilteredElementIt->second,
                            xResults, xRegEx))
                        {
                            // возвращаем строку в контрол
                            BaseCtrl::InsertString(existElementsIndex,
                                                   currentFilteredElementIt->second.c_str());
                            bChangeInStrings = true;
                            // удаляем ее из списка отфильтрованных
                            currentFilteredElementIt = m_filteredItems.erase(currentFilteredElementIt);

                            existElementsIndex++;
                        }
                        else
                            // переходим к след элементу
                            ++currentFilteredElementIt;
                    }
                    else
                    {
                        // проверяем строку из комбобокса
                        std::wstring str;
                        str.resize(BaseCtrl::GetLBTextLen(existElementsIndex) + 1);
                        BaseCtrl::GetLBText(existElementsIndex,
                                            const_cast<WCHAR*>(str.c_str()));

                        // если она не удовлетворяет фильтру выкидываем её
                        if (!std::regex_search(str, xResults, xRegEx))
                        {
                            // добавляем в список удалённых
                            assert(m_filteredItems.find(i) == m_filteredItems.end());
                            m_filteredItems[i] = str;

                            // убираем отфильтрованную строку
                            BaseCtrl::DeleteString(existElementsIndex);

                            bChangeInStrings = true;
                        }
                        else
                            ++existElementsIndex;
                    }
                }
            }
            catch (const std::regex_error& err)
            {
                ::MessageBox(0, CString(err.what()),
                             L"Возникло исключение!", MB_ICONERROR | MB_OK);
            }
        }
        else
            // убираем фильтр, возвращаем отфильтрованные строки в контрол
            bChangeInStrings |= resetSearch();
    }

    if (!dropShow)
    {
        if (BaseCtrl::GetCount())
            // если был закрыт выпадающий список и он не пустой - раскрываем его
            BaseCtrl::ShowDropDown(true);
    }
    else
    {
        // если были изменения - надо пересчитать высоту выпадающего списка
        if (bChangeInStrings)
            adjustComboBoxToContent();
    }

    // восстанавливаем текст и выделение
    BaseCtrl::SetWindowText(curWindowText);
    BaseCtrl::SetEditSel(LOWORD(curSel), HIWORD(curSel));

    ::SetCursor(LoadCursor(NULL, IDC_ARROW));
}

//----------------------------------------------------------------------------//
void ComboWithSearch::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kTextChangeTimerId)
    {
        KillTimer(kTextChangeTimerId);

        executeSearch();
    }

    BaseCtrl::OnTimer(nIDEvent);
}

//----------------------------------------------------------------------------//
bool ComboWithSearch::resetSearch()
{
    if (!m_filteredItems.empty())
    {
        // сохраняем выделение если возможно
        CurSelSaver selSaver(static_cast<ISelectionSaver*>(this));

        // убираем фильтр, возвращаем отфильтрованные строки в контрол
        for (auto &it : m_filteredItems)
        {
            BaseCtrl::InsertString(it.first, it.second.c_str());
        }

        m_filteredItems.clear();

        return true;
    }
    else
        return false;
}

//----------------------------------------------------------------------------//
void ComboWithSearch::updateRealCurSelFromCurrent()
{
    // получаем текущее значение выделения
    m_realSel = convertToRealIndex(BaseCtrl::GetCurSel());
}

//----------------------------------------------------------------------------//
void ComboWithSearch::adjustComboBoxToContent()
{
    if(!BaseCtrl::GetSafeHwnd() || !::IsWindow(BaseCtrl::GetSafeHwnd()))
    {
        ASSERT(FALSE);
        return;
    }

    // Make sure the drop rect for this combobox is at least tall enough to
    // show 3 items in the dropdown list.
    int nHeight = 0;
    int nItemsToShow = std::min<int>(10, BaseCtrl::GetCount());
    for (int i = 0; i < nItemsToShow; i++)
    {
        int nItemH = BaseCtrl::GetItemHeight(i);
        nHeight += nItemH;
    }

    nHeight += (4 * ::GetSystemMetrics(SM_CYEDGE));

    // Set the height if necessary -- save current size first
    COMBOBOXINFO cmbxInfo;
    cmbxInfo.cbSize = sizeof(COMBOBOXINFO);
    if (BaseCtrl::GetComboBoxInfo(&cmbxInfo))
    {
        CRect rcListBox;
        ::GetWindowRect(cmbxInfo.hwndList, &rcListBox);

        if (rcListBox.Height() != nHeight)
            ::SetWindowPos(cmbxInfo.hwndList, 0, 0, 0, rcListBox.Width(),
                           nHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOSENDCHANGING);
    }
}

//----------------------------------------------------------------------------//
int ComboWithSearch::convertToRealIndex(int index)
{
    if (index != -1 && !m_filteredItems.empty())
    {
        // корректируем индекс с учетом удаленных строк
        // будем увеличивать на каждую удаленную строку до искомого индекса
        for (const auto &it : m_filteredItems)
        {
            if (it.first <= index)
                ++index;
            else
                break;
        }
    }

    return index;
}

//----------------------------------------------------------------------------//
int ComboWithSearch::convertFromRealIndex(int index)
{
    if (index != -1 && !m_filteredItems.empty())
    {
        int currentElementsIndex = index;
        // ищем нде находится индекс с учетом удаленных строк
        // будем уменьшать на каждую удаленную строку до искомого индекса
        for (const auto &it : m_filteredItems)
        {
            if (it.first <= index)
                --currentElementsIndex;
            else
                break;
        }

        return currentElementsIndex;
    }

    return index;
}

//----------------------------------------------------------------------------//
int ComboWithSearch::getRealCurSel()
{
    return m_realSel;
}

//----------------------------------------------------------------------------//
void ComboWithSearch::setRealCurSel(int selection)
{
    BaseCtrl::SetCurSel(convertFromRealIndex(selection));
}

//----------------------------------------------------------------------------//
CurSelSaver::CurSelSaver(ISelectionSaver* pSaverClass)
    : m_saverClass(pSaverClass)
    , m_savedRealSel(m_saverClass->getRealCurSel())
{}

//----------------------------------------------------------------------------//
CurSelSaver::~CurSelSaver()
{
    m_saverClass->setRealCurSel(m_savedRealSel);
}