
// UI.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CUIApp:
// See UI.cpp for the implementation of this class
//

inline HWND FindMainWindowOfExe(CString exeName, bool ignoreCurrentProcess = true);

class CUIApp : public CWinApp
{
public:
	CUIApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CUIApp theApp;
