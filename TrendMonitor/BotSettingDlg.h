#pragma once

#include <string>

#include <ext/core/dependency_injection.h>

// диалог настройки бота Telegram
class CBotSettingDlg : ext::ServiceProviderHolder, public CDialogEx
{
    DECLARE_DYNAMIC(CBotSettingDlg)

public:
    CBotSettingDlg(ext::ServiceProvider::Ptr&& provider, CWnd* pParent = nullptr);   // standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_BOT_SETTINGS_DLG };
#endif

protected:
    DECLARE_MESSAGE_MAP()

    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual void OnOK();
};
