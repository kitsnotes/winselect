/* winselect - a simple utility to select a Windows Installer
 * (C) 2021 by The Cat <code -at NOSPAM- kitsnotes.com>
 * https://github.com/kitsnotes/winselect
 * This file is MIT Licensed
 */

#include "winsel.h"
 
/* About Dialog Win32 Pesentation Functions */
INT_PTR CALLBACK AboutDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hwnd, 0);
			return true;
		}
		break;
	case WM_CLOSE: EndDialog(hwnd, 0); return true;
	}

	return false;
}

bool ShowAboutDialog()
{
	DialogBox(g_hInstance,
		MAKEINTRESOURCE(IDD_ABOUT),
		g_selDlg != NULL ? g_selDlg : 0,
		AboutDialogProc);

	return true;
}