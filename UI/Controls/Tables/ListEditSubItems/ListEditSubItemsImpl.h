#pragma once

#include <cassert>

#include "ListEditSubItems.h"

////////////////////////////////////////////////////////////////////////////////
// Абстрактный класс для окна редактирования текста
class IEditSubItemWindow : public CDialogEx
{
public:
    // фабричный метод созданий окна для редактирования текста
    static std::unique_ptr<IEditSubItemWindow> createWindow(
        const CRect& rect, const LVSubItemParams::Ptr& subitemParams);

public:
    // устанавливаем внутренний контрол использующийся для редактирования
    virtual void setInternalControl(std::shared_ptr<CWnd> pControlWnd) = 0;
    // получаем внутренний контрол использующийся для редактирования
    virtual CWnd* getInternalControl() = 0;
    // получаем информацию об редактируемой ячейке
    virtual LVSubItemParams* getSubItemParams() = 0;
};

////////////////////////////////////////////////////////////////////////////////
// сообщение об окончании редактирования ячейки
// lParam - результат закрытия окна редакирования ячейки IDOK/IDCANCEL
#define WM_END_EDIT_SUB_ITEM WM_USER + 200

////////////////////////////////////////////////////////////////////////////////
// класс с возможностью редактировать ячейки списка
BEGIN_TEMPLATE_MESSAGE_MAP(CListEditSubItems, CBaseList, CBaseList)
    ON_WM_LBUTTONDBLCLK()
    ON_MESSAGE(WM_END_EDIT_SUB_ITEM, &CListEditSubItems::OnEndEditSubItem)
END_MESSAGE_MAP()

//----------------------------------------------------------------------------//
template <typename CBaseList>
void CListEditSubItems<CBaseList>::
setSubItemEditorController(int iSubItem, std::shared_ptr<ISubItemEditorController> controller)
{
    if (controller)
        m_subItemsEditorControllers[iSubItem] = controller;
    else
    {
        auto existedIt = m_subItemsEditorControllers.find(iSubItem);
        if (existedIt != m_subItemsEditorControllers.end())
            m_subItemsEditorControllers.erase(existedIt);
    }
}

//----------------------------------------------------------------------------//
template<typename CBaseList>
inline std::shared_ptr<ISubItemEditorController>
CListEditSubItems<CBaseList>::getSubItemEditorController(int iSubItem)
{
    std::shared_ptr<ISubItemEditorController> editorController;
    // проверяем задан ли контроллер для этой колонки
    auto editorControllerIt = m_subItemsEditorControllers.find(iSubItem);
    if (editorControllerIt != m_subItemsEditorControllers.end())
    {
        // если сказали что колонка не редактируется - выходим
        if (!editorControllerIt->second)
            return nullptr;

        editorController = editorControllerIt->second;
    }
    else
        // используем фабрику по умолчанию
        editorController = std::make_shared<SubItemEditorControllerBase>();

    assert(editorController.get()); // не создалась?

    return editorController;
}

//----------------------------------------------------------------------------//
template<typename CBaseList>
inline void CListEditSubItems<CBaseList>::setSubItemEditorController(int iSubItem,
    const createEditorControlFunc& createFunc, const onEndEditSubItemFunc& endEditFunc, const onShowControlCallback& onShowControl)
{
    class SubItemEditorControllerCustom : public SubItemEditorControllerBase
    {
    public:
        SubItemEditorControllerCustom(const createEditorControlFunc& createFunc, const onEndEditSubItemFunc& endEditFunc, const onShowControlCallback& onShowControl)
            : m_createFunc(createFunc), m_endEditFunc(endEditFunc), m_onShowControl(onShowControl)
        {}

    // ISubItemEditorController
    public:
        // создать контрол для редактирования, создает поле ввода для редактирования текста
        std::shared_ptr<CWnd> createEditorControl(CListCtrl* pList,
                                                  CWnd* parentWindow,
                                                  const LVSubItemParams* pParams) override
        {
            if (m_createFunc)
                return m_createFunc(pList, parentWindow, pParams);
            else
                return SubItemEditorControllerBase::createEditorControl(pList, parentWindow, pParams);
        }

        // Called after insertion and showing control inside list
        void onShowControl(CWnd* editorControl) override
        {
            if (m_onShowControl)
                m_onShowControl(editorControl);
        }

        // вызывается при окончании редактирования ячейки
        void onEndEditSubItem(CListCtrl* pList,
                              CWnd* editorControl,
                              const LVSubItemParams* pParams,
                              bool bAcceptResult) override
        {
            if (m_endEditFunc)
                m_endEditFunc(pList, editorControl, pParams, bAcceptResult);
            else
                SubItemEditorControllerBase::onEndEditSubItem(pList, editorControl, pParams, bAcceptResult);
        }

    public:
        const createEditorControlFunc m_createFunc;
        const onEndEditSubItemFunc m_endEditFunc;
        const onShowControlCallback m_onShowControl;
    };

    setSubItemEditorController(iSubItem, std::make_shared<SubItemEditorControllerCustom>(createFunc, endEditFunc, onShowControl));
}

