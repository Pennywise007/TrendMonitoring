// CBotSettingDlg.cpp : implementation file
//

#include "stdafx.h"
#include "TrendMonitor.h"
#include "afxdialogex.h"

#include "BotSettingDlg.h"

#include <include/ITelegramBot.h>

////////////////////////////////////////////////////////////////////////////////
// CBotSettingDlg dialog
IMPLEMENT_DYNAMIC(CBotSettingDlg, CDialogEx)

//----------------------------------------------------------------------------//
CBotSettingDlg::CBotSettingDlg(ext::ServiceProvider::Ptr&& provider, CWnd* pParent /*=nullptr*/)
    : ServiceProviderHolder(std::move(provider))
    , CDialogEx(IDD_BOT_SETTINGS_DLG, pParent)
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

    bool enable;
    std::wstring token;
    ServiceProviderHolder::ServiceProviderHolder::GetInterface<telegram::bot::ITelegramBotSettings>()->GetSettings(enable, token);

    ((CWnd*)GetDlgItem(IDC_EDIT_TOKEN))->SetWindowText(token.c_str());

    int enebleButtonId = enable ? IDC_RADIO_ENABLE_ON : IDC_RADIO_ENABLE_OFF;
    ((CButton*)GetDlgItem(enebleButtonId))->SetCheck(BST_CHECKED);

    return TRUE;
}

//----------------------------------------------------------------------------//
void CBotSettingDlg::OnOK()
{
    CString newToken;
    GetDlgItem(IDC_EDIT_TOKEN)->GetWindowText(newToken);

    bool newEnableState = GetCheckedRadioButton(IDC_RADIO_ENABLE_ON, IDC_RADIO_ENABLE_OFF) == IDC_RADIO_ENABLE_ON;

    bool enable;
    std::wstring token;
    ServiceProviderHolder::ServiceProviderHolder::GetInterface<telegram::bot::ITelegramBotSettings>()->GetSettings(enable, token);

    const bool bTokenChanged = newToken != token.c_str();
    bool bEnableChanged = newEnableState != enable;

    if (!token.empty() && bTokenChanged)
    {
        if (MessageBox(L"Вы действительно хотите поменять токен бота?", L"Внимание", MB_OKCANCEL | MB_ICONWARNING) != IDOK)
            return;
    }

    // если указали токен, но не включили бота спрашиваем мб включить
    if (bTokenChanged && token.empty() && (!bEnableChanged && !newEnableState))
    {
        if (MessageBox(L"Вы задали токен, но не включили бота. Включить?", L"Внимание", MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
        {
            newEnableState = true;
            bEnableChanged = true;
        }
    }

    // оповещаем только об изменениях
    if (bTokenChanged || bEnableChanged)
        ServiceProviderHolder::ServiceProviderHolder::GetInterface<telegram::bot::ITelegramBotSettings>()->SetSettings(newEnableState, newToken.GetString());

    CDialogEx::OnOK();
}
