//-----------------------------------------------------------------------------
#pragma once

#include <afxwin.h>
//-----------------------------------------------------------------------------
class CStaticColor : public CStatic
{
private:
	// ������������, ���� �� ����� ���� ����
	bool m_bTransparent;			// = true;
	// ���� ���� �� ��������� ����� (���� ���-�� ������ �������������� ��� ��������� �����)
	COLORREF m_colorBk;				// = GetSysColor(COLOR_3DFACE)
	// ���� ������ �� ��������� ������
	COLORREF m_colorText;			// = GetSysColor(COLOR_BTNTEXT)
	// ����� ��� ��������� ����
	HBRUSH m_brushBk;

	CString m_text;
		
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
public:
	CStaticColor(const COLORREF colorBackground = GetSysColor(COLOR_3DFACE), const COLORREF colorText = GetSysColor(COLOR_BTNTEXT));
	~CStaticColor();
	CStaticColor(const CStaticColor & val);
	CStaticColor & operator = (const CStaticColor & val);

	// ������� ��� ���������� (��� ���� bTransparent == true, ������������ ���������� ���� ����)
	bool GetTransparent();
	void SetTransparent(const bool bTransparent);

	// ���� ����. ��� ��� ������� ��� �������� ������������
	COLORREF GetBkColor();
	void SetBkColor(const COLORREF color);

	// ���� ������
	COLORREF GetTextColor();
	void SetTextColor(const COLORREF color);
};
//-----------------------------------------------------------------------------