
// TrendMonitorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "afxdialogex.h"
#include "UI.h"

#include <filesystem>
#include <set>

#include <boost/program_options.hpp>

#include <ext/core/dependency_injection.h>
#include <ext/scope/auto_setter.h>
#include <ext/scope/on_exit.h>

#include <Controls/ComboBox/CComboBoxWithSearch/ComboWithSearch.h>

#include <include/ITrendMonitoring.h>
#include <include/IDirService.h>

#include "BotSettingDlg.h"
#include "MainDlg.h"
#include "Tabs/TabTrendLog.h"
#include "Tabs/TabReports.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

////////////////////////////////////////////////////////////////////////////////
// текст отображаемый в таблице пока грузятся данные по каналу
const CString kWaitingDataString = L"Загружается...";

// интерпретатор интервала мониторинга в текст
std::map<MonitoringInterval, CString> kMonitoringIntervalStrings;

//----------------------------------------------------------------------------//
// MainDlg dialog
MainDlg::MainDlg(ext::ServiceProvider::Ptr serviceProvider, CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_TRENDMONITOR_DIALOG, pParent)
    , ServiceProviderHolder(serviceProvider)
    , ScopeSubscription(false)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    // добавляем все значение интервалов в комбобокс
    for (int i = 0; i < (int)MonitoringInterval::eLast; ++i)
    {
        const MonitoringInterval interval = (MonitoringInterval)i;
        kMonitoringIntervalStrings[interval] = monitoring_interval_to_string(interval).c_str();
    }
}

//----------------------------------------------------------------------------//
void MainDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_MONITORING_CHANNELS, m_monitorChannelsList);
    DDX_Control(pDX, IDC_TAB, m_tabCtrl);
}

//----------------------------------------------------------------------------//
BEGIN_MESSAGE_MAP(MainDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_CLOSE()
    ON_BN_CLICKED(IDC_MFCBUTTON_ADD, &MainDlg::OnBnClickedMfcbuttonAdd)
    ON_BN_CLICKED(IDC_MFCBUTTON_REMOVE, &MainDlg::OnBnClickedMfcbuttonRemove)
    ON_BN_CLICKED(IDC_MFCBUTTON_MOVE_UP, &MainDlg::OnBnClickedMfcbuttonMoveUp)
    ON_BN_CLICKED(IDC_MFCBUTTON_MOVE_DOWN, &MainDlg::OnBnClickedMfcbuttonMoveDown)
    ON_BN_CLICKED(IDC_MFCBUTTON_REFRESH, &MainDlg::OnBnClickedMfcbuttonRefresh)
    ON_BN_CLICKED(IDC_MFCBUTTON_SHOW_TRENDS, &MainDlg::OnBnClickedMfcbuttonShowTrends)
    ON_NOTIFY(NM_CUSTOMDRAW,    IDC_LIST_MONITORING_CHANNELS, &MainDlg::OnNMCustomdrawListMonitoringChannels)
    ON_NOTIFY(LVN_ITEMCHANGED,  IDC_LIST_MONITORING_CHANNELS, &MainDlg::OnLvnItemchangedListMonitoringChannels)
    ON_WM_SYSCOMMAND()
    ON_WM_WINDOWPOSCHANGING()
END_MESSAGE_MAP()

//----------------------------------------------------------------------------//
BOOL MainDlg::OnInitDialog()
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

    ScopeSubscription::SubscribeAll();

    // загружаем данные и ждем информацию по ним
    reloadChannelsList();

    // Добавляем иконку в трей
    addTrayIcon();

    return TRUE;  // return TRUE  unless you set the focus to a control
}

void MainDlg::OnChanged()
{
    reloadChannelsList();
}

