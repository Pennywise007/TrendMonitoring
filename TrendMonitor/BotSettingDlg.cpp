// CBotSettingDlg.cpp : implementation file
//

#include "stdafx.h"
#include "TrendMonitor.h"
#include "afxdialogex.h"

#include "BotSettingDlg.h"

////////////////////////////////////////////////////////////////////////////////
// CBotSettingDlg dialog
IMPLEMENT_DYNAMIC(CBotSettingDlg, CDialogEx)

//----------------------------------------------------------------------------//
CBotSettingDlg::CBotSettingDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_BOT_SETTINGS_DLG, pParent)
{
}

//----------------------------------------------------------------------------//
CBotSettingDlg::~CBotSettingDlg()
{
}

//----------------------------------------------------------------------------//
void CBotSettingDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

//----------------------------------------------------------------------------//
BEGIN_MESSAGE_MAP(CBotSettingDlg, CDialogEx)
END_MESSAGE_MAP()

//----------------------------------------------------------------------------//
BOOL CBotSettingDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_botSettings = get_monitoing_service()->getBotSettings();

    ((CWnd*)GetDlgItem(IDC_EDIT_TOKEN))->SetWindowText(m_botSettings.sToken);

    int enebleButtonId = m_botSettings.bEnable ? IDC_RADIO_ENABLE_ON : IDC_RADIO_ENABLE_OFF;
    ((CButton*)GetDlgItem(enebleButtonId))->SetCheck(BST_CHECKED);

    return TRUE;
}

//----------------------------------------------------------------------------//
void CBotSettingDlg::OnOK()
{
    CString token;
    ((CWnd*)GetDlgItem(IDC_EDIT_TOKEN))->GetWindowText(token);

    bool bEnableState = GetCheckedRadioButton(IDC_RADIO_ENABLE_ON, IDC_RADIO_ENABLE_OFF) == IDC_RADIO_ENABLE_ON;

    bool bTokenChanged = m_botSettings.sToken != token;
    bool bEnableChanged = m_botSettings.bEnable != bEnableState;

    if (!m_botSettings.sToken.IsEmpty() && bTokenChanged)
    {
        if (MessageBox(L"Вы действительно хотите поменять токен бота?", L"Внимание", MB_OKCANCEL | MB_ICONWARNING) != IDOK)
            return;
    }

    // если указали токен, но не включили бота спрашиваем мб включить
    if (bTokenChanged && m_botSettings.sToken.IsEmpty() && (!bEnableChanged && !bEnableState))
    {
        if (MessageBox(L"Вы задали токен, но не включили бота. Включить?", L"Внимание", MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
        {
            bEnableState = true;
            bEnableChanged = true;
        }
    }

    if (bTokenChanged)
        m_botSettings.sToken = std::move(token);
    if (bEnableChanged)
        m_botSettings.bEnable = bEnableState;

    // оповещаем только об изменениях
    if (bTokenChanged || bEnableChanged)
        get_monitoing_service()->setBotSettings(m_botSettings);

    CDialogEx::OnOK();
}
