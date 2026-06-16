#include "win32_fado.h"
#include <xinput.h>
#include "fado_math.h"
#include "fado_collision.h"

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

// ────────────────────────────────────────────────────────────────────────
// Input
// ────────────────────────────────────────────────────────────────────────
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

	// Only update if mouse is within the window bounds and the mouse-right click
	RECT clientRect;
	GetClientRect(Window, &clientRect);
	if ((mousePoint.x >= clientRect.left) && (mousePoint.x <= clientRect.right) &&
		(mousePoint.y >= clientRect.top) && (mousePoint.y <= clientRect.bottom))
	{
		// We update the delta only if the user was holding the right-click mouse button, i.e. was already rotating.
		// This prevents huge delta values if the user goes outside of the bounds and then back from another corner,
		// in that case, we just set the update the mouse position to the current, and calculate delta on the next frame.
		if (input->mouse.isRotating)
		{
			input->mouse.deltaX = mousePoint.x - input->mouse.x;
			input->mouse.deltaY = mousePoint.y - input->mouse.y;
		}
		else
		{
			input->mouse.deltaX = 0;
			input->mouse.deltaY = 0;
			input->mouse.isRotating = true;
		}
		input->mouse.x = mousePoint.x;
		input->mouse.y = mousePoint.y;
	}
	else
	{
		// Reset the position and delta if the mouse is out of the game bounds.
		input->mouse.x = 0;
		input->mouse.y = 0;
		input->mouse.deltaX = 0;
		input->mouse.deltaY = 0;
		input->mouse.isRotating = false;
	}
	input->mouse.z = 0;

	Win32ProcessButtonState(&input->mouse.buttons[0], (GetKeyState(VK_LBUTTON) & (1 << 15)),  dt);
	Win32ProcessButtonState(&input->mouse.buttons[1], (GetKeyState(VK_MBUTTON) & (1 << 15)),  dt);
	Win32ProcessButtonState(&input->mouse.buttons[2], (GetKeyState(VK_RBUTTON) & (1 << 15)),  dt);
	Win32ProcessButtonState(&input->mouse.buttons[3], (GetKeyState(VK_XBUTTON1) & (1 << 15)), dt);
	Win32ProcessButtonState(&input->mouse.buttons[4], (GetKeyState(VK_XBUTTON2) & (1 << 15)), dt);

	input->mouse.isRotating = ((input->mouse.buttons[2].isDown) || (input->mouse.buttons[2].wasDown));

	u32 maxControllerCount = XUSER_MAX_COUNT;
	if (maxControllerCount > (ArrayCount(input->controllers) - 1))
	{
		maxControllerCount = (ArrayCount(input->controllers) - 1);
	}

	for (u32 controllerIndex = 0; controllerIndex < maxControllerCount; ++controllerIndex)
	{
		// 0 in input->controllers is for the keyboard, but XInput counts the first controller as 0,
		// so we increment the index here only to get the controller, but use controllerIndex as is for XInput.
		FGameControllerInput* controller = &input->controllers[controllerIndex + 1];

		XINPUT_STATE controllerState;
		if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
		{
			// This controller is pluged in.
			controller->isConnected = true;

			XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

			// Analog is handled in the game code by checking the stick average.
			controller->leftStickAverage.x = Win32ProcessXInputStickValue(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			controller->leftStickAverage.y = Win32ProcessXInputStickValue(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			controller->rightStickAverage.x = Win32ProcessXInputStickValue(pad->sThumbRX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			controller->rightStickAverage.y = Win32ProcessXInputStickValue(pad->sThumbRY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

			controller->isAnalog = (controller->leftStickAverage.x != 0 || controller->leftStickAverage.y != 0);

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
			Win32ProcessButtonState(&controller->actionDown, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_A), dt);
			Win32ProcessButtonState(&controller->actionUp, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_Y), dt);
			Win32ProcessButtonState(&controller->actionLeft, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_X), dt);
			Win32ProcessButtonState(&controller->actionRight, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_B), dt);

			// DPad Buttons
			Win32ProcessButtonState(&controller->dpadDown, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_DPAD_DOWN), dt);
			Win32ProcessButtonState(&controller->dpadUp, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_DPAD_UP), dt);
			Win32ProcessButtonState(&controller->dpadLeft, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_DPAD_LEFT), dt);
			Win32ProcessButtonState(&controller->dpadRight, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_DPAD_RIGHT), dt);

			// Start & Back
			Win32ProcessButtonState(&controller->start, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_START), dt);
			Win32ProcessButtonState(&controller->back, Win32IsXInputButtonDown(pad->wButtons, XINPUT_GAMEPAD_BACK), dt);
		}
		else
		{
			controller->isConnected = false;
		}
	}
}

