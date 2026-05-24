#include "win32_fado.h"

internal void ToggleFullscreen(HWND Window)
{
	// >Note: Copied code from internet
	DWORD style = GetWindowLongW(Window, GWL_STYLE);
	if (style & WS_OVERLAPPEDWINDOW)
	{
		MONITORINFO monitorInfo = { sizeof(monitorInfo) };
		if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
			GetMonitorInfoW(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
		{
			SetWindowLongW(Window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(Window, HWND_TOP,
				monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
	else
	{
		SetWindowLongW(Window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(Window, &GlobalWindowPosition);
		SetWindowPos(Window, 0, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

//////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////
internal void Win32HandleKeyboardInput(Win32System* system, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Win32Input* input = &system->input;
	switch (msg)
	{
		case WM_KEYDOWN:
		{
			input->keys[(u32)wParam] = true;
		} break;

		case WM_KEYUP:
		{
			input->keys[(u32)wParam] = false;
		} break;
	}

	if (input->keys[VK_ESCAPE])
	{
		GlobalRunning = false;
	}

	if ((input->keys[VK_RETURN]))
	{
		ToggleFullscreen(system->window);
	}
}

LRESULT CALLBACK Win32MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	switch (message)
	{
		// Check if the window is being destroyed.
		case WM_DESTROY:
		case WM_CLOSE:
		{
			GlobalRunning = false;
		} break;

		// Explicitly call Render when the window is resized or dragged.
		case WM_PAINT:
		{
			Render(&GlobalApplicationHandle->world);
		}break;

		// Check if a key has been pressed on the keyboard.
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Win32HandleKeyboardInput(GlobalApplicationHandle, message, wParam, lParam);
		} break;

		// All other messages pass to the message handler in the system class.
		default:
		{
			result = DefWindowProcW(window, message, wParam, lParam);
		} break;
	}
	return result;
}

//////////////////////////////////////////////////
// Win32System
//////////////////////////////////////////////////
internal bool32 Win32Initialize(Win32System* win32System)
{
	bool32 result = true;

	GlobalApplicationHandle = win32System;

	// Get the instance of this application.
	win32System->instance = GetModuleHandleW(0);

	// Give the application a name.
	LPCWSTR applicationName = L"Fado Engine";

	// Setup the windows class with default settings.
	WNDCLASSEX windowClass = {};
	windowClass.style = 0; // These make the rendereing flash because of the redraw when we resize. //CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = Win32MainWindowCallback;
	windowClass.hInstance = win32System->instance;
	//windowClass.hIcon = LoadIconW(NULL, IDI_WINLOGO);
	windowClass.hCursor = LoadCursorW(NULL, IDC_CROSS);
	windowClass.hbrBackground = 0;	// Skips the automatic background erase.
	windowClass.lpszClassName = applicationName;
	windowClass.cbSize = sizeof(WNDCLASSEX);

	// Register the window class.
	RegisterClassExW(&windowClass);

	// Create the window.
	i32 screenWidth = 1280;
	i32 screenHeight = 720;
	win32System->window = CreateWindowExW(0,
		applicationName,
		applicationName,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, screenWidth, screenHeight,
		0, 0, win32System->instance, 0);

	// Initialize Dx11.
	result = Initialize(&win32System->world, screenWidth, screenHeight, VSYNC_ENABLED, win32System->window, FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR);

	return result;
}

//////////////////////////////////////////////////
// Main
//////////////////////////////////////////////////
int WINAPI wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PWSTR pCmdLine,
	int nCmdShow)
{
	Win32System win32System = {};
	Win32Initialize(&win32System);
	
	// Game loop.
	MSG message;
	while (GlobalRunning)
	{
		// Handle windows messages.
		if (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}

		Render(&win32System.world);
	}

	return 0;
}