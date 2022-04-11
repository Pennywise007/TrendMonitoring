#pragma once
#include <afxwin.h>
#include <map>

////////////////////////////////////////////////////////////////////////////////
// вспомогательный интерфейс сохранения выделения в комбобоксе
interface ISelectionSaver
{
    virtual ~ISelectionSaver() = default;

    // получает текущее выделение
    virtual int  getRealCurSel() = 0;
    // устанавливает текущее выделение
    virtual void setRealCurSel(int selection) = 0;
};

////////////////////////////////////////////////////////////////////////////////
class ComboWithSearch :
    public CComboBox,
    public ISelectionSaver
{
    typedef CComboBox BaseCtrl;
public:
    ComboWithSearch() = default;

protected:
    DECLARE_MESSAGE_MAP();

    // for processing Windows messages
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

// ISelectionSaver
public:
    // получает текущее выделение
    int  getRealCurSel() override;
    // устанавливает текущее выделение
    void setRealCurSel(int selection) override;

public:
    // сообщение об изменении текста
    BOOL OnCbnEditchange();
    // сообщение о выборе текущего элемента
    BOOL OnCbnSelendok();
    // сообщение об отмене выбора
    BOOL OnCbnSelendcancel();
    // оповещение о убирании фокуса с контроа
    BOOL OnCbnKillfocus();
    // таймер
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    // нажатие клавиш
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

private:
    // выполняем поиск
    void executeSearch();
    // сбрасываем фильтр
    bool resetSearch();
    // обновляем текущий реально выделенный элемент
    void updateRealCurSelFromCurrent();
    // регулируем выпадающий список к контенту комбобокса
    void adjustComboBoxToContent();

private:    // функции конвертации индексов
    // из текущего индекса в реальный индекс
    int convertToRealIndex(int index);
    // из реального индекса в текущий
    int convertFromRealIndex(int index);

private:
    // отфильтрованные строки
    //      индекс | текст
    std::map<int, std::wstring> m_filteredItems;

    // реальное значение выделенного элемента, с учетом удаленных строк
    int m_realSel = -1;
};