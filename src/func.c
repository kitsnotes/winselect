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

LPCTSTR g_iniFile = NULL;

bool RebootWindows()
{
	HANDLE            ht;
	TOKEN_PRIVILEGES  priv;

	/* We need to allow ourselves privilege tokens to reboot */
	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &ht))
		return false;

	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &priv.Privileges[0].Luid);
	priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	priv.PrivilegeCount = 1;

	AdjustTokenPrivileges(ht, false, &priv, sizeof(priv), (PTOKEN_PRIVILEGES)NULL, 0);
	if (GetLastError() != ERROR_SUCCESS)
		return false;

	return ExitWindowsEx(EWX_REBOOT|EWX_FORCEIFHUNG,
		SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
}

bool ParseINIFile(LPCTSTR path)
{
	TCHAR  stringName[MAX_ININAMES];
	TCHAR  retbuf[MAX_ININAMES];
	TCHAR* pNext = NULL;

	if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
		return false;

	g_iniFile = path;

	/* this is a loop to go through every profile section name *
	 * and install a list box item on the main dialog          */
	
	if (GetPrivateProfileSectionNames((LPTSTR)&retbuf, sizeof(retbuf), path) > 0)
	{
		pNext = retbuf;
		GetPrivateProfileString(pNext, TEXT("Name"), NULL, stringName, sizeof(stringName), path);
		
		/* we're not using this atm but reserving 'config' as a header for future use*/
		if (pNext != "config")
			InstallListItem(stringName, pNext); /* winsel.c */
		while (*pNext != 0x00)
		{
			pNext = pNext + strlen(pNext) + 1;
			if (*pNext != 0x00 && pNext != "config")
			{
				GetPrivateProfileString(pNext, TEXT("Name"), NULL, stringName, sizeof(stringName), path);
				InstallListItem(stringName, pNext); /* winsel.c */
			}
		}
		return true;
	}

	return false;
}

bool LoadINIFromFS(LPCTSTR path)
{
	/* an entry point for loading an ini configuration   *
	 * from a 'filesystem' including SMB.  Eventually I  *
	 * would like to expand to pull from HTTP?           */

	if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
		return false;

	return ParseINIFile(path);
}

bool MountSMBDirectory(LPCTSTR initag)
{
	/* run some sanity checks and mount our smb drive at Z:/ */
	TCHAR  smbpath[MAX_PATH];
	TCHAR  smbuser[255];
	TCHAR  smbpass[255];
	TCHAR  szErrorInfo[512];
	TCHAR  szTitle[128];
	TCHAR  szInvalidPath[50];

	/* Prepare some error strings */
	LoadString(g_hInstance, IDS_SMBERROR, szErrorInfo,
		sizeof(szErrorInfo) / sizeof(szErrorInfo[0]));

	LoadString(g_hInstance, IDS_SMBERRORTITLE, szTitle,
		sizeof(szTitle) / sizeof(szTitle[0]));

	LoadString(g_hInstance, IDS_INVALIDPATH, szInvalidPath,
		sizeof(szInvalidPath) / sizeof(szInvalidPath[0]));

	GetPrivateProfileString(initag, TEXT("Path"), NULL, smbpath, sizeof(smbpath), g_iniFile);
	if (smbpath == "")
		return false; /* No valid Path */

	if (GetFileAttributes(smbpath) == INVALID_FILE_ATTRIBUTES)
	{
		char buffer[MAX_PATH + 30];

		sprintf_s(buffer, MAX_PATH + 30, 
			szErrorInfo,
			smbpath, LOCALNAME_DISK, szInvalidPath);
		MessageBox(g_selDlg, buffer, szTitle, MB_OK | MB_ICONERROR);
		return false; /* Path is invalid */
	}

	GetPrivateProfileString(initag, TEXT("SMBUser"), NULL, smbuser, sizeof(smbuser), g_iniFile);
	GetPrivateProfileString(initag, TEXT("SMBPass"), NULL, smbpass, sizeof(smbpass), g_iniFile);

	/* Check to see if $SMBPATH/setup.exe exists */

	if (smbuser == "" && smbpass == "")
	{
		TCHAR setupPath[MAX_PATH];
		strcpy_s(setupPath, sizeof(setupPath), smbpath);
		strcat_s(setupPath, sizeof(setupPath), "\\setup.exe");
		if (GetFileAttributes(setupPath) == INVALID_FILE_ATTRIBUTES)
			return false; /* no $SMBPATH/setup.exe */
	}

	NETRESOURCE net = { 0 };
	net.dwType = RESOURCETYPE_DISK;
	net.dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
	net.lpLocalName = LOCALNAME_DISK;
	net.lpRemoteName = smbpath;

	/* WKNetAddConnection3 (lol, we're on 3?) provides a modal dialog  *
	 * for our user to enter in alternative credentials if required    */

	DWORD result = WNetAddConnection3(g_selDlg, &net, 
		NULL, NULL, CONNECT_INTERACTIVE);

	if (result == NO_ERROR)
		return true;

	/* TODO: Capture Error and deliver to user */
	char buffer[MAX_PATH + 30];

	sprintf_s(buffer, MAX_PATH + 30, "Unable to mount SMB path %s on drive %s.", smbpath, LOCALNAME_DISK);
	MessageBox(g_selDlg, buffer, TEXT("SMB Mount Error."), MB_OK | MB_ICONERROR);
	return false;
}

bool ExecSetup()
{
	/* launch a the setup executable */
	char path[MAX_PATH + 2] = LOCALNAME_DISK;
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	si.cb = sizeof(si);

	strcat_s(path, sizeof(path), TEXT("\\setup.exe"));
	if (!CreateProcess(path,
		NULL,
		NULL,
		NULL,
		false,
		CREATE_DEFAULT_ERROR_MODE,
		NULL,
		NULL,
		&si,
		&pi))
	{
		MessageBox(g_selDlg,  TEXT("Unable to launch setup.exe."), 
			TEXT("CreateProcess failed."), MB_OK | MB_ICONERROR);
		return false;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
}


bool ExecCmd()
{
	/* launch a command prompt */
	char path[MAX_PATH + 2];
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	si.cb = sizeof(si);

	GetSystemDirectory(path, sizeof(path));
	strcat_s(path, sizeof(path), TEXT("\\cmd.exe"));
	if (!CreateProcess(path,
		NULL,
		NULL,
		NULL,
		false,
		CREATE_DEFAULT_ERROR_MODE,
		NULL,
		NULL,
		&si,
		&pi))
	{
		MessageBox(g_selDlg, TEXT("Unable to launch command prompt."), 
			TEXT("CreateProcess failed."), MB_OK | MB_ICONERROR);
		return false;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
}
