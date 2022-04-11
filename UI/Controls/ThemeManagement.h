#pragma once

// Управление темами для контролов/окна
// см https://docs.microsoft.com/ru-ru/windows/win32/controls/parts-and-states?redirectedfrom=MSDN

#include <afx.h>
#include <libloaderapi.h>
#include <shlwapi.h>

////////////////////////////////////////////////////////////////////////////////
/* Установить тему для окна / контрола
   Примеры

   EnableWindowTheme(GetSafeHwnd(), L"ListView", L"Explorer", NULL);


   EnableWindowTheme(m_hWnd, L"WINDOW", L"Explorer", NULL);
   SetWindowTheme(m_hWnd, L"Explorer", NULL);

*/
inline LRESULT EnableWindowTheme(HWND hwnd, LPCWSTR classList, LPCWSTR subApp, LPCWSTR idlist)
{
    LRESULT lResult = S_FALSE;

    HRESULT(__stdcall *pSetWindowTheme)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList);
    HANDLE(__stdcall *pOpenThemeData)(HWND hwnd, LPCWSTR pszClassList);
    HRESULT(__stdcall *pCloseThemeData)(HANDLE hTheme);

    HMODULE hinstDll = ::LoadLibrary(_T("UxTheme.dll"));
    if (hinstDll)
    {
        (FARPROC&)pOpenThemeData = ::GetProcAddress(hinstDll, "OpenThemeData");
        (FARPROC&)pCloseThemeData = ::GetProcAddress(hinstDll, "CloseThemeData");
        (FARPROC&)pSetWindowTheme = ::GetProcAddress(hinstDll, "SetWindowTheme");
        if (pSetWindowTheme && pOpenThemeData && pCloseThemeData)
        {
            HANDLE theme = pOpenThemeData(hwnd, classList);
            if (theme != NULL)
            {
                VERIFY(pCloseThemeData(theme) == S_OK);
                lResult = pSetWindowTheme(hwnd, subApp, idlist);
            }
        }
        ::FreeLibrary(hinstDll);
    }
    return lResult;
}

//----------------------------------------------------------------------------//
inline bool IsCommonControlsEnabled()
{
    bool commoncontrols = false;

    // Test if application has access to common controls
    HMODULE hinstDll = ::LoadLibrary(_T("comctl32.dll"));
    if (hinstDll)
    {
        DLLGETVERSIONPROC pDllGetVersion = (DLLGETVERSIONPROC)::GetProcAddress(hinstDll, "DllGetVersion");
        if (pDllGetVersion != NULL)
        {
            DLLVERSIONINFO dvi = { 0 };
            dvi.cbSize = sizeof(dvi);
            HRESULT hRes = pDllGetVersion((DLLVERSIONINFO *)&dvi);
            if (SUCCEEDED(hRes))
                commoncontrols = dvi.dwMajorVersion >= 6;
        }
        ::FreeLibrary(hinstDll);
    }
    return commoncontrols;
}

//----------------------------------------------------------------------------//
inline bool IsThemeEnabled()
{
    bool XPStyle = false;
    bool(__stdcall *pIsAppThemed)();
    bool(__stdcall *pIsThemeActive)();

    // Test if operating system has themes enabled
    HMODULE hinstDll = ::LoadLibrary(_T("UxTheme.dll"));
    if (hinstDll)
    {
        (FARPROC&)pIsAppThemed = ::GetProcAddress(hinstDll, "IsAppThemed");
        (FARPROC&)pIsThemeActive = ::GetProcAddress(hinstDll, "IsThemeActive");
        if (pIsAppThemed != NULL && pIsThemeActive != NULL)
        {
            if (pIsAppThemed() && pIsThemeActive())
            {
                // Test if application has themes enabled by loading the proper DLL
                XPStyle = IsCommonControlsEnabled();
            }
        }
        ::FreeLibrary(hinstDll);
    }
    return XPStyle;
}