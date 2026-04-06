#include "Win32Fado.h"

///////////////////////////////
// Input //
///////////////////////////////
internal bool32 Win32IsKeyDown(Win32Input* input, uint32 key)
{
	bool32 isDown = input->Keys[key];

	return isDown;
}

internal void Win32HandleKeyboardInput(Win32Input* input, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_KEYDOWN:
		{
			input->Keys[(uint32)wParam] = true;
		} break;

		case WM_KEYUP:
		{
			input->Keys[(uint32)wParam] = false;
		} break;
	}

}

internal LRESULT CALLBACK Win32MainWindowCallback(Win32System* win32System, HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	switch (msg)
	{
		// Check if a key has been pressed on the keyboard.
		case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Win32HandleKeyboardInput(&win32System->Input, msg, wParam, lParam);
        } break;

		default:
		{
			result = DefWindowProc(window, msg, wParam, lParam);
		} break;
	}

	return result;
}

LRESULT CALLBACK Win32WindowProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
{
	LRESULT result = 0;
	switch (umessage)
	{
		// Check if the window is being destroyed.
		case WM_DESTROY:
		case WM_CLOSE:
		{
			PostQuitMessage(0);
		} break;

		// All other messages pass to the message handler in the system class.
		default:
		{
			result = Win32MainWindowCallback(ApplicationHandle, hwnd, umessage, wparam, lparam);
		} break;
	}
	return result;
}
////////////////////////////////////////////////////////////////////

internal void Win32SystemInitializeWindows(Win32System* win32System, int& screenWidth, int& screenHeight)
{
	// >> Pretty much copied this code:
	//

	WNDCLASSEX wc;
	DEVMODE dmScreenSettings;
	int32 posX, posY;

	ApplicationHandle = win32System;

	// Get the instance of this application.
	win32System->Instance = GetModuleHandle(0);

	// Give the application a name.
	win32System->ApplicationName = L"Fado Engine";

	// Setup the windows class with default settings.
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = Win32WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = win32System->Instance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = win32System->ApplicationName;
	wc.cbSize = sizeof(WNDCLASSEX);

	// Register the window class.
	RegisterClassEx(&wc);

	// Determine the resolution of the clients desktop screen.
	screenWidth = GetSystemMetrics(SM_CXSCREEN);
	screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// Setup the screen settings depending on whether it is running in full screen or in windowed mode.
	if (FULL_SCREEN)
	{
		// If full screen set the screen to maximum size of the users desktop and 32bit.
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth = (unsigned long)screenWidth;
		dmScreenSettings.dmPelsHeight = (unsigned long)screenHeight;
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Change the display settings to full screen.
		ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);

		// Set the position of the window to the top left corner.
		posX = posY = 0;
	}
	else
	{
		// If windowed then set it to 800x600 resolution.
		screenWidth = 800;
		screenHeight = 600;

		// Place the window in the middle of the screen.
		posX = (GetSystemMetrics(SM_CXSCREEN) - screenWidth) / 2;
		posY = (GetSystemMetrics(SM_CYSCREEN) - screenHeight) / 2;
	}

	// Create the window with the screen settings and get the handle to it.
	win32System->Window = CreateWindowEx(WS_EX_APPWINDOW, win32System->ApplicationName, win32System->ApplicationName,
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP,
		posX, posY, screenWidth, screenHeight, NULL, NULL, win32System->Instance, NULL);

	// Bring the window up on the screen and set it as main focus.
	ShowWindow(win32System->Window, SW_SHOW);
	SetForegroundWindow(win32System->Window);
	SetFocus(win32System->Window);

	// Hide the mouse cursor.
	ShowCursor(false);

	return;
}

internal void Win32SystemShutdownWindows(Win32System* win32System)
{
	// Show the mouse cursor.
	ShowCursor(true);

	// Fix the display settings if leaving full screen mode.
	if (FULL_SCREEN)
	{
		ChangeDisplaySettings(NULL, 0);
	}

	// Remove the window.
	DestroyWindow(win32System->Window);
	win32System->Window = 0;

	// Remove the application instance.
	UnregisterClass(win32System->ApplicationName, win32System->Instance);
	win32System->Instance = 0;

	// Release the pointer to this class.
	ApplicationHandle = nullptr;

	return;
}

internal bool32 Win32Initialize(Win32System* win32System)
{
	bool32 result = true;

	int32 screenWidth = 0;
	int32 screenHeight = 0;
	Win32SystemInitializeWindows(win32System, screenWidth, screenHeight);

	return result;
}

internal bool32 Win32Frame(Win32System* win32System)
{
	bool32 result = false;

	// // Check if the user pressed escape and wants to exit the application.
	if (Win32IsKeyDown(&win32System->Input, VK_ESCAPE))
	{
		return result;
	}

	// Do the frame processing for the application class object.
	result = true;

	return result;
}

internal void Win32Run(Win32System* win32System)
{
	MSG msg;
	bool32 running = true;
	bool32 frameResult = false;

	ZeroMemory(&msg, sizeof(MSG));


	// Loop until there is a quit message from the window or the user.
	while (running)
	{
		// Handle windows messages.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if ((msg.message == WM_QUIT) || (!Win32Frame(win32System)))
		{
			running = false;
		}
	}
}

/********************************************************/
/* Main */
/********************************************************/
int WINAPI wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PWSTR pCmdLine,
	int nCmdShow)
{
	Win32System System = {};
	Win32Initialize(&System);
	Win32Run(&System);

	return 0;
}