internal void Win32HandleKeyboardInput(MSG* msg, WPARAM wParam, LPARAM lParam, FGameControllerInput* keyboard, f32 deltaTime)
{
	u32 vKCode = (u32)wParam;
	bool32 wasDown = ((lParam & (1 << 30)) != 0);
	bool32 isDown =  ((lParam & (1 << 31)) == 0);

	if (vKCode == 'W')
	{
		Win32ProcessButtonState(&keyboard->dpadUp, isDown, deltaTime);
	}
	if (vKCode == 'S')
	{
		Win32ProcessButtonState(&keyboard->dpadDown, isDown, deltaTime);
	}
	if (vKCode == 'A')
	{
		Win32ProcessButtonState(&keyboard->dpadLeft, isDown, deltaTime);
	}
	if (vKCode == 'D')
	{
		Win32ProcessButtonState(&keyboard->dpadRight, isDown, deltaTime);
	}
	if (vKCode == 'E')
	{
		Win32ProcessButtonState(&keyboard->rightShoulder, isDown, deltaTime);
	}
	if (vKCode == 'Q')
	{
		Win32ProcessButtonState(&keyboard->leftShoulder, isDown, deltaTime);
	}

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

internal void Win32HandleWindowsMessageLoop(FGameControllerInput* keyboard, f32 deltaTime)
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		switch (message.message)
		{
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				Win32HandleKeyboardInput(&message, message.wParam, message.lParam, keyboard, deltaTime);
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
		

		// On resize/move we start a timer that updates the game and renders so we don't get a frozen screen.
		case WM_ENTERSIZEMOVE:
		{
			SetTimer(Window, 1, 16, NULL);
		} break;

		case WM_EXITSIZEMOVE:
		{
			KillTimer(Window, 1);
		} break;

		case WM_TIMER:
		{
			if (GlobalWin32System->gameState && GlobalWin32System->gameState->initialized)
			{
				FGameInput input = {};
				input.deltaTime = 0.016f;
				GameUpdate(GlobalWin32System->engineMemory, GlobalWin32System->gameState, &input);
#if FADO_DEBUG
				DebugRender(GlobalWin32System->world, GlobalWin32System->gameState->entityTable, GlobalWin32System->gameState->transforms, GlobalWin32System->gameState->collisionWorld);
#else
				Render(GlobalWin32System->world, GlobalWin32System->gameState->entityTable, GlobalWin32System->gameState->transforms);
#endif // FADO_DEBUG
			}
		} break;

		case WM_SIZE:
		{
			if (GlobalWin32System && GlobalWin32System->world)
			{
				i32 newWidth = LOWORD(LParam);
				i32 newHeight = HIWORD(LParam);
				if (newWidth > 0 && newHeight > 0)
				{
					D3DResize(&GlobalWin32System->world->d3d, newWidth, newHeight, SCREEN_NEAR, SCREEN_DEPTH);
				}
			}
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

// ────────────────────────────────────────────────────────────────────────
// Win32System
// ────────────────────────────────────────────────────────────────────────

// Initialize and create the game's Window and initialze DX11.
internal void Win32InitializeWindowAndD3D(FEngineMemory* memory, Win32System* win32System)
{
	GlobalWin32System = win32System;

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

	FD3DInitParams d3dInitParams = {};
	d3dInitParams.d3d = &win32System->world->d3d;
	d3dInitParams.window = win32System->window;
	d3dInitParams.screenWidth = screenWidth;
	d3dInitParams.screenHeight = screenHeight;
	d3dInitParams.vsync = VSYNC_ENABLED;
	d3dInitParams.fullScreen = FULL_SCREEN;
	d3dInitParams.screenDepth = SCREEN_DEPTH;
	d3dInitParams.screenNear = SCREEN_NEAR;

	// Initialize Dx11.
	win32System->world->scratchArena = &memory->scratch;
	InitializeFD3D(win32System->world, &d3dInitParams, win32System->gameState->transforms);
}

// Called before starting the game loop.
// Loads all models and textures at startup.
internal void InitLoadAssets(FRenderWorld* world, FGameState* gameState)
{
	gameState->hPlaneMesh = LoadGLBModel(world, "src\\assets_src\\models\\plane.glb");
	gameState->hCubeMesh = LoadGLBModel(world, "src\\assets_src\\models\\cube.glb");
	gameState->hSphereMesh = LoadGLBModel(world, "src\\assets_src\\models\\sphere.glb");
	//HMesh hMonkey = LoadGLBIntoWorld(world, "src\\models\\monkey.glb");

	gameState->hGridTexture = LoadFImage(world, "src\\assets\\textures\\grid.fasset");
	gameState->hMosaicTexture = LoadFImage(world, "src\\assets\\textures\\mosaic.fasset");
	gameState->hGraniteTexture = LoadFImage(world, "src\\assets\\textures\\granite.fasset");
}

// ────────────────────────────────────────────────────────────────────────
// Main
// ────────────────────────────────────────────────────────────────────────/
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

	// Create all memory for the game upfront, and use it across the game.
	FEngineMemory engineMemory = {};
	u32 totalSize = Megabytes(64) + Megabytes(16);
	void* base = VirtualAlloc(0, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	engineMemory.permanent = ArenaMake((u8*)base, Megabytes(64));
	engineMemory.scratch = ArenaMake((u8*)base + Megabytes(64), Megabytes(16));
	
	Win32LoadXInput();
	FGameState* gameState = ArenaPushSize(&engineMemory.permanent, FGameState);
	gameState->transforms = ArenaPushSize(&engineMemory.permanent, FTransformTable);
	gameState->entityTable = ArenaPushSize(&engineMemory.permanent, FEntityTable);
	gameState->collisionWorld = ArenaPushSize(&engineMemory.permanent, FCollisionWorld);

	Win32System win32System = {};
	win32System.world = ArenaPushSize(&engineMemory.permanent, FRenderWorld);
	win32System.gameState = gameState;
	win32System.engineMemory = &engineMemory;
	Win32InitializeWindowAndD3D(&engineMemory, &win32System);
	InitLoadAssets(win32System.world, gameState);

	FGameInput* input = ArenaPushSize(&engineMemory.permanent, FGameInput);

	// Game loop.
	gameState->running = true;
	while (GlobalRunning && gameState->running)
	{
		// Update delta time
		LARGE_INTEGER currentCounter;
		QueryPerformanceCounter(&currentCounter);
		f32 deltaTime = (f32)(currentCounter.QuadPart - lastCounter.QuadPart) / (f32)perfFrequency.QuadPart;
		// Clamp to avoid huge dt after modal resize/move loop blocks for a long time.
		if (deltaTime > (1.0f / 15.0f))
		{ 
			deltaTime = 1.0f / 60.0f;
		}
		input->deltaTime = deltaTime;

		// Handle keyboard and controller input.
		FGameControllerInput* keyboardInput = &input->controllers[0];
		keyboardInput->isConnected = true;
		Win32HandleWindowsMessageLoop(keyboardInput, deltaTime);
		Win32HandleControllerInput(win32System.window, input);

		// Update game and render.
		GameUpdate(&engineMemory, gameState, input);

#if FADO_DEBUG
		DebugRender(win32System.world, win32System.gameState->entityTable, win32System.gameState->transforms, gameState->collisionWorld);
#else
		Render(win32System.world, win32System.gameState->entityTable, win32System.gameState->transforms);
#endif // FADO_DEBUG

		// Update the previous buttons states.
		for (u32 controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
		{
			for (u32 buttonIndex = 0; buttonIndex < ArrayCount(input->controllers[0].buttons); ++buttonIndex)
			{
				input->controllers[controllerIndex].buttons[buttonIndex].wasDown = input->controllers[controllerIndex].buttons[buttonIndex].isDown;
			}
		}

		lastCounter = currentCounter;
	}

	return 0;
}