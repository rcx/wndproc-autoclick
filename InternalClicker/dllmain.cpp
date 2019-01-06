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

HWND hWnd = NULL;
WNDPROC wndProc = NULL;
LPARAM lParam;
LONG trampolineDone = 0;

__declspec(noreturn) void HijackTrampoline()
{
	printf("HELLO FROM HIJACKED THREAD!!!!!\n");
	printf("hWnd = %x, wndProc = %p, lParam = %x\n", hWnd, wndProc, lParam);
	for (int i = 0; i < 1; i++)
	{
		wndProc(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
		wndProc(hWnd, WM_LBUTTONUP, 0, lParam);
	}
	printf("Wndproc is done!\n");
	InterlockedAdd(&trampolineDone, 1);
	for (;;);
}

#ifdef NO_HIJACK
DWORD CALLBACK ThrGoat(LPVOID hModule)
{
	for (;;)
	{
		Sleep(10);
	}
}
#endif


DWORD CALLBACK ThrMain(LPVOID hModule)
{
	CreateConsoleEx(NULL, _T("WndProcClicker"));
	ClearConsole();

#ifdef NO_HIJACK
	DWORD dwThreadIdGoat = NULL;
	HANDLE hGoat = CreateThread(NULL, 0, ThrGoat, NULL, 0, &dwThreadIdGoat);
#endif

	printf("AutoClick loaded\n");

	POINT pt;
	BYTE keyStates[256];
	LPVOID pHijackStack = NULL;
#define HIJACK_STACK_SIZE 0x10000
	pHijackStack = VirtualAlloc(NULL, HIJACK_STACK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!pHijackStack)
	{
		dank_perror("VirtualAlloc");
	}
	printf("Hijack stack = 0x%p\n", pHijackStack);

	memset(keyStates, 0, sizeof(keyStates));
	while (true)
	{
		BYTE state;
		if (state = !!GetAsyncKeyState(VK_F2), state != keyStates[VK_F2] && (keyStates[VK_F2] = state))
		{
			if (wndProc)
			{
				wndProc = NULL;
				hWnd = NULL;
				printf("Disabled\n");
			}
			else
			{
				GetCursorPos(&pt);
				printf("Cursor at (%d,%d) screen\n", pt.x, pt.y);
				hWnd = WindowFromPoint(pt);
				printf("hWnd = 0x%p\n", hWnd);
				wndProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
				printf("wndProc = 0x%p\n", wndProc);
				ScreenToClient(hWnd, &pt);
				printf("Cursor at (%d,%d) client\n", pt.x, pt.y);
			}
		}
		if (state = !!GetAsyncKeyState(VK_F6), state != keyStates[VK_F6] && (keyStates[VK_F6] = state))
		{
			break;
		}
		
		if (wndProc && hWnd)
		{
			lParam = MAKELPARAM(pt.x, pt.y);
			memset(pHijackStack, 0, HIJACK_STACK_SIZE);

			InterlockedAnd(&trampolineDone, 0);
			DWORD dwThreadId = GetWindowThreadProcessId(hWnd, NULL);
			if (!dwThreadId)
			{
				dank_perror("GetWindowThreadProcessId");
				goto fail;
			}
#ifdef NO_HIJACK
			dwThreadId = dwThreadIdGoat;
#endif
			printf("My ThreadID = %d\n", GetCurrentThreadId());
			printf("ThreadID = %d = 0x%x\n", dwThreadId, dwThreadId);
			if (dwThreadId == GetCurrentThreadId())
			{
				printf("Don't suspend myself!!!\n");
				goto fail;
			}
			
			HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, dwThreadId);
			if (!hThread)
			{
				dank_perror("OpenThread");
				goto fail;
			}
			printf("hThread = 0x%x\n", hThread);
			
			DWORD dwSuspendCnt = 0;

			dwSuspendCnt = SuspendThread(hThread);
			if (dwSuspendCnt == (DWORD)-1)
			{
				dank_perror("SuspendThread (1)");
				goto fail;
			}
			printf("SuspendCount = %d\n", dwSuspendCnt);

			CONTEXT savedContext;
			SecureZeroMemory(&savedContext, sizeof(CONTEXT));
			savedContext.ContextFlags = CONTEXT_FULL;
			BOOL success;
			success = GetThreadContext(hThread, &savedContext);
			if (!success)
			{
				dank_perror("GetThreadContext");
				goto fail;
			}
			
			CONTEXT threadContext;
			memcpy(&threadContext, &savedContext, sizeof(CONTEXT));
			//memset(&threadContext.Dr0, 0, &threadContext.Rip - &threadContext.Dr0);

			int gucci = 0;
#ifdef _WIN64
			uintptr_t oldIp = savedContext.Rip;
			gucci = *(BYTE*)oldIp == 0xC3 && *(WORD*)(oldIp - 2) == 0x050f /*syscall*/ && oldIp > (uintptr_t)GetModuleHandle(L"win32u");
			DWORD syscall = *(DWORD*)(oldIp - 0x10);
			printf("Syscall = %x\n", syscall);
			if (syscall == 0x1049 || syscall == 0x1471 || syscall == 8 || syscall == 0x1004) // NtGdiOffsetClipRgn, ????, NtWorkerFactoryWorkerReady
				gucci = 0;
#else
			gucci = 1;// *(BYTE*)oldIp == 0xC3;
#endif
			if (gucci)
			{
				printf("Gucci\n");
			}
			else
			{
				printf("not gucci\n");
				dwSuspendCnt = ResumeThread(hThread);
				if (dwSuspendCnt == (DWORD)-1)
				{
					dank_perror("ResumeThread (3)");
					goto fail;
				}
				goto fail;
			}

#ifdef _WIN64
			printf("Old IP = 0x%p\n", savedContext.Rip);
			printf("Old SP = 0x%p\n", savedContext.Rsp);
			printf("Old SP alignment = %x\n", savedContext.Rsp & 0xff);
			threadContext.Rip = (DWORD64) HijackTrampoline;
			threadContext.Rsp = (((DWORD64) pHijackStack + HIJACK_STACK_SIZE/2) & ~0xff) + (savedContext.Rsp & 0xff);
			threadContext.Rbp = threadContext.Rsp;
			printf("New IP = 0x%p\n", threadContext.Rip);
			printf("New SP = 0x%p\n", threadContext.Rsp);
#else
			printf("Old IP = 0x%p\n", savedContext.Eip);
			printf("Old SP = 0x%p\n", savedContext.Esp);
			threadContext.Eip = (DWORD) HijackTrampoline;
			threadContext.Ebp = (DWORD) pHijackStack + HIJACK_STACK_SIZE/2;
			threadContext.Esp = (DWORD) pHijackStack + HIJACK_STACK_SIZE/2;
			printf("New IP = 0x%p\n", threadContext.Eip);
			printf("New SP = 0x%p\n", threadContext.Esp);
#endif
			success = SetThreadContext(hThread, &threadContext);
			if (!success)
			{
				dank_perror("SetThreadContext (1)");
				goto fail;
			}
			dwSuspendCnt = ResumeThread(hThread);
			if (dwSuspendCnt == (DWORD)-1)
			{
				dank_perror("ResumeThread (1)");
				goto fail;
			}

			printf("Hijack in progress\n");
			for (; !trampolineDone;);
			printf("Hijack is done\n");

			dwSuspendCnt = SuspendThread(hThread);
			if (dwSuspendCnt == (DWORD)-1)
			{
				dank_perror("SuspendThread (2)");
				goto fail;
			}
			success = SetThreadContext(hThread, &savedContext);
			if (!success)
			{
				dank_perror("SetThreadContext (2)");
				goto fail;
			}
			dwSuspendCnt = ResumeThread(hThread);
			if (dwSuspendCnt == (DWORD)-1)
			{
				dank_perror("ResumeThread (2)");
				goto fail;
			}

		fail:;
			if (hThread)
			{
				CloseHandle(hThread);
			}
		}
		else
		{
			Sleep(10);
		}
	}

	printf("Quitting...\n");
	if (pHijackStack)
	{
		VirtualFree(pHijackStack, 0, MEM_RELEASE);
	}
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
		CreateThread(NULL, NULL, ThrMain, hModule, NULL, NULL);
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

