#include "win32_fado.h"
#include <xinput.h>

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
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void Win32LoadXInput()
{
	HMODULE xInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (xInputLibrary)
	{
		XInputGetState = (x_input_get_state*)GetProcAddress(xInputLibrary, "XInputGetState");
		if (!XInputGetState) { XInputGetState = XInputGetStateStub; }

		XInputSetState = (x_input_set_state*)GetProcAddress(xInputLibrary, "XInputSetState");
		if (!XInputSetState) { XInputSetState = XInputSetStateStub; }
	}
}

internal f32 Win32ProcessXInputStickValue(SHORT value, SHORT deadZoneThreshold)
{
	f32 result = 0;
	if (value > deadZoneThreshold)
	{
		result = (f32)value / 32768.0f;
	}
	else if (value < -deadZoneThreshold)
	{
		result = (f32)value / 32767.0f;
	}
	return result;
}

internal void Win32ProcessButtonState(FGameButtonState* state, bool32 isDown, f32 deltaTime)
{
	state->isDown = isDown;

	if (state->isDown && state->wasDown)
	{
		state->heldLength += deltaTime;
	}
	else
	{
		state->heldLength = 0.0f;
	}
}

internal bool32 Win32IsXInputButtonDown(DWORD XInputButtonState, DWORD ButtonBit)
{
	bool32 result = ((XInputButtonState & ButtonBit) == ButtonBit);
	return result;
}

internal void Win32HandleControllerInput(HWND Window, FGameInput* input)
{
	f32 dt = input->deltaTime;

	POINT mousePoint;
	GetCursorPos(&mousePoint);
	ScreenToClient(Window, &mousePoint);
	input->mouse.x = mousePoint.x;
	input->mouse.y = mousePoint.y;
	input->mouse.z = 0;
	Win32ProcessButtonState(&input->mouse.buttons[0], (GetKeyState(VK_LBUTTON) & (1 << 15)),  dt);
	Win32ProcessButtonState(&input->mouse.buttons[1], (GetKeyState(VK_MBUTTON) & (1 << 15)),  dt);
	Win32ProcessButtonState(&input->mouse.buttons[2], (GetKeyState(VK_RBUTTON) & (1 << 15)),  dt);
	Win32ProcessButtonState(&input->mouse.buttons[3], (GetKeyState(VK_XBUTTON1) & (1 << 15)), dt);
	Win32ProcessButtonState(&input->mouse.buttons[4], (GetKeyState(VK_XBUTTON2) & (1 << 15)), dt);

	DWORD controllerIndex = 0;	// Currently only one controller.
	FGameControllerInput* controller = &input->controller;

	XINPUT_STATE controllerState;
	if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
	{
		// This controller is pluged in.
		controller->isConnected = true;

		XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

		controller->stickAverage.x = Win32ProcessXInputStickValue(
			pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		controller->stickAverage.y = Win32ProcessXInputStickValue(
			pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

		controller->isAnalog = (controller->stickAverage.x != 0 || controller->stickAverage.y != 0);

		f32 threshold = 0.5f;

		// Triggers
		controller->leftTrigger = pad->bLeftTrigger / 255.0f;
		controller->rightTrigger = pad->bRightTrigger / 255.0f;
		Win32ProcessButtonState(&controller->leftTriggerButton, (controller->leftTrigger > threshold), dt);
		Win32ProcessButtonState(&controller->rightTriggerButton, (controller->rightTrigger > threshold), dt);

		// Shoulder Buttons
		Win32ProcessButtonState(&controller->leftShoulder,
			Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER),
			dt);
		Win32ProcessButtonState(&controller->rightShoulder,
			Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER),
			dt);

		// Gamepad Buttons
		Win32ProcessButtonState(&controller->actionDown,  Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_A), dt);
		Win32ProcessButtonState(&controller->actionUp,    Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_Y), dt);
		Win32ProcessButtonState(&controller->actionLeft,  Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_X), dt);
		Win32ProcessButtonState(&controller->actionRight, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_B), dt);

		// DPad Buttons
		Win32ProcessButtonState(&controller->dpadDown,  Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_DPAD_DOWN),  dt);
		Win32ProcessButtonState(&controller->dpadUp,    Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_DPAD_UP),    dt);
		Win32ProcessButtonState(&controller->dpadLeft,  Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_DPAD_LEFT),  dt);
		Win32ProcessButtonState(&controller->dpadRight, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_DPAD_RIGHT), dt);

		// Analog
		Win32ProcessButtonState(&controller->dpadDown, (controller->stickAverage.y > -threshold ? 1 : 0), dt);
		Win32ProcessButtonState(&controller->dpadLeft, (controller->stickAverage.x > -threshold ? 1 : 0), dt);
		// Analog overrides the original dpad state for up and right, so we check it only if the spad wasn't used.
		if (!controller->dpadUp.isDown)
		{
			Win32ProcessButtonState(&controller->dpadUp, (controller->stickAverage.y > threshold ? 1 : 0), dt);
		}
		if (!controller->dpadRight.isDown)
		{
			Win32ProcessButtonState(&controller->dpadRight, (controller->stickAverage.x > threshold ? 1 : 0), dt);
		}

		// Start & Back
		Win32ProcessButtonState(&controller->start, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_START), dt);
		Win32ProcessButtonState(&controller->back, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_BACK), dt);
	}
	else
	{
		controller->isConnected = false;
	}
}

