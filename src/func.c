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

LPTSTR g_unattendFolder = NULL;
LPTSTR g_unattendXml = NULL;
LPTSTR g_unattendIni = NULL;

bool g_smbMounted = false;

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

bool ParseINIFile()
{
	TCHAR  stringName[MAX_ININAMES];
	TCHAR  retbuf[MAX_ININAMES];
	TCHAR* pNext = NULL;

	if (GetFileAttributes(g_iniFile) == INVALID_FILE_ATTRIBUTES)
		return false;

	/* this is a loop to go through every profile section name *
	 * and install a list box item on the main dialog          */
	
	if (GetPrivateProfileSectionNames((LPTSTR)&retbuf, sizeof(retbuf), g_iniFile) > 0)
	{
		pNext = retbuf;
		GetPrivateProfileString(pNext, TEXT("Name"), NULL, stringName, sizeof(stringName), g_iniFile);

		/* we're not using this atm but reserving 'config' as a header for future use*/
		if(_tcsicmp(TEXT("config"), pNext) != 0)
			InstallListItem(stringName, pNext); /* winsel.c */
		while (*pNext != 0x00)
		{
			pNext = pNext + strlen(pNext) + 1;
			if (*pNext != 0x00 && _tcsicmp("config", pNext) != 0)
			{
				GetPrivateProfileString(pNext, TEXT("Name"), NULL, stringName, sizeof(stringName), g_iniFile);
				InstallListItem(stringName, pNext); /* winsel.c */
			}
		}
		return true;
	}

	return false;
}

bool LoadINIConfigOptions()
{
	TCHAR  retbuf[MAX_ININAMES];
	TCHAR* pNext = NULL;

	if (GetFileAttributes(g_iniFile) == INVALID_FILE_ATTRIBUTES)
		return false;

	/* this is a loop to go through every profile section name *
	 * and install a list box item on the main dialog          */

	if (GetPrivateProfileSectionNames((LPTSTR)&retbuf, sizeof(retbuf), g_iniFile) > 0)
	{
		pNext = retbuf;

		/* we're not using this atm but reserving 'config' as a header for future use*/
		if (_tcsicmp(TEXT("config"), pNext) == 0)
		{
			g_unattendFolder = malloc(MAX_PATH);
			GetPrivateProfileString(pNext, TEXT("UnattendFolder"), NULL, 
				g_unattendFolder, sizeof(g_unattendFolder), g_iniFile);

			return true;
		}
			
		while (*pNext != 0x00)
		{
			pNext = pNext + strlen(pNext) + 1;
			if (*pNext != 0x00 && _tcsicmp(TEXT("config"), pNext) == 0)
			{
				g_unattendFolder = malloc(MAX_PATH);
				GetPrivateProfileString(pNext, TEXT("UnattendFolder"), NULL,
					g_unattendFolder, sizeof(g_unattendFolder), g_iniFile);

				return true;
			}
		}
	}

	return false;
}

