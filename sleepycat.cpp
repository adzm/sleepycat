// sleepycat.cpp : Defines the entry point for the application.
//

// icon attribution:
// modified from box cat by Denis Sazhin from the Noun Project

#include "stdafx.h"
#include "sleepycat.h"

//

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

//

extern "C" IMAGE_DOS_HEADER __ImageBase;
#define HINST_THIS ((HINSTANCE)&__ImageBase)

#define WM_SLEEPYNOTIFYICON (WM_APP + 1)

LRESULT CALLBACK SleepyWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

//

HWND CreateSleepyMessageWindow()
{
	WNDCLASSEX wx = { sizeof(wx) };
	wx.lpfnWndProc = SleepyWndProc;
	wx.lpszClassName = L"sleepycat";
	wx.hInstance = HINST_THIS;
	RegisterClassEx(&wx);

	return CreateWindowEx(0
		, L"sleepycat"
		, L"sleepycat"
		, WS_POPUP
		, 0, 0, 0, 0
		, nullptr, nullptr
		, HINST_THIS, nullptr);
}

void AddTaskbarIcon(HWND hwnd)
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.uVersion = NOTIFYICON_VERSION_4;
	nid.hWnd = hwnd;
	nid.uID = 42;

	nid.uCallbackMessage = WM_SLEEPYNOTIFYICON;
	wcscpy_s(nid.szTip, L"sleepycat");
	LoadIconMetric(HINST_THIS, MAKEINTRESOURCE(IDI_SLEEPYCAT), LIM_SMALL, &(nid.hIcon));
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_MESSAGE;

	Shell_NotifyIcon(NIM_ADD, &nid);
	
	// set version 4
	nid.uVersion = NOTIFYICON_VERSION_4;
	Shell_NotifyIcon(NIM_SETVERSION, &nid);

	DestroyIcon(nid.hIcon);
}

void UpdateTaskbarIcon(HWND hwnd)
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = hwnd;
	nid.uID = 42;

	LoadIconMetric(HINST_THIS, MAKEINTRESOURCE(IDI_SLEEPYCAT), LIM_SMALL, &(nid.hIcon));
	nid.uFlags = NIF_ICON;

	Shell_NotifyIcon(NIM_MODIFY, &nid);

	DestroyIcon(nid.hIcon);
}

void RemoveTaskbarIcon(HWND hwnd)
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = hwnd;
	nid.uID = 42;

	Shell_NotifyIcon(NIM_DELETE, &nid);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	HWND hwnd = CreateSleepyMessageWindow();

	MSG msg = { 0 };
	BOOL bRet = FALSE;
	while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			DWORD err = GetLastError();
			return err ? err : E_FAIL;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

    return (int) msg.wParam;
}

void ShowContextMenu(HWND hwnd, POINT pt)
{
	HMENU hMenu = LoadMenu(HINST_THIS, MAKEINTRESOURCE(IDC_SLEEPYCAT));
	if (hMenu)
	{
		HMENU hSubMenu = GetSubMenu(hMenu, 0);
		if (hSubMenu)
		{
			// our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
			SetForegroundWindow(hwnd);

			// respect menu drop alignment
			UINT uFlags = TPM_RIGHTBUTTON;
			if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
			{
				uFlags |= TPM_RIGHTALIGN;
			}

			TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
			PostMessage(hwnd, WM_NULL, 0, 0);
		}
		DestroyMenu(hMenu);
	}
}

bool IsSessionLocked()
{
	LPTSTR ppBuffer = NULL;
	DWORD dwBytesReturned = 0;
	LONG sessionFlags = WTS_SESSIONSTATE_UNKNOWN; // until we know otherwise. Prevents a false positive since WTS_SESSIONSTATE_LOCK == 0

	if (!WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfoEx, &ppBuffer, &dwBytesReturned)) {
		return false;
	}

	if (dwBytesReturned > 0)
	{
		auto* pInfo = (WTSINFOEXW*)ppBuffer;
		if (pInfo->Level == 1)
		{
			sessionFlags = pInfo->Data.WTSInfoExLevel1.SessionFlags;
		}
	}
	WTSFreeMemory(ppBuffer);
	ppBuffer = NULL;

	return (sessionFlags == WTS_SESSIONSTATE_LOCK);
}

bool SleepIsForTheWeak()
{
	DWORD ticks = GetTickCount();

	LASTINPUTINFO lii = { sizeof(lii) };
	if (GetLastInputInfo(&lii)) {
		DWORD elapsed = ticks - lii.dwTime;

		if (elapsed < (30 * 1000)) {
			return false;
		}
	}

	bool isLocked = IsSessionLocked();
	if (isLocked) {
		// if the session is locked, don't send input
		return false;
	}
	
	// SendInput will appear to succeed even if input is blocked due to UIPI
	INPUT input = { INPUT_MOUSE };
	input.mi.dwFlags = MOUSEEVENTF_MOVE;
	input.mi.time = ticks;
	UINT ret = SendInput(1, &input, sizeof(input));
	if (ret != 1) {
		// SendInput failed, but nothing I can do
		return false;
	}
	else {
		return true;
	}
}

void NeverSleepAgain(HWND hwnd)
{
	SetTimer(hwnd, 1337, 42 * 1000, nullptr);
}

LRESULT CALLBACK SleepyWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static UINT s_uTaskbarRestart = -1;

	switch (message) {
	case WM_CREATE:
		s_uTaskbarRestart = RegisterWindowMessage(L"TaskbarCreated");
		AddTaskbarIcon(hwnd);

		SleepIsForTheWeak();
		NeverSleepAgain(hwnd);
		break;
	case WM_DESTROY:
		RemoveTaskbarIcon(hwnd);
		PostQuitMessage(0);
		break;
	case WM_TIMER:
		if (wParam == 1337) {
			SleepIsForTheWeak();
			NeverSleepAgain(hwnd);
			return 0;
		}
		break;
	case WM_COMMAND:
		{
			int cmd = LOWORD(wParam);
			switch (cmd) {
			case IDM_EXIT:
				DestroyWindow(hwnd);
				return 0;
				break;
			case IDM_ABOUT:
				DialogBox(HINST_THIS, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, AboutDlgProc);
				return 0;
				break;
			default:
				return DefWindowProc(hwnd, message, wParam, lParam);
			}
		}
		break;
	case WM_SLEEPYNOTIFYICON:
		switch (LOWORD(lParam)) {
			case NIN_SELECT:
				// selected with keyboard or clicked with mouse
				break;
			case WM_CONTEXTMENU:
				{
					POINT pt = { GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam) };
					// of course the wParam seems to be invalid sometimes when monitors/dpis change.
					// let's see if removing and re-adding the icon avoids this issue.
					// Otherwise GetCursorPos works great except for the keyboard stuff!
					//::GetCursorPos(&pt);
					ShowContextMenu(hwnd, pt);
				}
				break;
		}
		break;
	case WM_DPICHANGED:
		// explorer / shell bug apparently. when dpi changes we need to re-add the icon
		// otherwise we get incorrect coordinates in the notifications
		RemoveTaskbarIcon(hwnd);
		AddTaskbarIcon(hwnd);
		break;
	default:
		if (message == s_uTaskbarRestart) {
			RemoveTaskbarIcon(hwnd);
			AddTaskbarIcon(hwnd);
		}
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hwnd, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