internal void Win32HandleKeyboardInput(MSG* msg, WPARAM wParam, LPARAM lParam, FGameControllerInput* input)
{
	u32 vKCode = (u32)wParam;
	bool32 wasDown = ((lParam & (1 << 30)) != 0);
	bool32 isDown =  ((lParam & (1 << 31)) == 0);

	bool32 altIsDown = (lParam & (1 << 29));
	if (altIsDown && (vKCode == VK_RETURN) && isDown && !wasDown)
	{
		ToggleFullscreen(msg->hwnd);
	}

	if ((altIsDown && (vKCode == VK_F4)) || ((vKCode == VK_ESCAPE)))
	{
		GlobalRunning = false;
	}
}

internal void Win32HandleWindowsMessageLoop(FGameControllerInput* controller)
{
	MSG message;
	PeekMessage(&message, 0, 0, 0, PM_REMOVE);
	switch (message.message)
	{
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Win32HandleKeyboardInput(&message, message.wParam, message.lParam, controller);
		} break;

		case WM_DESTROY:
		case WM_CLOSE:
		{
			GlobalRunning = false;
		} break;

		default:
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}
}

internal LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	LRESULT result = 0;

	switch (Message)
	{
		case WM_DESTROY:
		case WM_CLOSE:
		{
			GlobalRunning = false;
		} break;

		case WM_PAINT:
		{
			Render(&GlobalApplicationHandle->world);
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Assert(!"Keyboard input came in through a non-dispatch message.");
		} break;

		default:
		{
			result = DefWindowProcW(Window, Message, WParam, LParam);
		} break;
	}
	return result;
}

//////////////////////////////////////////////////
// Win32System
//////////////////////////////////////////////////
internal void Win32Initialize(FEngineMemory* memory, Win32System* win32System)
{
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
	Initialize(&memory->scratch, &win32System->world, screenWidth, screenHeight, VSYNC_ENABLED, win32System->window, FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR);
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
	LARGE_INTEGER perfFrequency;
	QueryPerformanceFrequency(&perfFrequency);

	LARGE_INTEGER lastCounter;
	QueryPerformanceCounter(&lastCounter);

	FEngineMemory engineMemory = {};
	u32 totalSize = Megabytes(64) + Megabytes(8);
	void* base = VirtualAlloc(0, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	engineMemory.permanent = ArenaMake((u8*)base, Megabytes(64));
	engineMemory.scratch = ArenaMake((u8*)base + Megabytes(64), Megabytes(8));

	Win32System win32System = {};
	Win32Initialize(&engineMemory, &win32System);
	
	Win32LoadXInput();
	FGameState gameState = {};
	FGameInput input = {};

	// Game loop.
	gameState.running = true;
	while (GlobalRunning && gameState.running)
	{
		LARGE_INTEGER currentCounter;
		QueryPerformanceCounter(&currentCounter);
		f32 deltaTime =
			(f32)(currentCounter.QuadPart - lastCounter.QuadPart) /
			(f32)perfFrequency.QuadPart;

		input.deltaTime = deltaTime;

		// Handle windows messages.
		Win32HandleWindowsMessageLoop(&input.controller);
		Win32HandleControllerInput(win32System.window, &input);

		// Update and render.
		GameUpdate(&engineMemory, &gameState, &input);
		Render(&win32System.world);

		// Update the previous buttons state.
		for (u32 buttonIndex = 0; buttonIndex < ArrayCount(input.controller.buttons); ++buttonIndex)
		{
			input.controller.buttons[buttonIndex].wasDown = input.controller.buttons[buttonIndex].isDown;
		}

		lastCounter = currentCounter;
	}

	return 0;
}