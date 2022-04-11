#pragma once

#include <afxcmn.h>

class TextProgressCtrl : public CProgressCtrl
{
    DECLARE_DYNAMIC(TextProgressCtrl)

public:
    TextProgressCtrl() = default;
    virtual ~TextProgressCtrl();

protected:
    DECLARE_MESSAGE_MAP()

    afx_msg void OnPaint();

public:
    void	SetZeroRange( short range );
    void	SetPosition( int pos );
    void	SetIndeterminate( BOOL bInf = TRUE ) const;
    void	Pause() const;
    void	Error() const;
};
