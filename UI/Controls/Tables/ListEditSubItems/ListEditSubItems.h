#pragma once

/*

Allow add custom edit field insode ListCtrl

Usage:
  CListEditSubItems m_listSearchStrings;
  m_listSearchStrings.setSubItemEditorController(0,
        [](CListCtrl* pList, CWnd* parentWindow, const LVSubItemParams* pParams)
        {
            auto comboBox = std::make_shared<CComboBox>();

            comboBox->Create(SubItemEditorControllerBase::getStandartEditorWndStyle() |
                             CBS_DROPDOWNLIST | CBS_HASSTRINGS | CBS_AUTOHSCROLL | WS_VSCROLL,
                             CRect(), parentWindow, 0);

            for (auto&& [text, type] : g_searchTypesLocalizations)
            {
                comboBox->AddString(text.c_str());
            }

            comboBox->SetCurSel(0);
            CString curSubItemText = pList->GetItemText(pParams->iItem, pParams->iSubItem);
            if (!curSubItemText.IsEmpty())
            {
                const auto it = g_searchTypesLocalizations.find(curSubItemText.GetString());
                if (it != g_searchTypesLocalizations.end())
                    comboBox->SetCurSel((int)std::distance(g_searchTypesLocalizations.begin(), it));
            }

            return std::shared_ptr<CWnd>(comboBox);
        },
        [](CListCtrl* pList, CWnd* editorControl, const LVSubItemParams* pParams, bool bAcceptResult)
        {
            if (!bAcceptResult)
                return;

            CString currentEditorText;
            editorControl->GetWindowTextW(currentEditorText);

            pList->SetItemText(pParams->iItem, pParams->iSubItem, currentEditorText);
        },
        [](CWnd* window)
        {
            if (auto* comboBox = dynamic_cast<CComboBox*>(window))
                comboBox->ShowDropDown(TRUE);
        });
*/

#include "afxcmn.h"

#include <afxwin.h>
#include <map>
#include <memory>
#include <functional>

////////////////////////////////////////////////////////////////////////////////
// Структура с параметрами редактируемой ячейки листа
struct LVSubItemParams
{
    // упрощенное обращение к указателю на структуру
    typedef std::shared_ptr<LVSubItemParams> Ptr;

    LVSubItemParams(int iItem, int iSubItem, int iGroup)
        : iItem(iItem), iSubItem(iSubItem), iGroup(iGroup)
    {};

    // параметры редактируемого элемента
    int iItem;
    int iSubItem;
    int iGroup;
};

////////////////////////////////////////////////////////////////////////////////
// Интерфейс контроллера для редактирования элементов в таблице
interface ISubItemEditorController;

////////////////////////////////////////////////////////////////////////////////
/*
    CListEditSubItems - обертка над классами наследниками от CListCtrl для редактирования ячеек
    По умолчанию все колонки будут редактироваться через CEdit

    Можно задать кастомные контроллеры для редактирования ячеек через setSubItemEditorController.
    Они должны реализовывать ISubItemEditorController и пи создании контрола использовать стили
    получаемые через SubItemEditorControllerBase::getStandartEditorWndStyle()

Принцип работы:
    По дабл клику в ячейке создается дополнительное окно в которое вставляется заданный контрол для редактирования

    Если при редактировании контрола произойдет завершение родительского окна по IDCANCEL(::EndDialog(GetParent()->m_hWnd, IDCANCEL);),
    нажмется Ecs или иной другой способ вызвать IDCANCEL изменений не будет применены(в случае использования SubItemEditorControllerBase),
    в ISubItemEditorController::onEndEditSubItem придет bAcceptResult == false
    в случаях если придет IDOK - изменения будут применены(в случае использования SubItemEditorControllerBase) и bAcceptResult == true
*/
template <typename CBaseList = CListCtrl>
class CListEditSubItems : public CBaseList
{
public:
    CListEditSubItems() = default;

public:
    // задать контроллер для управления редактированием ячейки
    void setSubItemEditorController(int iSubItem,
                                    std::shared_ptr<ISubItemEditorController> controller);

    // контрол должен наследоваться от parentWindow и иметь стили
    // WS_CHILDWINDOW | WS_VISIBLE, см SubItemEditorControllerBase::getStandartEditorWndStyle()
    // @param pList - указатель на лист в котором происходит редактирование
    // @param parentWindow - родительское окно куда вставлен контрол
    // @param params - структура с параметрами о ячейке для которой создается контрол
    typedef std::function<std::shared_ptr<CWnd>(CListCtrl* pList,
                                                CWnd* parentWindow,
                                                const LVSubItemParams* pParams)> createEditorControlFunc;
    // Вызывается при окончании редактирования ячейки
    // @param pList - указатель на лист в котором происходит редактирование
    // @param editorControl - редактируемый контрол
    // @param pParams - структура с параметрами о ячейке для которой создается контрол
    // @param bAcceptResult - пользователь завершил редактирование применением изменений
    typedef std::function<void(CListCtrl* pList,
                               CWnd* editorControl,
                               const LVSubItemParams* pParams,
                               bool bAcceptResult)> onEndEditSubItemFunc;

