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

#ifndef WINSEL_H
#define WINSEL_H
#pragma once

/* clean boolean support for my sanity */
#ifndef bool
#define bool BOOL
#define true TRUE
#define false FALSE
#endif /* bool */

#include <Windows.h>
#include <winnetwk.h>
#include <iphlpapi.h>
#include <CommCtrl.h>
#include <shlwapi.h>
#include <tchar.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "resource.h"

#define	MAX_HELPTEXT		4096
#define MAX_ININAMES        128
#define MAX_URL	            255
#define LOCALNAME_DISK		"Z:"

#define WM_DO_INIT		(WM_USER + 0x0001)

extern HWND        g_selDlg;
extern HINSTANCE   g_hInstance;
extern LPTSTR      g_iniFile;
extern bool        g_smbMounted;
extern LPTSTR      g_unattendFolder;
extern LPTSTR      g_unattendXml;
extern LPTSTR      g_unattendIni;
extern LPTSTR      g_lpCmdLine;
extern int         g_nCmdShow;
extern bool		   g_hasInit;
/* Functions: about.c */
INT_PTR CALLBACK   AboutDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool               ShowAboutDialog();

/* Functions: winsel.c */
bool               PromptForReboot();
bool               RequestClose();
bool               StartInstaller();
bool               InstallListItem(LPCTSTR item, LPCTSTR initag);
INT_PTR CALLBACK   DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Functions: func.c */
bool               RebootWindows();
bool               ExecCmd();
bool               ExecSetup();
bool               WinPEInit();
bool               LoadINIConfigOptions();
bool               LoadINIFromFS();
bool               TempMountSMB(LPCTSTR unc);
bool               MountSMBDirectory(LPCTSTR inisect);
bool               ParseINIFile();
bool               CheckForMACUnattend();
bool               ProcessMACBasedUnattendedInstall();
#endif /* WINSEL_H */
