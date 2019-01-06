#include "stdafx.h"
#include <stdio.h>
#include <tchar.h>
#include <consoleapi.h>
#include "console.h"

static bool g_bCreated;
static PHANDLER_ROUTINE ConCtrlHandler;

void CreateConsoleEx(PHANDLER_ROUTINE CtrlHandler, LPTSTR szTitle)
{
	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w+", stdout);
	freopen_s((FILE**)stdin, "CONIN$", "r", stdin);

	if (ConCtrlHandler = CtrlHandler)
		SetConsoleCtrlHandler(ConCtrlHandler, TRUE);

	SetConsoleTitle(szTitle);

	g_bCreated = true;
}

void CreateConsole()
{
	CreateConsoleEx(NULL, _T("Debug Console"));
}

void CloseConsole()
{
	if (!g_bCreated)
		return;

	if (ConCtrlHandler)
		SetConsoleCtrlHandler(ConCtrlHandler, FALSE);

	fclose(stdout);
	fclose(stdin);

	FreeConsole();
	g_bCreated = false;
}

void ClearConsole()
{
	const HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	const COORD topLeft = { 0, 0 };
	CONSOLE_SCREEN_BUFFER_INFO screen;
	GetConsoleScreenBufferInfo(hStdout, &screen);
	const DWORD dwLength = screen.dwSize.X * screen.dwSize.Y;

	DWORD cWritten; // NOTE: as of Win10, this is no longer optional!
	FillConsoleOutputCharacterA(hStdout, ' ', dwLength, topLeft, &cWritten);
	FillConsoleOutputAttribute(hStdout, screen.wAttributes, dwLength, topLeft, &cWritten);
	SetConsoleCursorPosition(hStdout, topLeft);
}
