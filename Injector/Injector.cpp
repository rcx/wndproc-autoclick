// Injector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


DWORD FindPid(const char* szProcName)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (stricmp(entry.szExeFile, szProcName) == 0)
			{
				return entry.th32ProcessID;
			}
		}
	}
	return NULL;
	CloseHandle(snapshot);
}

#define dank_perror(msg) { \
	LPCSTR _errorText = NULL; \
	FormatMessageA( \
		FORMAT_MESSAGE_FROM_SYSTEM \
		| FORMAT_MESSAGE_ALLOCATE_BUFFER \
		| FORMAT_MESSAGE_IGNORE_INSERTS, \
		NULL, \
		GetLastError(), \
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
		(LPSTR)&_errorText, \
		0, \
		NULL); \
	if (_errorText) \
	{ \
		printf(msg " failed: %s\n", _errorText); \
		LocalFree((HLOCAL) _errorText); \
		_errorText = NULL; \
	} \
	else \
	{ \
		printf(msg " failed\n"); \
	} \
}

int main()
{
	printf("Waiting for target...\n");
	while (!GetAsyncKeyState(VK_F2))
	{
		Sleep(10);
	}
	POINT pt;
	GetCursorPos(&pt);
	HWND hWnd = WindowFromPoint(pt);
	printf("hWnd = 0x%p\n", hWnd);
	DWORD dwPid;
	GetWindowThreadProcessId(hWnd, &dwPid);

	//while (dwPid = FindPid("win32calc.exe"), !dwPid)
	//{
	//	Sleep(100);
	//}
	printf("PID = %d = 0x%x\n", dwPid, dwPid);
	HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD
		| PROCESS_QUERY_INFORMATION
		| PROCESS_VM_OPERATION
		| PROCESS_VM_WRITE
		| PROCESS_VM_READ, FALSE, dwPid);
	if (!hProcess)
	{
		dank_perror("OpenProcess");
		goto exit;
	}

	LPVOID pMem = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!pMem)
	{
		dank_perror("VirtualAllocEx");
		goto exit;
	}
	printf("pMem = 0x%p\n", pMem);
	
	char szDllFilename[MAX_PATH];
	GetCurrentDirectory(sizeof(szDllFilename) - 1, szDllFilename);
	strncat(szDllFilename, "\\InternalClicker.dll", max(0, MAX_PATH - strlen(szDllFilename) - 1));
	BOOL bSuccess = WriteProcessMemory(hProcess, pMem, szDllFilename, strlen(szDllFilename) + 1, NULL);
	if (!bSuccess)
	{
		dank_perror("WriteProcessMemory");
		goto exit;
	}
	printf("Wrote %s\n", szDllFilename);

	LPTHREAD_START_ROUTINE pLoadLibraryA = (LPTHREAD_START_ROUTINE) GetProcAddress(GetModuleHandle("kernel32"), "LoadLibraryA");
	printf("LoadLibraryA = 0x%p\n", pLoadLibraryA);
	DWORD dwThreadId;
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, NULL, pLoadLibraryA, pMem, 0, &dwThreadId);
	if (!hThread)
	{
		dank_perror("CreateRemoteThread");
		goto exit;
	}
	printf("ThreadID = %d\n", dwThreadId);
	CloseHandle(hThread);
	printf("Success\n");

exit:
	if (hProcess)
	{
		CloseHandle(hProcess);
	}
	_getch();
    return 0;
}