    // Called after creation and insertion control into list
    typedef std::function<void(CWnd* editorControl)> onShowControlCallback;

    // задание контроллера для редактирования без создания объекта
    // через передачу функций, параметры функций идентичны аналогичным функциям ISubItemEditorController
    // если функция не задана будет использоваться стандартная обработка из SubItemEditorControllerBase
    // аналогичной функции
    void setSubItemEditorController(int iSubItem,
                                    const createEditorControlFunc& createFunc,
                                    const onEndEditSubItemFunc& endEditFunc = nullptr,
                                    const onShowControlCallback& onShowControl = nullptr);

protected://********************************************************************
    DECLARE_MESSAGE_MAP()

    virtual void PreSubclassWindow() override;

    // дабл клик с созданием контрола редактирования если необходимо
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    // оповоещение об окончании редактирования ячейки
    afx_msg LRESULT OnEndEditSubItem(WPARAM wParam, LPARAM lParam);

private:
    // получить фабрику использующуюся для редактирования ячейки
    // если не задана пользователем создаст SubItemEditorControllerBase
    // если фабрики для этой ячейки не предусмотрено вернет nullptr
    std::shared_ptr<ISubItemEditorController> getSubItemEditorController(int iSubItem);

private:
    // контроллеры для каждой колонки, если не задана - будет использоваться SubItemEditorFabricBase
    // если задана как nullptr, то колонка не редактируется
    std::map<int, std::shared_ptr<ISubItemEditorController>> m_subItemsEditorControllers;

    // окно редактирования объекта
    std::unique_ptr<class IEditSubItemWindow> m_editSubItemWindow;
};

////////////////////////////////////////////////////////////////////////////////
// Интерфейс контроллера для редактирования элементов в таблице
// см SubItemEditorControllerBase
interface ISubItemEditorController
{
    virtual ~ISubItemEditorController() = default;

    // Фабричный метод создания контрол для редактирования ячейки
    // контрол должен наследоваться от parentWindow и иметь стили
    // WS_CHILDWINDOW | WS_VISIBLE, см SubItemEditorControllerBase::getStandartEditorWndStyle()
    // @param pList - указатель на лист в котором происходит редактирование
    // @param parentWindow - родительское окно куда вставлен контрол
    // @param params - структура с параметрами о ячейке для которой создается контрол
    virtual std::shared_ptr<CWnd> createEditorControl(CListCtrl* pList,
                                                      CWnd* parentWindow,
                                                      const LVSubItemParams* pParams) = 0;

    // Called after insertion and showing control inside list
    virtual void onShowControl(CWnd* editorControl) = 0;

    // Вызывается при окончании редактирования ячейки
    // @param pList - указатель на лист в котором происходит редактирование
    // @param editorControl - редактируемый контрол
    // @param pParams - структура с параметрами о ячейке для которой создается контрол
    // @param bAcceptResult - пользователь завершил редактирование применением изменений
    virtual void onEndEditSubItem(CListCtrl* pList,
                                  CWnd* editorControl,
                                  const LVSubItemParams* pParams,
                                  bool bAcceptResult) = 0;
};

////////////////////////////////////////////////////////////////////////////////
// Базовый класс использующийся для редактирования элементов, по умолчанию создает поле ввода
class SubItemEditorControllerBase : public ISubItemEditorController
{
public:
    SubItemEditorControllerBase() = default;
    virtual ~SubItemEditorControllerBase() = default;

    // ISubItemEditorController
public:
    // создать контрол для редактирования, создает поле ввода для редактирования текста
    std::shared_ptr<CWnd> createEditorControl(CListCtrl* pList,
                                              CWnd* parentWindow,
                                              const LVSubItemParams* pParams) override;

    void onShowControl(CWnd* editorControl) override {}

    // вызывается при окончании редактирования ячейки
    // получает текст из окна и проставляет его в таблицу
    void onEndEditSubItem(CListCtrl* pList,
                          CWnd* editorControl,
                          const LVSubItemParams* pParams,
                          bool bAcceptResult) override;

public:
    // получить стиль окна для контрола с редактированием по умолчанию
    static DWORD getStandartEditorWndStyle()
    {
        return WS_CHILDWINDOW | WS_VISIBLE;
    }
};

// т.к класс шаблонный прячем реализацию в другой хедер
#include "ListEditSubItemsImpl.h"