//----------------------------------------------------------------------------//
template <typename CBaseList>
void CListEditSubItems<CBaseList>::PreSubclassWindow()
{
    CBaseList::PreSubclassWindow();

    // убираем свойство редактирования ячеек потому что он работает только для первой колонки
    CBaseList::ModifyStyle(LVS_EDITLABELS, 0);

    DWORD extendedListStyle = CBaseList::GetExtendedStyle();
    // Focus retangle is not painted properly without double-buffering
#if (_WIN32_WINNT >= 0x501)
    extendedListStyle |= LVS_EX_DOUBLEBUFFER;
#endif
    extendedListStyle |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP;
    CBaseList::SetExtendedStyle(extendedListStyle);
}

//----------------------------------------------------------------------------//
template <typename CBaseList>
afx_msg void CListEditSubItems<CBaseList>::OnLButtonDblClk(UINT nFlags,
                                                           CPoint point)
{
    CBaseList::OnLButtonDblClk(nFlags, point);
    // CMT: Select the item the user clicked on.
    UINT uFlags;
    int nItem = CBaseList::HitTest(point, &uFlags);

    // CMT: If the click has been made on some valid item
    if (-1 != nItem)
    {
        if (uFlags & LVHT_ONITEMLABEL)
        {
            LVHITTESTINFO hti;
            hti.pt = point;
            hti.flags = LVM_SUBITEMHITTEST;

            CBaseList::SubItemHitTest(&hti);

            LVITEMINDEX htItemIndex;
            htItemIndex.iItem = hti.iItem;
            htItemIndex.iGroup = hti.iGroup;

            // получаем размер ячейки
            CRect htItemRect;
            if (!CBaseList::GetItemIndexRect(&htItemIndex, hti.iSubItem,
                                             LVIR_LABEL, htItemRect))
            {
                assert(false);  // не смогли получить область куда был клик
                return;
            }
            CBaseList::ClientToScreen(htItemRect);

            if (m_editSubItemWindow)
            {
                assert(false);   // предыдущее должно быть уже уничтожено
                m_editSubItemWindow.reset();
            }

            // получаем фабрику для ячейки
            std::shared_ptr<ISubItemEditorController> editorController =
                getSubItemEditorController(hti.iSubItem);
            if (!editorController)
                return;

            const LVSubItemParams::Ptr params =
                std::make_shared<LVSubItemParams>(hti.iItem, hti.iSubItem, hti.iGroup);

            // создаем окно редактирования сообщения
            m_editSubItemWindow = IEditSubItemWindow::createWindow(htItemRect, params);
            if (!::IsWindow(m_editSubItemWindow->m_hWnd))
            {
                assert(false);
                m_editSubItemWindow.reset();
                return;
            }

            // создаем контрол
            std::shared_ptr<CWnd> editorControl =
                editorController->createEditorControl(this,
                                                      m_editSubItemWindow.get(),
                                                      params.get());
            if (!editorControl || !::IsWindow(editorControl->m_hWnd))
            {
                m_editSubItemWindow.reset();
                return;
            }

            // проставляем владельца чтобы сообщения шли нам
            m_editSubItemWindow->SetOwner(this);

            // проставляем шрифт от таблицы
            editorControl->SetFont(CBaseList::GetFont());

            // ставим контрол на место
            CRect editControlRect;
            m_editSubItemWindow->GetClientRect(editControlRect);
            editorControl->MoveWindow(editControlRect);

            // выделяем весь текст
            ::PostMessage(editorControl->m_hWnd, EM_SETSEL, 0, editorControl->GetWindowTextLengthW());

            // сообщаем нашему окну редактирования о вставленном в него контроле
            m_editSubItemWindow->setInternalControl(editorControl);

            editorController->onShowControl(editorControl.get());
        }
    }
}

//----------------------------------------------------------------------------//
template <typename CBaseList>
afx_msg LRESULT CListEditSubItems<CBaseList>::OnEndEditSubItem(WPARAM wParam,
                                                               LPARAM /*lParam*/)
{
    // во время закрытия окна после OnOk или OnCancel может придти оповещение ещё раз из OnActivate
    // по-этому берем наше окно на промежуточный смарт поинтер и выходим если окна уже нет
    decltype(m_editSubItemWindow) editWindow;
    editWindow.swap(m_editSubItemWindow);
    if (!editWindow)
        return -1;

    bool bAcceptResult = false;
    switch (wParam)
    {
        case IDOK:
            bAcceptResult = true;
            break;
        case IDCANCEL:
            break;
        default:
            assert(false);  // неизвестный флаг
            break;
    }

    CWnd* internalControl = editWindow->getInternalControl();
    LVSubItemParams* subItemParams = editWindow->getSubItemParams();
    if (internalControl && subItemParams)
    {
        // сообщаем фабрике об окончании редактирования
        getSubItemEditorController(subItemParams->iSubItem)->onEndEditSubItem(
            this, internalControl, subItemParams, bAcceptResult);
    }
    else
        assert(false);

    editWindow->DestroyWindow();

    return 0;
}