bool TempMountSMB(LPCTSTR unc)
{
	TCHAR filterUnc[MAX_PATH];
	strcpy_s(filterUnc, sizeof(filterUnc), unc);
	PathRemoveFileSpec(filterUnc);
	NETRESOURCE net = { 0 };
	net.dwType = RESOURCETYPE_DISK;
	net.dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
	net.lpLocalName = NULL;
	net.lpRemoteName = filterUnc;

	DWORD result = WNetAddConnection3(NULL, &net,
		NULL, NULL, CONNECT_INTERACTIVE);

	if (result == S_OK)
		return true;

	return false;
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

	char inifile[MAX_PATH];
	sprintf_s(inifile, sizeof(inifile), "%s\\winselect.ini", g_lpCmdLine);
	g_iniFile = inifile;

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

	if (g_smbMounted)
	{
		DWORD cancel = WNetCancelConnection2A(LOCALNAME_DISK, 0, true);
	}
	/* WKNetAddConnection3 (lol, we're on 3?) provides a modal dialog  *
	 * for our user to enter in alternative credentials if required    */

	DWORD result = WNetAddConnection3(g_selDlg, &net, 
		NULL, NULL, CONNECT_INTERACTIVE);

	if (result == NO_ERROR)
	{
		g_smbMounted = true;
		return true;
	}

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

	char unattend[MAX_PATH + 11];
	if (g_unattendXml != NULL)
		sprintf_s(unattend, sizeof(unattend), "Z:\\setup.exe /unattend:%s", g_unattendXml);

	strcat_s(path, sizeof(path), TEXT("\\setup.exe"));
	if (!CreateProcess(path,
		unattend,
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

bool WinPEInit()
{
	/* launch a the setup executable */
	char path[MAX_PATH];
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	si.cb = sizeof(si);
	GetSystemDirectory(path, sizeof(path));
	strcat_s(path, sizeof(path), TEXT("\\wpeinit.exe"));
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
		MessageBox(g_selDlg, TEXT("Unable to launch wpeinit.exe."),
			TEXT("CreateProcess failed."), MB_OK | MB_ICONERROR);
		return false;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);
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

bool CheckForMACUnattend()
{
	bool found_mac = false;
	IP_ADAPTER_INFO AdapterInfo[16];
	DWORD dwBufLen = sizeof(AdapterInfo);
	if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == ERROR_SUCCESS)
	{
		PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;
		g_unattendIni = malloc(MAX_PATH);
		g_unattendXml = malloc(MAX_PATH);

		do {
#pragma warning(suppress:4996)
			sprintf(g_unattendIni, "%s\\%s\\%02X-%02X-%02X-%02X-%02X-%02X.ini\0",
				g_lpCmdLine, g_unattendFolder,
				pAdapterInfo->Address[0], pAdapterInfo->Address[1],
				pAdapterInfo->Address[2], pAdapterInfo->Address[3],
				pAdapterInfo->Address[4], pAdapterInfo->Address[5]);

			/* Check to see if we have the .INI file */
			if (GetFileAttributes(g_unattendIni) != INVALID_FILE_ATTRIBUTES)
			{
				/* Now check to see if we have the xml file */
#pragma warning(suppress:4996)
				sprintf(g_unattendXml, "%s\\%s\\%02X-%02X-%02X-%02X-%02X-%02X.xml\0",
					g_lpCmdLine, g_unattendFolder,
					pAdapterInfo->Address[0], pAdapterInfo->Address[1],
					pAdapterInfo->Address[2], pAdapterInfo->Address[3],
					pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
				if (GetFileAttributes(g_unattendXml) != INVALID_FILE_ATTRIBUTES)
				{
					/* we have 2 good files store in global vars and return true */
					return true;
				}
			}
			pAdapterInfo = pAdapterInfo->Next;
		} while (pAdapterInfo);
	}
	free(g_unattendIni);
	free(g_unattendXml);
	g_unattendIni = NULL;
	g_unattendXml = NULL;
	return false;
}

bool ProcessMACBasedUnattendedInstall()
{
	/* Load up the g_unattendIni file, check [Deploy] and get Version= key */
	TCHAR  retbuf[MAX_ININAMES];
	if (GetPrivateProfileSectionNames((LPTSTR)&retbuf, sizeof(retbuf), g_unattendIni) > 0)
	{
		if (_tcsicmp(TEXT("deploy"), retbuf) == 0)
		{
			TCHAR unattendDistro[MAX_PATH];
			GetPrivateProfileString(retbuf, TEXT("Version"), NULL,
				unattendDistro, sizeof(unattendDistro), g_unattendIni);

			/* Load up our winselect.ini and see for a matching distro */
			TCHAR  retbuf[MAX_ININAMES];
			TCHAR* pNext = NULL;
			char path[MAX_PATH];
			TCHAR smbpath[MAX_PATH];
			sprintf_s(path, sizeof(path), "%s\\winselect.ini\0", g_lpCmdLine);
			if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
				return false;

			/* this is a loop to go through every profile section name *
			 * and find the distro that we want                        */

			bool matched = false;
			if (GetPrivateProfileSectionNames((LPTSTR)&retbuf, sizeof(retbuf), path) > 0)
			{
				pNext = retbuf;
				while (*pNext != 0x00)
				{
					if (*pNext != 0x00 && _stricmp(pNext, unattendDistro) == 0)
					{
						GetPrivateProfileString(pNext, TEXT("Path"), NULL,
							smbpath, sizeof(smbpath), path);

						matched = true;
						goto check_match;
					}
					pNext = pNext + strlen(pNext) + 1;
				}
			check_match:
				if (matched)
				{
					/* mount SMB */
					if (!MountSMBDirectory(unattendDistro))
						goto cleanup;

					/* execute the setup with the unattend file */
					if (ExecSetup())
						return true;
				}
			}
		}
	}
cleanup:
	if (g_unattendXml != NULL)
	{
		free(g_unattendXml);
		g_unattendXml = NULL;
	}
	if (g_unattendIni != NULL)
	{
		free(g_unattendIni);
		g_unattendIni = NULL;
	}
	return false;
}