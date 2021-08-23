/* winselect - a simple utility to select a Windows Installer
 * (C) 2021 by The Cat <code -at NOSPAM- kitsnotes.com>
 * https://github.com/kitsnotes/winselect
 * https://kitsnotes.com/winselect
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in the
 * Software without restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "winsel.h"

#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' "\
  "version='6.0.0.0' "\
  "processorArchitecture='*' "\
  "publicKeyToken='6595b64144ccf1df' "\
  "language='*'\"")

HWND		g_selDlg = NULL;
HINSTANCE   g_hInstance = NULL;

bool RequestClose()
{
#ifdef _DEBUG
	DestroyWindow(g_selDlg);
	g_selDlg = NULL;
	return true;
#else
	if (PromptForReboot())
	{
		if (RebootWindows())
		{
			PostQuitMessage(0);
			return true;
		}
	}
	return false;
#endif
}

bool InstallListItem(LPCTSTR item, LPCTSTR initag)
{
	HWND lb = GetDlgItem(g_selDlg, IDC_LIST);
	if (!IsWindow(lb))
		return false;

	LRESULT idx = SendMessage(lb, LB_ADDSTRING, (WPARAM)0, (LPARAM)item);
	if (idx == LB_ERR)
		return false;

	size_t size = strlen(initag) + 1;
	char* init = (char*)malloc(sizeof(char) * size);

	strcpy_s(init, sizeof(char) * size, initag);
	if (SendMessage(lb, LB_SETITEMDATA, idx, (WPARAM)init) == LB_ERR)
		return false;

	return true;
}

bool PromptForReboot()
{
	TCHAR szTitle[128];
	TCHAR szInfo[128];
	LoadString(g_hInstance, IDS_REBOOT_TITLE, szTitle,
		sizeof(szTitle) / sizeof(szTitle[0]));

	LoadString(g_hInstance, IDS_REBOOT_CONFIRM, szInfo,
		sizeof(szInfo) / sizeof(szInfo[0]));

	int res = MessageBox(g_selDlg, szInfo, szTitle, MB_YESNO | MB_ICONQUESTION);
	if (res == IDYES)
	{
		return true;
	}
	else
		return false;
}

bool StartInstaller()
{
	HWND lb = GetDlgItem(g_selDlg, IDC_LIST);

	LRESULT sel = SendMessage(lb, LB_GETCURSEL, 0, 0);
	if (sel == LB_ERR)
		return false;

	LRESULT result = SendMessage(lb, LB_GETITEMDATA, sel, 0);
	if (result == LB_ERR)
		return false;


	if (MountSMBDirectory((char*)result))
	{
		if(ExecSetup())
			return true;

		/* we failed to execute setup - we should unmount smb */
		WNetCancelConnection2(LOCALNAME_DISK, 0, true);
		return false;
	}

	return false;
}

INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_SYSCOMMAND:
		switch (LOWORD(wParam))
		{
		case ID_ABOUT: 
			return ShowAboutDialog();
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_INSTALL:
			return StartInstaller();
		case ID_REBOOT:	
			if (PromptForReboot())
				return RebootWindows();
			else
				return false;
		case ID_CMDP: 
			return ExecCmd();
		case IDC_LIST:
			switch (HIWORD(wParam))
			{
			case LBN_DBLCLK:
				return StartInstaller();
			case LBN_SELCHANGE:
				return EnableWindow(GetDlgItem(g_selDlg, ID_INSTALL), true);
			case LBN_SELCANCEL:
				return EnableWindow(GetDlgItem(g_selDlg, ID_INSTALL), false);
			}
		}
		break;
	case WM_CLOSE: 
		return RequestClose();
	case WM_DESTROY: 
		PostQuitMessage(0); 
		return true;
	}

	return false;
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MSG    msg;
	bool   mr;
	HMENU  sysMenu;
	HICON  sysIcon;
	TCHAR  szAppTitle[128];
	TCHAR  szAbout[128];
	TCHAR  szReboot[128];
	TCHAR  szMsgTitle[128];
	TCHAR  szInvFile[128];
	TCHAR  szHelpInfo[MAX_HELPTEXT];
	TCHAR  szHelpFile[128];
	TCHAR  szHelpSmb[128];

	g_hInstance = hInstance;

	/* prepare msgbox help text strings */
	LoadString(g_hInstance, IDS_UTILITY_USAGE, szMsgTitle,
		sizeof(szMsgTitle) / sizeof(szMsgTitle[0]));

	LoadString(g_hInstance, IDS_STARTHELP, szHelpInfo,
		sizeof(szHelpInfo) / sizeof(szHelpInfo[0]));

	LoadString(g_hInstance, IDS_INVFILE, szInvFile,
		sizeof(szInvFile) / sizeof(szInvFile[0]));

	LoadString(g_hInstance, IDS_UNCPATH, szHelpSmb,
		sizeof(szHelpSmb) / sizeof(szHelpSmb[0]));
	
	LoadString(g_hInstance, IDS_LOCALPATH, szHelpFile,
		sizeof(szHelpFile) / sizeof(szHelpFile[0]));

	/* cheap herustics of lpCmdLine */

	if (strncmp(lpCmdLine, "\\\\", 2) == 0)
	{
		/* we have a UNC path */
		/* in WinPE we need to manually mount this since the * 
		 * winpe system can not deal with a UNC in file apis natively */
		if (!TempMountSMB(lpCmdLine))
		{
			TCHAR helptmp[350];
			sprintf_s(helptmp, sizeof(helptmp), szHelpSmb, lpCmdLine, "Bad SMB Mount");
			TCHAR helpstr[MAX_HELPTEXT];
			sprintf_s(helpstr, sizeof(helpstr), "%s\n\n%s", helptmp, szHelpInfo);
			MessageBox(0, helpstr,
				szMsgTitle,
				MB_OK | MB_ICONINFORMATION);
			return 0;
		}

		if (GetFileAttributes(lpCmdLine) == INVALID_FILE_ATTRIBUTES)
		{
			TCHAR helptmp[350];
			sprintf_s(helptmp, sizeof(helptmp), szHelpSmb, lpCmdLine, szInvFile);
			TCHAR helpstr[MAX_HELPTEXT];
			sprintf_s(helpstr, sizeof(helpstr), "%s\n\n%s", helptmp, szHelpInfo);
			MessageBox(0, helpstr,
				szMsgTitle,
				MB_OK | MB_ICONINFORMATION);
			return 0;
		}
	}
	else
	{
		/* we have a local file */
		if (GetFileAttributes(lpCmdLine) == INVALID_FILE_ATTRIBUTES)
		{
			TCHAR helptmp[350];
			sprintf_s(helptmp, sizeof(helptmp), szHelpFile, lpCmdLine, szInvFile);
			TCHAR helpstr[MAX_HELPTEXT];
			sprintf_s(helpstr, sizeof(helpstr), "%s\n\n%s", helptmp, szHelpInfo);
			MessageBox(0, helpstr,
				szMsgTitle,
				MB_OK | MB_ICONINFORMATION);
			return 0;
		}
	}

	InitCommonControls();

	g_selDlg = CreateDialog(g_hInstance,
							MAKEINTRESOURCE(IDD_SELECT),
							NULL,
							DialogProc);

	if (!g_selDlg)
	{
		MessageBox(0, TEXT("CreateDialog failed. You should never see this. Unable to launch."), 
					  TEXT("Win32 Error."), 
					  MB_OK | MB_ICONERROR);

		return -1;
	}

	LoadString(g_hInstance, IDS_TITLE, szAppTitle, sizeof(szAppTitle) / sizeof(szAppTitle[0]));
	LoadString(g_hInstance, IDS_ABOUT, szAbout, sizeof(szAbout) / sizeof(szAbout[0]));
	LoadString(g_hInstance, IDS_REBOOT, szReboot, sizeof(szReboot) / sizeof(szReboot[0]));

	SetWindowText(g_selDlg, szAppTitle);

	sysIcon = (HICON)LoadImage(GetModuleHandle(NULL), 
									MAKEINTRESOURCE(IDI_MAIN),
									IMAGE_ICON, 0, 0, 
									LR_DEFAULTCOLOR | LR_DEFAULTSIZE);
	SendMessage(g_selDlg, WM_SETICON, ICON_BIG, (LPARAM)sysIcon);

	sysMenu = GetSystemMenu(g_selDlg, false);
	if(sysMenu)
	{
		/* Change our "Close" option to "Reboot" */
		MENUITEMINFO mi = { sizeof(MENUITEMINFO) };
		GetMenuItemInfo(sysMenu, SC_CLOSE, false, &mi);
		mi.dwTypeData = szReboot;
		mi.cch = sizeof(szReboot) / sizeof(szReboot[0]);
		ModifyMenu(sysMenu, SC_CLOSE, MF_BYCOMMAND|MF_STRING, SC_CLOSE, mi.dwTypeData);
		/* Append a separator and our About Utility menu*/
		AppendMenu(sysMenu, MF_BYPOSITION|MF_SEPARATOR, 0, "");
		AppendMenu(sysMenu, MF_BYPOSITION|MF_STRING, ID_ABOUT, szAbout);
	}
	
	/* Show the dialog and parse the INI file*/
	ShowWindow(g_selDlg, nCmdShow);
	LoadINIFromFS(lpCmdLine);

	/* The win32 dialog message loop */
	while((mr = GetMessage(&msg, 0, 0, 0)) != 0)
	{
		if(mr == -1)
			return -1;

		if(!IsDialogMessage(g_selDlg, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}
