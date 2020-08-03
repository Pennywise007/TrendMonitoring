
// TrendMonitorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "afxdialogex.h"
#include "TrendMonitor.h"

#include <boost/program_options.hpp>

#include <set>

#include <Controls/ComboBox/CComboBoxWithSearch/ComboWithSearch.h>

#include <ITrendMonitoring.h>

#include "BotSettingDlg.h"
#include "TrendMonitorDlg.h"
#include "TabTrendLog.h"
#include "TabReports.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

////////////////////////////////////////////////////////////////////////////////
// текст отображаемый в таблице пока грузятся данные по каналу
const CString kWaitingDataString = L"Загружается...";

// интерпретатор интервала мониторинга в текст
std::map<MonitoringInterval, CString> kMonitoringIntervalStrings;

//----------------------------------------------------------------------------//
// CTrendMonitorDlg dialog
CTrendMonitorDlg::CTrendMonitorDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_TRENDMONITOR_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    // добавляем все значение интервалов в комбобокс
    for (int i = 0; i < (int)MonitoringInterval::eLast; ++i)
    {
        MonitoringInterval interval = (MonitoringInterval)i;
        kMonitoringIntervalStrings[interval] = monitoring_interval_to_string(interval);
    }
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_MONITORING_CHANNELS, m_monitorChannelsList);
    DDX_Control(pDX, IDC_TAB, m_tabCtrl);
}

//----------------------------------------------------------------------------//
BEGIN_MESSAGE_MAP(CTrendMonitorDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_CLOSE()
    ON_BN_CLICKED(IDC_MFCBUTTON_ADD, &CTrendMonitorDlg::OnBnClickedMfcbuttonAdd)
    ON_BN_CLICKED(IDC_MFCBUTTON_REMOVE, &CTrendMonitorDlg::OnBnClickedMfcbuttonRemove)
    ON_BN_CLICKED(IDC_MFCBUTTON_MOVE_UP, &CTrendMonitorDlg::OnBnClickedMfcbuttonMoveUp)
    ON_BN_CLICKED(IDC_MFCBUTTON_MOVE_DOWN, &CTrendMonitorDlg::OnBnClickedMfcbuttonMoveDown)
    ON_BN_CLICKED(IDC_MFCBUTTON_REFRESH, &CTrendMonitorDlg::OnBnClickedMfcbuttonRefresh)
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST_MONITORING_CHANNELS, &CTrendMonitorDlg::OnNMCustomdrawListMonitoringChannels)
    ON_WM_SYSCOMMAND()
    ON_WM_WINDOWPOSCHANGING()
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_MONITORING_CHANNELS, &CTrendMonitorDlg::OnLvnItemchangedListMonitoringChannels)
END_MESSAGE_MAP()

//----------------------------------------------------------------------------//
BOOL CTrendMonitorDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    // парсим и применяем настройки из командной строки
    processCommandLine();

    // инициализируем контролы, добавляем колонки таблицы и вкладки
    initControls();

    // подписываемся на событие изменение списка
    EventRecipientImpl::subscribe(onMonitoringListChanged);
    // подписываемся на событие добавления сообщений в лог
    EventRecipientImpl::subscribe(onNewLogMessageEvent);
    // подписываемся на событие готовности отчёта
    EventRecipientImpl::subscribe(onReportPreparedEvent);

    // загружаем данные и ждем информацию по ним
    reloadChannelsList();

    // Добавляем иконку в трей
    addTrayIcon();

    return TRUE;  // return TRUE  unless you set the focus to a control
}