void MainDlg::OnNewLogMessage(const std::shared_ptr<LogMessageData>& logMessage)
{
    showTrayNotification(L"Новое сообщение в логе",
                         logMessage->logMessage.c_str(),
                         logMessage->messageType == LogMessageData::MessageType::eError ?
                         NIIF_ERROR : NIIF_WARNING,
                         [tabControl = &m_tabCtrl]()
    {
        // переклчюаемся не вкладку с логом
        tabControl->SetCurSel(TabIndexes::eTabLog);
    });
}

void MainDlg::OnReportDone(std::wstring /*messageText*/)
{
    showTrayNotification(L"Новый отчёт сформирован",
                         L"Нажмите чтобы ознакомиться с ним",
                         NIIF_INFO,
                         [tabControl = &m_tabCtrl]()
    {
        // переклчюаемся не вкладку с логом
        tabControl->SetCurSel(TabIndexes::eTabReport);
    });
}

//----------------------------------------------------------------------------//
void MainDlg::initControls()
{
    m_monitorChannelsList.ModifyStyle(0, LVS_EX_CHECKBOXES);

    // создаем колонки таблицы
    m_monitorChannelsList.InsertColumn(TableColumns::eNotify,           L"Оповещать", -1, 25);
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
    m_monitorChannelsList.InsertColumn(TableColumns::eAlarmingValue,    L"Оповестить при достижении", LVCFMT_CENTER, 90);
    m_monitorChannelsList.InsertColumn(TableColumns::eLastDataExistTime,L"Последние существующие данные",   LVCFMT_CENTER, 140);
    m_monitorChannelsList.InsertColumn(TableColumns::eNoDataTime,       L"Нет данных",              LVCFMT_CENTER, 120);

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
                    [&](CListCtrl* pList, CWnd* parentWindow, const LVSubItemParams* pParams) -> std::shared_ptr<CWnd>
                    {
                        ComboWithSearch* comboBox = new ComboWithSearch;

                        comboBox->Create(SubItemEditorControllerBase::getStandartEditorWndStyle() |
                                         CBS_DROPDOWN | CBS_HASSTRINGS | CBS_AUTOHSCROLL | WS_VSCROLL,
                                         CRect(), parentWindow, 0);

                        // добавляем все каналы из списка в комбобокс
                        auto allChannelsNames = ServiceProviderHolder::GetInterface<ITrendMonitoring>()->GetNamesOfAllChannels();
                        allChannelsNames.sort([](const std::wstring& lhs, const std::wstring& rhs)
                        {
                            return lhs.compare(rhs) < 0;
                        });

                        for (const auto& channelName : allChannelsNames)
                            comboBox->AddString(channelName.c_str());

                        // получаем текущее имя канала которое выбранно в таблицы
                        std::wstring curSubItemText = pList->GetItemText(pParams->iItem, pParams->iSubItem).GetString();
                        if (!curSubItemText.empty())
                        {
                            // ищем и ставим на текущий выделенный канал выделение в комбобоксе
                            const auto it = std::find(allChannelsNames.begin(), allChannelsNames.end(), curSubItemText);
                            if (it != allChannelsNames.end())
                                comboBox->SetCurSel((int)std::distance(allChannelsNames.begin(), it));
                        }

                        return std::shared_ptr<CWnd>(comboBox);
                    },
                    [&](CListCtrl* pList,
                       CWnd* editorControl,
                       const LVSubItemParams* pParams,
                       bool bAcceptResult)
                    {
                        if (!bAcceptResult)
                            return;

                        CString text;
                        editorControl->GetWindowText(text);

                        pList->SetItemText(pParams->iItem, pParams->iSubItem, text);

                        ServiceProviderHolder::GetInterface<ITrendMonitoring>()->ChangeMonitoringChannelName(pParams->iItem, text.GetString());
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
                [&](CListCtrl* pList,
                   CWnd* editorControl,
                   const LVSubItemParams* pParams,
                   bool bAcceptResult)
                {
                    if (!bAcceptResult)
                        return;

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
                        ServiceProviderHolder::GetInterface<ITrendMonitoring>()->ChangeMonitoringChannelInterval(pParams->iItem, it->first);
                    else
                        EXT_ASSERT(false);
                });
            break;
        case TableColumns::eAlarmingValue:
            m_monitorChannelsList.setSubItemEditorController(
                TableColumns::eAlarmingValue,
                nullptr,
                [&](CListCtrl* pList,
                   CWnd* editorControl,
                   const LVSubItemParams* pParams,
                   bool bAcceptResult)
                {
                    if (!bAcceptResult)
                        return;

                    CString text;
                    editorControl->GetWindowText(text);

                    pList->SetItemText(pParams->iItem, pParams->iSubItem, text);

                    if (text == L"-")
                        ServiceProviderHolder::GetInterface<ITrendMonitoring>()->ChangeMonitoringChannelAlarmingValue(pParams->iItem, NAN);
                    else
                        ServiceProviderHolder::GetInterface<ITrendMonitoring>()->ChangeMonitoringChannelAlarmingValue(pParams->iItem, (float)_wtof(text));
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
            EXT_ASSERT(!"Неизвестная колонка, добавление редактора не удалось");
            break;
        }
    }

    // вставляем табы с логом и отчётами
    m_tabCtrl.InsertTab(TabIndexes::eTabLog, L"Лог событий",
                        std::make_shared<CTabTrendLog>(), IDD_TAB_EVENTS_LOG);
    m_tabCtrl.InsertTab(TabIndexes::eTabReport, L"Отчёты",
                        std::make_shared<CTabReports>(), IDD_TAB_REPORTS);
}

