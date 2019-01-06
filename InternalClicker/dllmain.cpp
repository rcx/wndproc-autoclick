// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "console.h"

#define dank_perror(msg) { \
	LPCSTR _errorText = NULL; \
	FormatMessageA( \
		FORMAT_MESSAGE_FROM_SYSTEM \
		| FORMAT_MESSAGE_ALLOCATE_BUFFER \
		| FORMAT_MESSAGE_IGNORE_INSERTS, \
		NULL, \
		GetLastError(), \
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
		(LPSTR) &_errorText, \
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

DWORD CALLBACK ThrEject(LPVOID hModule)
{
	Sleep(100);
	FreeLibraryAndExitThread((HMODULE)hModule, 0);
	return 0;
}

CRITICAL_SECTION criticalSection; // protects the following variables
HWND hWnd = NULL;
WNDPROC wndProc = NULL;
LPARAM lParam;
LONG trampolineDone = 0;

BYTE* stolenBytes;
int wndProcHooked = 0;

// Assembles a far jump to dest.
void AssembleTrampoline(BYTE* dst, uintptr_t target)
{
#ifdef _WIN64
	BYTE trampoline[] = {
		0x68, 0x00, 0x00, 0x00, 0x00, // push qword XXXXXXXX
		0xC7, 0x44, 0x24, 0x04, 0x00, 0x00, 0x00, 0x00, // mov dword ptr [rsp+4], XXXXXXXX
		0xC3 // ret
	};
#define TRAMPOLINE_SIZE 14
#else
	BYTE trampoline[] = {
		0x68, 0x00, 0x00, 0x00, 0x00, // push dword XXXXXXXX
		0xC3 // ret
	}
#define TRAMPOLINE_SIZE 6
#endif
	
	DWORD dwOldProtect;
	VirtualProtect(dst, TRAMPOLINE_SIZE, PAGE_EXECUTE_READWRITE, &dwOldProtect);
	
	memcpy(dst, trampoline, sizeof(trampoline));
#ifdef _WIN64
	DWORD64 jmpTarget = (DWORD64)target;
	*(DWORD*)(dst + 1) = (DWORD)(jmpTarget & 0xFFFFFFFF);
	*(DWORD*)(dst + 9) = (DWORD)((jmpTarget >> 32) & 0xFFFFFFFF);
#else
	
	*(DWORD*)(dst + 1) = (DWORD)target;
#endif
	
	DWORD trash;
	VirtualProtect(dst, TRAMPOLINE_SIZE, dwOldProtect, &trash);
}

void RestoreHookedBytes()
{
	DWORD dwOldProtect;
	VirtualProtect(wndProc, TRAMPOLINE_SIZE, PAGE_EXECUTE_READWRITE, &dwOldProtect);
	memcpy((LPVOID)wndProc, stolenBytes, TRAMPOLINE_SIZE);
	DWORD trash;
	VirtualProtect(wndProc, TRAMPOLINE_SIZE, dwOldProtect, &trash);
}

extern "C" {
	CONTEXT savedContext;
	void Trampoline64();
}

__declspec(noreturn) extern "C" void HookCallback(CONTEXT* savedContext)
{
	EnterCriticalSection(&criticalSection);
	printf("HELLO FROM WNDPROC HOOK!!!!!\n");
	printf("hWnd = %x, wndProc = %p, lParam = %x\n", hWnd, wndProc, lParam);

	if (wndProc)
	{
		// unhook
		RestoreHookedBytes();
		
		for (int i = 0; i < 1; i++)
		{
			wndProc(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
			wndProc(hWnd, WM_LBUTTONUP, 0, lParam);
		}

		// restore hook
		AssembleTrampoline((BYTE*)wndProc, (uintptr_t)Trampoline64);
	}
	else
	{
		printf("WTF!\n");
	}
	LeaveCriticalSection(&criticalSection);

	printf("Wndproc done!\n");

	savedContext->Rip = (DWORD64)stolenBytes;
	RtlRestoreContext(savedContext, NULL);
}

DWORD dwThreadId = 0;
HANDLE hThread = NULL;

void DisableAutoclick()
{
	EnterCriticalSection(&criticalSection);
	printf("Disabling\n");
	if (hThread)
	{
		CloseHandle(hThread);
	}
	if (wndProcHooked)
	{
		RestoreHookedBytes();
		wndProcHooked = 0;
	}
	wndProc = NULL;
	hWnd = NULL;
	dwThreadId = 0;
	LeaveCriticalSection(&criticalSection);
	printf("Disabled\n");
}

void EnableAutoclick()
{
	EnterCriticalSection(&criticalSection);
	POINT pt;
	GetCursorPos(&pt);
	printf("Cursor at (%d,%d) screen\n", pt.x, pt.y);
	hWnd = WindowFromPoint(pt);
	printf("hWnd = 0x%p\n", hWnd);

	dwThreadId = GetWindowThreadProcessId(hWnd, NULL);
	if (!dwThreadId)
	{
		dank_perror("GetWindowThreadProcessId");
		goto fail;
	}
	printf("My ThreadID = %d\n", GetCurrentThreadId());
	printf("ThreadID = %d = 0x%x\n", dwThreadId, dwThreadId);
	if (dwThreadId == GetCurrentThreadId())
	{
		printf("Don't suspend myself!!!\n");
		goto fail;
	}

	wndProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
	printf("wndProc = 0x%p\n", wndProc);
	ScreenToClient(hWnd, &pt);
	printf("Cursor at (%d,%d) client\n", pt.x, pt.y);
	lParam = MAKELPARAM(pt.x, pt.y);

	hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, dwThreadId);
	if (!hThread)
	{
		dank_perror("OpenThread");
		goto fail;
	}
	printf("hThread = 0x%x\n", hThread);
	LeaveCriticalSection(&criticalSection);
	return;

fail:
	printf("Failed to setup AutoClick\n");
	LeaveCriticalSection(&criticalSection);
	DisableAutoclick();
}

int IsAutoclickEnabled()
{
	return !!wndProc;
}

void HookWndProc()
{
	printf("Hooking WndProc\n");

	// Suspend before hooking
	//DWORD dwSuspendCnt = 0;
	//dwSuspendCnt = SuspendThread(hThread);
	//if (dwSuspendCnt == (DWORD)-1)
	//{
	//	dank_perror("SuspendThread");
	//	goto fail;
	//}
	//printf("SuspendCount = %d\n", dwSuspendCnt);

	// Setup hook
	memcpy(stolenBytes, (LPVOID)wndProc, TRAMPOLINE_SIZE + 3);
	AssembleTrampoline(stolenBytes + TRAMPOLINE_SIZE + 3, (uintptr_t)wndProc + TRAMPOLINE_SIZE + 2); // jump back to WndProc
	AssembleTrampoline((BYTE*)wndProc, (uintptr_t)Trampoline64);
	wndProcHooked = 1;

	// Resume
	//dwSuspendCnt = ResumeThread(hThread);
	//if (dwSuspendCnt == (DWORD)-1)
	//{
	//	dank_perror("ResumeThread");
	//	goto fail;
	//}

	printf("WndProc successfully hooked -> 0x%p\n", (uintptr_t)Trampoline64);
	return;

fail:
	printf("Failed to hook WndProc\n");
	DisableAutoclick();
}

DWORD CALLBACK ThrMain(LPVOID hModule)
{
	CreateConsoleEx(NULL, _T("WndProcClicker"));
	ClearConsole();
	printf("AutoClick loaded\n");

	BYTE _keyStates[256] = { 0 };
	BYTE _keyState;
#define _CheckKey(vk) (_keyState = !!GetAsyncKeyState(vk), _keyState != _keyStates[vk] && (_keyStates[vk] = _keyState))
	while (true)
	{
		if (_CheckKey(VK_F2))
		{
			if (IsAutoclickEnabled())
			{
				DisableAutoclick();
			}
			else
			{
				EnableAutoclick();
			}
		}
		if (_CheckKey(VK_F6))
		{
			DisableAutoclick();
			break;
		}
		
		if (IsAutoclickEnabled())
		{
			if (!wndProcHooked)
			{
				HookWndProc();
			}
		}
		else
		{
			Sleep(10);
		}
	}

	printf("Quitting...\n");
	CloseConsole();
	CreateThread(NULL, 0, ThrEject, hModule, NULL, NULL);
	return ERROR_SUCCESS;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		stolenBytes = (BYTE*)VirtualAlloc(NULL, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		InitializeCriticalSection(&criticalSection);
		CreateThread(NULL, NULL, ThrMain, hModule, NULL, NULL);
		break;
	case DLL_PROCESS_DETACH:
		DeleteCriticalSection(&criticalSection);
		VirtualFree(stolenBytes, 0, MEM_RELEASE);
		break;
	}
	return TRUE;
}

