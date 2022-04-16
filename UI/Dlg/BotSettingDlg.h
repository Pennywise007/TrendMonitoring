#pragma once

#include <string>
#include <memory>

#include <include/ITelegramBot.h>

// диалог настройки бота Telegram
class CBotSettingDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CBotSettingDlg)

public:
    CBotSettingDlg(std::shared_ptr<telegram::bot::ITelegramBotSettings> botSettings,
                   CWnd* pParent = nullptr);   // standard constructor

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
    std::shared_ptr<telegram::bot::ITelegramBotSettings> m_botSettings;
};
