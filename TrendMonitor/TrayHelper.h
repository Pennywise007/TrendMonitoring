#pragma once

#include <afxdialogex.h>
#include <functional>

////////////////////////////////////////////////////////////////////////////////
// Вспомогательный класс для настройки трея и показа оповещений
class CTrayHelper : public CDialogEx
{
public:
    CTrayHelper();
    ~CTrayHelper();

    // функция запроса создания меню
    typedef std::function<HMENU()> CreateTrayMenu;
    // функция вызывается для уничтожения созданного меню
    typedef std::function<void(HMENU)> DestroyTrayMenu;
    // функция выбора элемента в меню
    typedef std::function<void(UINT)> OnSelectTrayMenu;
    // функция вызывается при оповещении о клике
    typedef std::function<void()> OnUserClick;
    // функция вызывается при оповещении о дабл клике
    typedef std::function<void()> OnUserDBLClick;
public:
    /// <summary>Добавить иконку в трей.</summary>
    /// <param name="icon">Иконка в трее.</param>
    /// <param name="tipText">Подсказка в трее.</param>
    /// <param name="createTrayMenu">Создание меню по ПКМ/ЛКМ по иконке трея.</param>
    /// <param name="destroyTrayMenu">Удаление меню, если nullptr будет вызван стандартный ::DestroyMenu.</param>
    /// <param name="onSelectTrayMenu">Функция вызываемая при клике на команду меню.</param>
    /// <param name="onDBLClick">Двойной клик по меню иконке.</param>
    void addTrayIcon(const HICON& icon,
                     const CString& tipText,
                     const CreateTrayMenu& createTrayMenu = nullptr,
                     const DestroyTrayMenu& destroyTrayMenu = nullptr,
                     const OnSelectTrayMenu& onSelectTrayMenu = nullptr,
                     const OnUserDBLClick& onDBLClick = nullptr);
    /// <summary>Уджалить иконку из трея.</summary>
    void removeTrayIcon();
    /// <summary>Показать виндовое оповещение.</summary>
    /// <param name="title">Заголовок оповещения.</param>
    /// <param name="descr">Описаение оповещения.</param>
    /// <param name="dwBubbleFlags">Флаг с которыми показывать оповещение.</param>
    /// <param name="onUserClick">Клик пользователя по иконке.</param>
    void showBubble(const CString& title,
                    const CString& descr,
                    const DWORD dwBubbleFlags = NIIF_WARNING,
                    const OnUserClick& onUserClick = nullptr);

protected:
    DECLARE_MESSAGE_MAP()

    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTrayIcon(WPARAM wParam, LPARAM lParam);

// Параметры отображения иконки в трее
private:
    // флаг наличия иконки в трее
    bool m_bTrayIconExist = false;

    // Колбэки для управления треем
    CreateTrayMenu   m_trayCreateMenu       = nullptr;   // создание меню
    DestroyTrayMenu  m_trayDestroyMenu      = nullptr;   // удаление меню
    OnSelectTrayMenu m_traySelectMenuItem   = nullptr;   // выбор из меню
    OnUserDBLClick   m_trayDBLUserClick     = nullptr;   // дабл клик в меню

// Параметры для нотификаций
private:
    OnUserClick      m_notificationClick    = nullptr;   // клик по нотификации

private:
    // структура с параметрами нотификации
    NOTIFYICONDATA   m_niData;
};