//----------------------------------------------------------------------------//
void MainDlg::processCommandLine()
{
    // получаем командную строку
    int nArgc = 0;
    LPWSTR *pArgv = ::CommandLineToArgvW(::GetCommandLine(), &nArgc);

    try
    {
        // заполняем параметрами командной строки
        boost::program_options::options_description desc("Allowed options");
        desc.add_options()
            ("hidden", "Скрытый режим запуска программы");

        int xstyle = 0;
        {
            using namespace boost::program_options::command_line_style;
            // если хотим настроить стиль - надо поставить стандартные стили
            // в частности нужен allow_long для long_case_insensitive
            xstyle = default_style | case_insensitive;
        }

        // делаем так, а не через boost::program_options::parse_command_line ибо нам надо allow_unregistered
        // чтобы если ввели не корректную командную строку мы что-то да распарсили
        boost::program_options::wparsed_options parsed =
            boost::program_options::wcommand_line_parser(nArgc, pArgv)
            .options(desc).style(xstyle).allow_unregistered().run();
        // опции которые есть в результате
        boost::program_options::variables_map vm;
        boost::program_options::store(parsed, vm);

        m_bHiddenDialog = !vm["hidden"].empty();
    }
    catch (const boost::program_options::error& err)
    {
        ::MessageBox(NULL, CString(err.what()),
                     L"Возникло исключение при анализе командной строки", MB_ICONERROR | MB_OK);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Анонимный неймспейс для внутренних функций которыми не хочется торчать наружу
namespace {
//------------------------------------------------------------------------//
// функция конвертации данных мониторинга в строку
CString monitoring_data_to_string(const float value)
{
    CString res;
    res.Format(L"%.02f", value);

    EXT_ASSERT(!res.IsEmpty());
    return res;
}

//------------------------------------------------------------------------//
// функция конвертации данных мониторинга в строку
CString monitoring_data_to_string(const CTime& value)
{
    return value.Format(L"%d.%m.%Y %H:%M");
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
void MainDlg::reloadChannelsList()
{
    // Перезаполняем нашу таблицу
    ext::scope::AutoSet set(m_bUpdatingTable, true, false);

    // получаем сервис с данными
    auto monitoringService = ServiceProviderHolder::GetInterface<ITrendMonitoring>();

    // получаем текущее и новое количество каналов
    int newChannelsCount = (int)monitoringService->GetNumberOfMonitoringChannels();
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
    for (int channelIndex = 0, channelsCount = (int)monitoringService->GetNumberOfMonitoringChannels();
         channelIndex < channelsCount; ++channelIndex)
    {
        const MonitoringChannelData& channelData = monitoringService->GetMonitoringChannelData(channelIndex);
        // отрабатываем оповещения
        m_monitorChannelsList.SetCheck(channelIndex, channelData.bNotify);
        m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eChannelName,     channelData.channelName.c_str());
        m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eInterval,        monitoring_interval_to_string(channelData.monitoringInterval).c_str());

        CString alarmingValStr = _finite(channelData.alarmingValue) != 0 ? monitoring_data_to_string(channelData.alarmingValue) : L"-";
        m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eAlarmingValue,  alarmingValStr);

        if (channelData.channelState.dataLoaded)
        {
            // заполняем данные трендов если они были загружены
            const auto& trendData = channelData.trendData;

            // будем писать форматом 12.02.20 15:00 (5 ч)
            CString lastExistData = monitoring_data_to_string(trendData.lastDataExistTime);
            if (CTimeSpan noDataTime(CTime::GetCurrentTime() - trendData.lastDataExistTime);
                noDataTime.GetTotalMinutes() > 10)
                lastExistData.AppendFormat(L" (%s)", time_span_to_string(noDataTime).c_str());

            m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eLastDataExistTime, std::move(lastExistData));
            m_monitorChannelsList.SetItemText(channelIndex, TableColumns::eNoDataTime,      time_span_to_string(trendData.emptyDataTime).c_str());
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
            if (channelData.channelState.loadingDataError)
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
void MainDlg::selectChannelsListItem(const size_t index)
{
    int nItem = (int)index;
    m_monitorChannelsList.EnsureVisible(nItem, FALSE);
    m_monitorChannelsList.SetItemState(nItem, LVIS_FOCUSED | LVIS_SELECTED,
                                       LVIS_FOCUSED | LVIS_SELECTED);
}

//----------------------------------------------------------------------------//
void MainDlg::addTrayIcon()
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
                                         auto botSettingsDlg = ServiceProviderHolder::CreateObject<CBotSettingDlg>();
                                         botSettingsDlg->DoModal();
                                     }
                                     break;
                                 case ID_MENU_CLOSE:
                                     EndDialog(IDCANCEL);
                                     break;
                                 default:
                                     EXT_ASSERT(!"Не известный пункт меню!");
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
void MainDlg::showTrayNotification(const CString& title,
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
void MainDlg::restoreDlg()
{
    m_bHiddenDialog = false;

    // Показываем наш диалог
    ShowWindow(SW_RESTORE);
    SetForegroundWindow();
}

//----------------------------------------------------------------------------//
void MainDlg::OnNMCustomdrawListMonitoringChannels(NMHDR *pNMHDR, LRESULT *pResult)
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
                    ServiceProviderHolder::GetInterface<ITrendMonitoring>()->GetMonitoringChannelData(iItem);
                if (monitoringData.channelState.dataLoaded &&
                    _finite(monitoringData.alarmingValue) != 0)
                {
                    // на сколько текущее значение близко в критическому
                    float percent = monitoringData.trendData.currentValue /
                        monitoringData.alarmingValue;

                    // допустимое значение
                    const float allowablePercent = 0.8f;

                    if (percent > allowablePercent)
                    {
                        float allowableValue = monitoringData.alarmingValue * allowablePercent;

                        // если процент больше 80 будем показывать градиентным цветом
                        // рассчитываем цветность в оставшихся 80%
                        float newPercent = (monitoringData.trendData.currentValue - allowableValue) /
                            (monitoringData.alarmingValue - allowableValue);

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
void MainDlg::OnLvnItemchangedListMonitoringChannels(NMHDR* pNMHDR,
                                                              LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    if (!m_bUpdatingTable && pNMLV->uChanged & LVIF_STATE)
    {
        switch (pNMLV->uNewState & LVIS_STATEIMAGEMASK)
        {
        case INDEXTOSTATEIMAGEMASK(2):
            ServiceProviderHolder::GetInterface<ITrendMonitoring>()->ChangeMonitoringChannelNotify(pNMLV->iItem, true);
            break;
        case INDEXTOSTATEIMAGEMASK(1):
            ServiceProviderHolder::GetInterface<ITrendMonitoring>()->ChangeMonitoringChannelNotify(pNMLV->iItem, false);
            break;
        }
    }

    *pResult = 0;
}

//----------------------------------------------------------------------------//
void MainDlg::OnSysCommand(UINT nID, LPARAM lParam)
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
void MainDlg::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
    // прячем диалог
    if (m_bHiddenDialog)
        lpwndpos->flags &= ~SWP_SHOWWINDOW;

    __super::OnWindowPosChanging(lpwndpos);
}

//----------------------------------------------------------------------------//
void MainDlg::OnPaint()
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
HCURSOR MainDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

//----------------------------------------------------------------------------//
void MainDlg::OnOK()
{
    // не даем закрыться по Enter`у
}

//----------------------------------------------------------------------------//
void MainDlg::OnClose()
{
    ScopeSubscription::UnsubscribeAll();

    m_trayHelper.removeTrayIcon();

    __super::OnClose();
}

//----------------------------------------------------------------------------//
void MainDlg::OnBnClickedMfcbuttonAdd()
{
    try
    {
        selectChannelsListItem(ServiceProviderHolder::GetInterface<ITrendMonitoring>()->AddMonitoringChannel());
    }
    catch (std::exception&)
    {
        MessageBoxW(ext::ManageExceptionText<wchar_t>().c_str(), L"Не удалось найти каналы для мониторинга.", MB_ICONERROR | MB_OK);
    }
}

//----------------------------------------------------------------------------//
void MainDlg::OnBnClickedMfcbuttonRemove()
{
    POSITION pos = m_monitorChannelsList.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_monitorChannelsList.GetNextSelectedItem(pos);

        selectChannelsListItem(ServiceProviderHolder::GetInterface<ITrendMonitoring>()->RemoveMonitoringChannelByIndex(nItem));
    }
}

//----------------------------------------------------------------------------//
void MainDlg::OnBnClickedMfcbuttonMoveUp()
{
    POSITION pos = m_monitorChannelsList.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_monitorChannelsList.GetNextSelectedItem(pos);

        selectChannelsListItem(ServiceProviderHolder::GetInterface<ITrendMonitoring>()->MoveUpMonitoringChannelByIndex((size_t)nItem));
    }
}

//----------------------------------------------------------------------------//
void MainDlg::OnBnClickedMfcbuttonMoveDown()
{
    POSITION pos = m_monitorChannelsList.GetFirstSelectedItemPosition();
    if (pos != NULL)
    {
        int nItem = m_monitorChannelsList.GetNextSelectedItem(pos);
        selectChannelsListItem(ServiceProviderHolder::GetInterface<ITrendMonitoring>()->MoveDownMonitoringChannelByIndex((size_t)nItem));
    }
}

//----------------------------------------------------------------------------//
void MainDlg::OnBnClickedMfcbuttonRefresh()
{
    ServiceProviderHolder::GetInterface<ITrendMonitoring>()->UpdateDataForAllChannels();
}

//----------------------------------------------------------------------------//
void MainDlg::OnBnClickedMfcbuttonShowTrends()
{
    const auto monitoringService = ServiceProviderHolder::GetInterface<ITrendMonitoring>();

    const size_t countChannels = monitoringService->GetNumberOfMonitoringChannels();
    if (countChannels == 0)
    {
        MessageBox(L"", L"Нет выбранных каналов!", MB_OK);
        return;
    }

    // список каналдов через ;
    CString channels;
    // максимальный временной промежуток мониторинга
    CTimeSpan maxTimeSpan = 0;
    for (size_t ind = 0; ind < countChannels; ++ind)
    {
        const MonitoringChannelData& channelData = monitoringService->GetMonitoringChannelData(ind);
        channels.AppendFormat(L"%s;", channelData.channelName.c_str());
        maxTimeSpan = std::max<CTimeSpan>(maxTimeSpan, monitoring_interval_to_timespan(channelData.monitoringInterval));
    }

    // начальный и конечный интервал
    CTime endTime = CTime::GetCurrentTime();
    CTime startTime = endTime - maxTimeSpan;

    // дополнительный отступ чтобы было удобнее смотреть
    const CTimeSpan extraTime(std::max<LONG>(static_cast<long>(maxTimeSpan.GetDays()) / 10l, 1l), 0, 0, 0);
    endTime += extraTime;
    startTime -= extraTime;

    // form the command line
    CString commandLine;
    commandLine.Format(L" -ip 127.0.0.1 -start %s -end %s -channels \"%s\" -autoscale",
                       startTime.Format(L"%d.%m.%Y").GetString(),
                       endTime  .Format(L"%d.%m.%Y").GetString(),
                       channels.GetString());

    constexpr auto kTrendProgramName = L"ZETTrends.exe";
    // if command line is too long zet trends will close with error, try to start it and send command line via WM_COPYDATA
    HWND trendsMainWindow = FindMainWindowOfExe(kTrendProgramName);
    if (trendsMainWindow == nullptr)
    {
        // Getting full path to trends
        CString trendsFullPath;
        {
            CString zetInstallFolder = ServiceProviderHolder::GetInterface<IDirService>()->GetZetInstallDir().c_str();
            if (zetInstallFolder.IsEmpty())
            {
                MessageBox(L"", L"Директория установки зетлаба не найдена.", MB_OK);
                return;
            }

            trendsFullPath = std::move(zetInstallFolder) + kTrendProgramName;
            if (!std::filesystem::is_regular_file(trendsFullPath.GetString()))
            {
                MessageBox(L"", L"Программа просмотра трендов не найдена!", MB_OK);
                return;
            }
        }

        // start up exe
        STARTUPINFO cif = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION m_ProcInfo = { 0 };
        if (FALSE != CreateProcess(trendsFullPath.GetBuffer(),  // имя исполнdяемого модуля
            NULL,	                     // Командная строка
            NULL,                        // Указатель на структуру SECURITY_ATTRIBUTES
            NULL,                        // Указатель на структуру SECURITY_ATTRIBUTES
            0,                           // Флаг наследования текущего процесса
            NULL,                        // Флаги способов создания процесса
            NULL,                        // Указатель на блок среды
            NULL,                        // Текущий диск или каталог
            &cif,                        // Указатель на структуру STARTUPINFO
            &m_ProcInfo))                // Указатель на структуру PROCESS_INFORMATION)
        {	// идентификатор потока не нужен
            CloseHandle(m_ProcInfo.hThread);
            CloseHandle(m_ProcInfo.hProcess);

            // wait for start
            Sleep(1000);
        }

        trendsMainWindow = FindMainWindowOfExe(kTrendProgramName);
        if (trendsMainWindow == nullptr)
        {
            MessageBox(L"", CString(L"Can`t start ") + kTrendProgramName, MB_OK);
            return;
        }
    }

    // send WM_COPYDATA with command line
    COPYDATASTRUCT cds;
    cds.dwData = 0;
    cds.cbData = (commandLine.GetLength() + 1) * sizeof(wchar_t);
    cds.lpData = (PVOID)commandLine.GetBuffer();
    if (cds.lpData != 0)
        memcpy_s(cds.lpData, cds.cbData, commandLine.GetBuffer(), cds.cbData);
    ::SendMessage(trendsMainWindow, WM_COPYDATA, (WPARAM)trendsMainWindow, (LPARAM)&cds);

    ::OpenIcon(trendsMainWindow);
    ::SetActiveWindow(trendsMainWindow);
    ::SetForegroundWindow(trendsMainWindow);
}