//----------------------------------------------------------------------------//
// IMessagesRecipient
void CTrendMonitorDlg::onEvent(const EventId& code, float /*eventValue*/,
                               std::shared_ptr<IEventData> eventData)
{
    if (code == onMonitoringListChanged)
        reloadChannelsList();
    else if (code == onNewLogMessageEvent)
    {
        std::shared_ptr<LogMessageData> logMessageData = std::static_pointer_cast<LogMessageData>(eventData);

        showTrayNotification(L"Новое сообщение в логе",
                             logMessageData->logMessage,
                             logMessageData->messageType == LogMessageData::MessageType::eError ?
                             NIIF_ERROR : NIIF_WARNING,
                             [tabControl = &m_tabCtrl]()
                             {
                                 // переклчюаемся не вкладку с логом
                                 tabControl->SetCurSel(TabInices::eTabLog);
                             });

    }
    else if (code == onReportPreparedEvent)
    {
        showTrayNotification(L"Новый отчёт сформирован",
                             L"Нажмите чтобы ознакомиться с ним",
                             NIIF_INFO,
                             [tabControl = &m_tabCtrl]()
                             {
                                 // переклчюаемся не вкладку с логом
                                 tabControl->SetCurSel(TabInices::eTabReport);
                             });
    }
    else
        assert(!"Возникло не обработанное событие на которое подписались");
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

//----------------------------------------------------------------------------//
HCURSOR CTrendMonitorDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnOK()
{
    // не даем закрыться по Enter`у
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnClose()
{
    // отписываемся от событий
    EventRecipientImpl::unsubscribe(onMonitoringListChanged);
    EventRecipientImpl::unsubscribe(onNewLogMessageEvent);
    EventRecipientImpl::unsubscribe(onReportPreparedEvent);

    m_trayHelper.removeTrayIcon();

    __super::OnClose();
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnBnClickedMfcbuttonAdd()
{
    selectChannelsListItem(get_monitoing_service()->addMonitoingChannel());
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnBnClickedMfcbuttonRemove()
{
    POSITION pos = m_monitorChannelsList.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_monitorChannelsList.GetNextSelectedItem(pos);

        selectChannelsListItem(get_monitoing_service()->removeMonitoringChannelByIndex(nItem));
    }
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnBnClickedMfcbuttonMoveUp()
{
    POSITION pos = m_monitorChannelsList.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_monitorChannelsList.GetNextSelectedItem(pos);

        selectChannelsListItem(get_monitoing_service()->moveUpMonitoingChannelByIndex((size_t)nItem));
    }
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnBnClickedMfcbuttonMoveDown()
{
    POSITION pos = m_monitorChannelsList.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_monitorChannelsList.GetNextSelectedItem(pos);
        selectChannelsListItem(get_monitoing_service()->moveDownMonitoingChannelByIndex((size_t)nItem));
    }
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnBnClickedMfcbuttonRefresh()
{
    get_monitoing_service()->updateDataForAllChannels();
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::processCommandLine()
{
    // получаем командную строку
    int nArgc = 0;
    LPWSTR *pArgv = ::CommandLineToArgvW(::GetCommandLine(), &nArgc);

    // заполняем параметрами командной строки
    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("hidden", "Скрытый режим запуска программы");

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::parse_command_line(nArgc, pArgv, desc), vm);

    m_bHiddenDialog = !vm["hidden"].empty();
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::initControls()
{
    m_monitorChannelsList.ModifyStyle(0, LVS_EX_CHECKBOXES);

    // создаем колонки таблицы
    m_monitorChannelsList.InsertColumn(TableColumns::eNotify,           L"Оповещать", -1, 45);
    m_monitorChannelsList.InsertColumn(TableColumns::eChannelName,      L"Название канала",         LVCFMT_LEFT, 145, -1,
                                       [](const CString& str1, const CString& str2) mutable
                                       {
                                           return str1.CompareNoCase(str2);
                                       });

    m_monitorChannelsList.InsertColumn(TableColumns::eInterval,         L"Интервал мониторинга",    LVCFMT_CENTER, 80, -1,
                                       [](const CString& str1, const CString& str2) mutable
                                       {
                                           return str1.CompareNoCase(str2);
                                       });
    m_monitorChannelsList.InsertColumn(TableColumns::eAllarmingValue,   L"Оповестить при достижении", LVCFMT_CENTER, 90);
    m_monitorChannelsList.InsertColumn(TableColumns::eLastDataExistTime,L"Последние существующие данные",   LVCFMT_CENTER, 140);
    m_monitorChannelsList.InsertColumn(TableColumns::eNoDataTime,       L"Нет данных",     LVCFMT_CENTER, 120);

    const int kValueSize = 100;
    m_monitorChannelsList.InsertColumn(TableColumns::eStartValue,       L"Начальное значение",      LVCFMT_CENTER, kValueSize);
    m_monitorChannelsList.InsertColumn(TableColumns::eCurrentValue,     L"Текущее значение",        LVCFMT_CENTER, kValueSize);
    m_monitorChannelsList.InsertColumn(TableColumns::eMaxValue,         L"Максимальное значение",   LVCFMT_CENTER, kValueSize);
    m_monitorChannelsList.InsertColumn(TableColumns::eMinValue,         L"Минимальное значение",    LVCFMT_CENTER, kValueSize);

    // задаем редакторы для таблицы
    for (int i = 0, count = TableColumns::eColumnsCount; i < count; ++i)
    {
        std::shared_ptr<CWnd> pControlWindow;

        switch ((TableColumns)i)
        {
        case TableColumns::eChannelName:
            {
                m_monitorChannelsList.setSubItemEditorController(
                    TableColumns::eChannelName,
                    [](CListCtrl* pList, CWnd* parentWindow, const LVSubItemParams* pParams) -> std::shared_ptr<CWnd>
                    {
                        ComboWithSearch* comboBox = new ComboWithSearch;

                        comboBox->Create(SubItemEditorControllerBase::getStandartEditorWndStyle() |
                                         CBS_DROPDOWN | CBS_HASSTRINGS | CBS_AUTOHSCROLL | WS_VSCROLL,
                                         CRect(), parentWindow, 0);

                        // добавляем все каналы из списка в комбобокс
                        auto allChennelsNames = get_monitoing_service()->getNamesOfAllChannels();
                        for (const auto& channelName : allChennelsNames)
                            comboBox->AddString(channelName);

                        // получаем текущее имя канала которое выбранно в таблицы
                        CString curSubItemText = pList->GetItemText(pParams->iItem, pParams->iSubItem);
                        if (!curSubItemText.IsEmpty())
                        {
                            // ищем и ставим на текущий выделенный канал выделение в комбобоксе
                            auto it = allChennelsNames.find(curSubItemText);
                            if (it != allChennelsNames.end())
                                comboBox->SetCurSel((int)std::distance(allChennelsNames.begin(), it));
                        }

                        return std::shared_ptr<CWnd>(comboBox);
                    },
                    [](CListCtrl* pList,
                       CWnd* editorControl,
                       const LVSubItemParams* pParams,
                       bool bAcceptResult)
                    {
                        CString text;
                        editorControl->GetWindowText(text);

                        pList->SetItemText(pParams->iItem, pParams->iSubItem, text);

                        get_monitoing_service()->changeMonitoingChannelName(pParams->iItem, text);
                    });
            }
            break;
        case TableColumns::eInterval:
            m_monitorChannelsList.setSubItemEditorController(
                TableColumns::eInterval,
                [](CListCtrl* pList, CWnd* parentWindow, const LVSubItemParams* pParams) -> std::shared_ptr<CWnd>
                {
                    ComboWithSearch* comboBox = new ComboWithSearch;

                    comboBox->Create(SubItemEditorControllerBase::getStandartEditorWndStyle() |
                                     CBS_DROPDOWNLIST | CBS_HASSTRINGS | CBS_AUTOHSCROLL | WS_VSCROLL,
                                     CRect(), parentWindow, 0);

                    // добавляем все значение интервалов в комбобокс
                    for (const auto& intervalStr : kMonitoringIntervalStrings)
                        comboBox->AddString(intervalStr.second);

                    // получаем текущее имя канала которое выбранно в таблицы
                    CString curSubItemText = pList->GetItemText(pParams->iItem, pParams->iSubItem);
                    if (!curSubItemText.IsEmpty())
                    {
                        // ищем и ставим на текущий выделенный канал выделение в комбобоксе
                        auto it = std::find_if(kMonitoringIntervalStrings.begin(),
                                               kMonitoringIntervalStrings.end(),
                                               [&curSubItemText](const auto& intervalPair)
                                               {
                                                   return intervalPair.second == curSubItemText;
                                               });
                        if (it != kMonitoringIntervalStrings.end())
                            comboBox->SetCurSel((int)std::distance(kMonitoringIntervalStrings.begin(), it));
                    }

                    return std::shared_ptr<CWnd>(comboBox);
                },
                [](CListCtrl* pList,
                   CWnd* editorControl,
                   const LVSubItemParams* pParams,
                   bool bAcceptResult)
                {
                    CString text;
                    editorControl->GetWindowText(text);

                    pList->SetItemText(pParams->iItem, pParams->iSubItem, text);

                    // ищем и сообщаем какой новый интервал мониторинга у канала
                    auto it = std::find_if(kMonitoringIntervalStrings.begin(),
                                           kMonitoringIntervalStrings.end(),
                                           [&text](const auto& intervalPair)
                                           {
                                               return intervalPair.second == text;
                                           });
                    if (it != kMonitoringIntervalStrings.end())
                        get_monitoing_service()->changeMonitoingChannelInterval(pParams->iItem, it->first);
                    else
                        assert(false);
                });
            break;
        case TableColumns::eAllarmingValue:
            m_monitorChannelsList.setSubItemEditorController(
                TableColumns::eAllarmingValue,
                nullptr,
                [](CListCtrl* pList,
                   CWnd* editorControl,
                   const LVSubItemParams* pParams,
                   bool bAcceptResult)
                {
                    CString text;
                    editorControl->GetWindowText(text);

                    pList->SetItemText(pParams->iItem, pParams->iSubItem, text);

                    if (text == L"-")
                        get_monitoing_service()->changeMonitoingChannelAllarmingValue(pParams->iItem, NAN);
                    else
                        get_monitoing_service()->changeMonitoingChannelAllarmingValue(pParams->iItem, (float)_wtof(text));
                });
            break;
        case TableColumns::eNotify:
        case TableColumns::eLastDataExistTime:
        case TableColumns::eNoDataTime:
        case TableColumns::eStartValue:
        case TableColumns::eCurrentValue:
        case TableColumns::eMaxValue:
        case TableColumns::eMinValue:
            // не редактируемые колонки
            m_monitorChannelsList.setSubItemEditorController(i, std::shared_ptr<ISubItemEditorController>());
            break;
        default:
            assert(!"Неизвестная колонка, добавление редактора не удалось");
            break;
        }
    }

    // вставляем табы с логом и отчётами
    m_tabCtrl.InsertTab(TabInices::eTabLog, L"Лог событий",
                        std::make_shared<СTabTrendLog>(), IDD_TAB_EVENTS_LOG);
    m_tabCtrl.InsertTab(TabInices::eTabReport, L"Отчёты",
                        std::make_shared<CTabReports>(), IDD_TAB_REPORTS);
}

////////////////////////////////////////////////////////////////////////////////
// Анонимный неймспейс для внутренних функций которыми не хочется торчать наружу
namespace
{
    //------------------------------------------------------------------------//
    // функция конвертации данных мониторинга в строку
    CString monitoring_data_to_string(const float value)
    {
        CString res;
        res.Format(L"%.02f", value);

        assert(!res.IsEmpty());
        return res;
    }

    //------------------------------------------------------------------------//
    // функция конвертации данных мониторинга в строку
    CString monitoring_data_to_string(const CTime& value)
    {
        return value.Format(L"%d.%m.%Y %H:%M:%S");
    }

    //------------------------------------------------------------------------//
    /// Функция рассчета градиента
    /// @param startColor - начальный цвет
    /// @param endColor - конечный цвет
    /// @param percent - процент на сколько результат должен быть похож на startColor и endColor
    /// @return результирующий градиентный цвет
    COLORREF calc_graditent_color(COLORREF startColor, COLORREF endColor, float percent)
    {
        if (percent <= 0.f)
            return startColor;
        if (percent >= 1.f)
            return endColor;

        BYTE red = BYTE((1.f - percent) * GetRValue(startColor) +
                        percent * GetRValue(endColor));
        BYTE green = BYTE((1.f - percent) * GetGValue(startColor) +
                          percent * GetGValue(endColor));
        BYTE blue = BYTE((1.f - percent) * GetBValue(startColor) +
                         percent * GetBValue(endColor));

        return RGB(red, green, blue);
    }
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::reloadChannelsList()
{
    // Перезаполняем нашу таблицу

    // получаем сервис с данными
    ITrendMonitoring* monitoingService = get_monitoing_service();

    // получаем текущее и новое количество каналов
    int newChannelsCount = (int)monitoingService->getNumberOfMonitoringChannels();
    int curChannelsCount = m_monitorChannelsList.GetItemCount();

    // ресайзим таблицу до нашего количества каналов
    while (curChannelsCount != newChannelsCount)
    {
        if (curChannelsCount < newChannelsCount)
            m_monitorChannelsList.InsertItem(curChannelsCount++, L"");
        else
            m_monitorChannelsList.DeleteItem(--curChannelsCount);
    }

    // проходим по всем каналам и заносим в таблицу информацию о канале
    for (int channelIndex = 0, channelsCount = (int)monitoingService->getNumberOfMonitoringChannels();
         channelIndex < channelsCount; ++channelIndex)
    {
        const MonitoringChannelData& channelData = monitoingService->getMonitoringChannelData(channelIndex);
        // отрабатываем оповещения
        m_monitorChannelsList.SetCheck(channelIndex, channelData.bNotify);
        m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eChannelName,     channelData.channelName);
        m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eInterval,        monitoring_interval_to_string(channelData.monitoringInterval));

        CString allarmingValStr = _finite(channelData.allarmingValue) != 0 ? monitoring_data_to_string(channelData.allarmingValue) : L"-";
        m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eAllarmingValue,  allarmingValStr);

        if (channelData.channelState[MonitoringChannelData::ChannelState::eDataLoaded])
        {
            // заполняем данные трендов если они были загружены
            const auto& trendData = channelData.trendData;

            m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eLastDataExistTime, monitoring_data_to_string(trendData.lastDataExistTime));
            m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eNoDataTime,      time_span_to_string(trendData.emptyDataTime));
            m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eStartValue,      monitoring_data_to_string(trendData.startValue));

            // считаем на сколько изменилось значение
            float deltaVal = trendData.currentValue - trendData.startValue;
            CString currentValStr;
            currentValStr.Format(L"%s (%s)", monitoring_data_to_string(trendData.currentValue).GetString(),
                ((deltaVal > 0 ? L"+" : L"") + monitoring_data_to_string(deltaVal)).GetString());
            m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eCurrentValue,    currentValStr);

            m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eMaxValue,        monitoring_data_to_string(trendData.maxValue));
            m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eMinValue,        monitoring_data_to_string(trendData.minValue));
        }
        else
        {
            CString columnsText;
            if (channelData.channelState[MonitoringChannelData::ChannelState::eLoadingError])
                columnsText = L"Возникла ошибка";
            else
                // нет не данных не ошибки - данные ещё не загрузились
                columnsText = kWaitingDataString;

            // ставим колонкам соответствующий текст
            for (int i = TableColumns::eStartDataColumns; i <= TableColumns::eEndDataColumns; ++i)
                m_monitorChannelsList.SetItemText(channelIndex, i, columnsText);
        }
    }
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::selectChannelsListItem(const size_t index)
{
    int nItem = (int)index;
    m_monitorChannelsList.EnsureVisible(nItem, FALSE);
    m_monitorChannelsList.SetItemState(nItem, LVIS_FOCUSED | LVIS_SELECTED,
                                       LVIS_FOCUSED | LVIS_SELECTED);
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::addTrayIcon()
{
    // Добавляем иконку в трей
    m_trayHelper.addTrayIcon(m_hIcon, L"Мониторинг данных",
                             []()
                             {
                                 return ::GetSubMenu(LoadMenu(AfxGetInstanceHandle(),
                                                              MAKEINTRESOURCE(IDR_MENU_TRAY)),
                                                     0);
                             },
                             nullptr,
                             [this](UINT commandId)
                             {
                                 switch (commandId)
                                 {
                                 case ID_MENU_OPEN:
                                     // Показываем наш диалог
                                     restoreDlg();
                                     break;
                                 case ID_MENU_BOTSETTINGS:
                                     {
                                         // Показываем диалог настройки
                                         CBotSettingDlg botSettingsDlg;
                                         botSettingsDlg.DoModal();
                                     }
                                     break;
                                 case ID_MENU_CLOSE:
                                     EndDialog(IDCANCEL);
                                     break;
                                 default:
                                     assert(!"Не известный пункт меню!");
                                     break;
                                 }
                             },
                             [this]()
                             {
                                 // Показываем наш диалог
                                 restoreDlg();
                             });
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::showTrayNotification(const CString& title,
                                            const CString& descr,
                                            DWORD dwBubbleFlags /*= NIIF_WARNING*/,
                                            const CTrayHelper::OnUserClick& onUserClick /*= nullptr*/)
{
    // проксируем в вспомогательный класс, добавляем показ нашего диалога при клике
    m_trayHelper.showBubble(title, descr, dwBubbleFlags,
                            [this, onUserClick]()
                            {
                                // Показываем наш диалог
                                restoreDlg();

                                // отрабатываем клик
                                if (onUserClick)
                                    onUserClick();
                            });
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::restoreDlg()
{
    m_bHiddenDialog = false;

    // Показываем наш диалог
    ShowWindow(SW_RESTORE);
    SetForegroundWindow();
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnNMCustomdrawListMonitoringChannels(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLVCUSTOMDRAW pNMCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);

    // Сначало надо определить текущую стадию
    switch (pNMCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        // если рисуется весь элемент целиком - запрашиваем получение сообщений
        // для каждого элемента списка.
        *pResult = CDRF_NOTIFYSUBITEMDRAW;
        break;

    case CDDS_ITEMPREPAINT:
        // если рисуется весь элемент списка целиком - запрашиваем получение сообщений
        // для каждого подэлемента списка.
        *pResult = CDRF_NOTIFYSUBITEMDRAW;
        break;

    case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
        {
            // Стадия, которая наступает перед отрисовкой каждого элемента списка.
            int iSubItem = pNMCD->iSubItem;

            // ставим значение фона по умолчанию
            pNMCD->clrTextBk = m_monitorChannelsList.GetTextBkColor();

            if (iSubItem == TableColumns::eCurrentValue)
            {
                // если это колонка с текущим значением проверяем каким цветом его отрисовать
                DWORD_PTR iItem = pNMCD->nmcd.dwItemSpec;

                const MonitoringChannelData& monitoringData =
                    get_monitoing_service()->getMonitoringChannelData(iItem);
                if (monitoringData.channelState[MonitoringChannelData::ChannelState::eDataLoaded] &&
                    _finite(monitoringData.allarmingValue) != 0)
                {
                    // на сколько текущее значение близко в критическому
                    float percent = monitoringData.trendData.currentValue /
                        monitoringData.allarmingValue;

                    // допустимое значение
                    const float allowablePercent = 0.8f;

                    if (percent > allowablePercent)
                    {
                        float allowableValue = monitoringData.allarmingValue * allowablePercent;

                        // если процент больше 80 будем показывать градиентным цветом
                        // рассчитываем цветность в оставшихся 80%
                        float newPercent = (monitoringData.trendData.currentValue - allowableValue) /
                            (monitoringData.allarmingValue - allowableValue);

                        pNMCD->clrTextBk = calc_graditent_color(RGB(255, 255, 128), RGB(255, 128, 128), newPercent);
                    }
                    else
                        // показываем обычным цветом
                        pNMCD->clrTextBk = RGB(128, 255, 128);
                }
            }

            // Уведомляем систему, чтобы она самостоятельно нарисовала элемент.
            *pResult = CDRF_DODEFAULT;
        }
        break;

    default:
        // Будем выполнять стандартную обработку для всех сообщений по умолчанию
        *pResult = CDRF_DODEFAULT;
        break;
    }
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    switch (nID)
    {
    case SC_MINIMIZE:
        // скрываем наше приложение
        ShowWindow(SW_MINIMIZE);
        ShowWindow(SW_HIDE);
        // оповещаем что приложение свернуто
        showTrayNotification(L"Приложение свёрнуто в трей",
                             L"Для разворачивания приложения нажмите два раза на иконку или выберите соответствующий пункт меню.",
                             NIIF_INFO);
        break;
    }

    __super::OnSysCommand(nID, lParam);
}

//----------------------------------------------------------------------------//
void CTrendMonitorDlg::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
    // прячем диалог
    if (m_bHiddenDialog)
        lpwndpos->flags &= ~SWP_SHOWWINDOW;

    __super::OnWindowPosChanging(lpwndpos);
}


void CTrendMonitorDlg::OnLvnItemchangedListMonitoringChannels(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    if (pNMLV->uChanged & LVIF_STATE)
    {
        bool bNotify = false;

        switch (pNMLV->uNewState & LVIS_STATEIMAGEMASK)
        {
        case INDEXTOSTATEIMAGEMASK(2):
            get_monitoing_service()->changeMonitoingChannelNotify(pNMLV->iItem, false);
            break;
        case INDEXTOSTATEIMAGEMASK(1):
            get_monitoing_service()->changeMonitoingChannelNotify(pNMLV->iItem, false);
            break;
        }
    }

    // TODO: Add your control notification handler code here
    *pResult = 0;
}
