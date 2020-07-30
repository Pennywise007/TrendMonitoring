#pragma once

#include <string>

#include "ITrendMonitoring.h"

// диалог настройки бота Telegram
class CBotSettingDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CBotSettingDlg)

public:
    CBotSettingDlg(CWnd* pParent = nullptr);   // standard constructor
    virtual ~CBotSettingDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_BOT_SETTINGS_DLG };
#endif

protected:
    DECLARE_MESSAGE_MAP()

    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual void OnOK();

private:
    // настройки телеграм бота
    TelegramBotSettings m_botSettings;
};
