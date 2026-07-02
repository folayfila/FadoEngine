// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#include "fado_win32.h"
#include <xinput.h>
#include "fado_math.h"
#include "fado_collision.h"

#if FADO_DEBUG
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/backends/imgui_impl_win32.h"
#include "ThirdParty/imgui/backends/imgui_impl_dx11.h"
#endif // FADO_DEBUG

// ────────────────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────────────────
internal void ToggleFullscreen(HWND Window)
{
	// Note: Copied code from internet
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
// Game Code
// ────────────────────────────────────────────────────────────────────────
internal FILETIME Win32GetLastWriteTime(char* fileName)
{
	FILETIME lastWriteTime = {};

	WIN32_FILE_ATTRIBUTE_DATA data;
	if (GetFileAttributesExA(fileName, GetFileExInfoStandard, &data))
	{
		lastWriteTime = data.ftLastWriteTime;
	}
	return lastWriteTime;
}

// Returns the loaded game moode if successful.
// - sourceDLLName: The actual dll to load. 
// - tempDLLName: a temporary dll that gets created while loading the new one to bypass msvc.
internal Win32GameCode Win32LoadGameCode(char* sourceDLLName, char* tempDLLName)
{
	Win32GameCode gameCode = {};

	gameCode.dllLastWriteTime = Win32GetLastWriteTime(sourceDLLName);
	CopyFileA(sourceDLLName, tempDLLName, FALSE);

	gameCode.gameCodeDLL = LoadLibraryA(tempDLLName);
	if (gameCode.gameCodeDLL)
	{
		gameCode.gameUpdate = (FGameUpdate*)GetProcAddress(gameCode.gameCodeDLL, "GameUpdate");
		gameCode.isValid = (gameCode.gameUpdate != nullptr);
	}

	if (!gameCode.isValid)
	{
		gameCode.gameUpdate = 0;
	}

	return gameCode;
}

internal void Win32UnloadGameCode(Win32GameCode* gameCode)
{
	if (gameCode->gameCodeDLL)
	{
		FreeLibrary(gameCode->gameCodeDLL);
	}
	gameCode->gameCodeDLL = 0;
	gameCode->isValid = false;
	gameCode->gameUpdate = 0;
}

// ────────────────────────────────────────────────────────────────────────
// Sound
// ────────────────────────────────────────────────────────────────────────

// Initializes XAudio2 and creates the voices used for audio playback.
internal void Win32InitSound(Win32SoundState* soundState)
{
	// -- 2D --
	HRESULT result = XAudio2Create(&soundState->xaudio2);
	Assert(!FAILED(result));

	result = soundState->xaudio2->CreateMasteringVoice(&soundState->masterVoice);
	Assert(!FAILED(result));

	WAVEFORMATEX format2D = {};
	format2D.wFormatTag = WAVE_FORMAT_PCM;
	format2D.nChannels = SOUND_CHANNELS;
	format2D.nSamplesPerSec = SOUND_SAMPLE_RATE;
	format2D.wBitsPerSample = 16;
	format2D.nBlockAlign = (format2D.nChannels * format2D.wBitsPerSample) / 8;	// in bytes
	format2D.nAvgBytesPerSec = format2D.nSamplesPerSec * format2D.nBlockAlign;

	result = soundState->xaudio2->CreateSourceVoice(&soundState->sourceVoice, &format2D, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &GlobalVoiceCallback);
	Assert(!FAILED(result));

	// Start playback. It will remain idle until buffers are submitted.
	soundState->sourceVoice->Start(0);

	soundState->initialized = true;
	soundState->currentBuffer = 0;

	// -- 3D --
	// Mono format, since panning is computed manually and applied via the output matrix.
	WAVEFORMATEX format3D = {};
	format3D.wFormatTag = WAVE_FORMAT_PCM;
	format3D.nChannels = 1; // mono source, we pan manually
	format3D.nSamplesPerSec = SOUND_SAMPLE_RATE;
	format3D.wBitsPerSample = 16;
	format3D.nBlockAlign = (format3D.nChannels * format3D.wBitsPerSample) / 8;
	format3D.nAvgBytesPerSec = format3D.nSamplesPerSec * format3D.nBlockAlign;

	for (i32 i = 0; i < WIN32_MAX_3D_VOICES; i++)
	{
		soundState->xaudio2->CreateSourceVoice(&Global_3DVoiceSlots[i].voice, &format3D, 0,
			XAUDIO2_DEFAULT_FREQ_RATIO,
			nullptr);	// no callback needed, we poll GetState() instead

		Global_3DVoiceSlots[i].inUse = false;
	}
}

// Copies a mixed audio buffer into the next XAudio2 buffer and submits it (2D).
internal void Win32SubmitSound(Win32SoundState* soundState, FSoundOutput* output)
{
	i16* dst = soundState->buffers[soundState->currentBuffer];

	fmemcpy(dst, output->samples, (output->sampleCount * SOUND_CHANNELS * sizeof(i16)));

	XAUDIO2_BUFFER xbuf = {};
	xbuf.AudioBytes = (output->sampleCount * SOUND_CHANNELS * sizeof(i16));
	xbuf.pAudioData = (BYTE*)dst;

	// Queue it for playback and advance to the next buffer in the ring.
	soundState->sourceVoice->SubmitSourceBuffer(&xbuf);
	soundState->currentBuffer = (soundState->currentBuffer + 1) % WIN32_SOUND_BUFFER_COUNT;		// The % (modulo) wraps back to the beginning.
}

internal i32 Win32Acquire3DVoice()
{
	for (i32 i = 0; i < WIN32_MAX_3D_VOICES; i++)
	{
		if (!Global_3DVoiceSlots[i].inUse)
		{
			Global_3DVoiceSlots[i].inUse = true;
			return i;
		}
	}
	return -1; // pool exhausted
}

internal void Win32Release3DVoice(i32 slot)
{
	if (slot < 0)
	{
		return;
	}
	Global_3DVoiceSlots[slot].voice->Stop(0);
	Global_3DVoiceSlots[slot].voice->FlushSourceBuffers();
	Global_3DVoiceSlots[slot].inUse = false;
}

internal void Win32Update3DSoundInstance(FSoundManager* manager, FSoundInstance* instance, i32 instanceHandle, IXAudio2MasteringVoice* masterVoice)
{
	Win32VoiceSlot3D* slot = nullptr;

	// Acquire + submit buffer once
	if (instance->voiceSlot < 0)
	{
		instance->voiceSlot = Win32Acquire3DVoice();
		if (instance->voiceSlot < 0)
		{
			return; // pool exhausted this frame
		}

		slot = &Global_3DVoiceSlots[instance->voiceSlot];

		FSoundBuffer* buf = &manager->assetBank->assets[instance->bufferIndex];

		XAUDIO2_BUFFER xbuf = {};
		xbuf.AudioBytes = buf->sampleCount * sizeof(i16);
		xbuf.pAudioData = (BYTE*)buf->samples;
		xbuf.LoopCount = instance->loop ? XAUDIO2_LOOP_INFINITE : 0;

		slot->voice->SubmitSourceBuffer(&xbuf);
		slot->voice->Start(0);
	}
	else
	{
		slot = &Global_3DVoiceSlots[instance->voiceSlot];
	}

	// Check if finished (non-looping only)
	if (!instance->loop)
	{
		XAUDIO2_VOICE_STATE state;
		slot->voice->GetState(&state);

		if (state.BuffersQueued == 0)
		{
			Win32Release3DVoice(instance->voiceSlot);
			SoundStop(manager, instanceHandle);
			return;
		}
	}

	// Compute + apply matrix every frame (position/listener may have moved)
	f32 distance = V3Distance(manager->listener.position, instance->position);
	f32 atten = SoundCalculateAttenuation(distance, instance->minDistance, instance->maxDistance);
	f32 pan = SoundCalculatePan(&manager->listener, instance->position);

	f32 matrix[2];
	SoundCalculateStereoMatrix(pan, instance->volume * atten, matrix);

	slot->voice->SetOutputMatrix(masterVoice, 1, 2, matrix);
}

// Force end all current 3D instances.
internal void Win32StopAll3DSounds()
{
	for (i32 i = 0; i < WIN32_MAX_3D_VOICES; i++)
	{
		Win32VoiceSlot3D* slot = &Global_3DVoiceSlots[i];
		if (slot->inUse)
		{
			slot->voice->Stop(0);
			slot->voice->FlushSourceBuffers();
			slot->inUse = false;
		}
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

internal void Win32ProcessButtonState(FGameButtonState* state, b8 isDown, f32 deltaTime)
{
	state->wasDown = state->isDown;
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

internal b8 Win32IsXInputButtonDown(DWORD XInputButtonState, DWORD ButtonBit)
{
	b8 result = ((XInputButtonState & ButtonBit) == ButtonBit);
	return result;
}

internal void Win32HandleControllerInput(HWND Window, FGameInput* input)
{
	f32 dt = input->deltaTime;

	POINT mousePoint;
	GetCursorPos(&mousePoint);
	ScreenToClient(Window, &mousePoint);

	// Update the mouse only if it's within the window bounds.
	RECT clientRect;
	GetClientRect(Window, &clientRect);
	if ((mousePoint.x >= clientRect.left) && (mousePoint.x <= clientRect.right) &&
		(mousePoint.y >= clientRect.top) && (mousePoint.y <= clientRect.bottom))
	{
		// Rotation:
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

	// Update mouse buttons state.
	Win32ProcessButtonState(&input->mouse.buttons[0], (GetKeyState(VK_LBUTTON) & (1 << 15)),  dt);
	Win32ProcessButtonState(&input->mouse.buttons[1], (GetKeyState(VK_MBUTTON) & (1 << 15)),  dt);
	Win32ProcessButtonState(&input->mouse.buttons[2], (GetKeyState(VK_RBUTTON) & (1 << 15)),  dt);
	Win32ProcessButtonState(&input->mouse.buttons[3], (GetKeyState(VK_XBUTTON1) & (1 << 15)), dt);
	Win32ProcessButtonState(&input->mouse.buttons[4], (GetKeyState(VK_XBUTTON2) & (1 << 15)), dt);

	// No-clip camera can rotate only if the right-click mouse button is pressed.
	// TODO: Create button state functions, e.g. Held, Clicked, Pressed.
	// TODO: Add different camera types and handle their movement and input based on their type.
	input->mouse.isRotating = (input->mouse.buttons[2].heldLength > 0.0f);

	u32 maxControllerCount = XUSER_MAX_COUNT;
	if (maxControllerCount > (ArrayCount(input->controllers) - 1))
	{
		maxControllerCount = (ArrayCount(input->controllers) - 1);
	}

	// Update controllers input.
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
	b8 wasDown = ((lParam & (1 << 30)) != 0);
	b8 isDown =  ((lParam & (1 << 31)) == 0);

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

	b8 altIsDown = (lParam & (1 << 29));
	if (altIsDown && (vKCode == VK_RETURN) && isDown && !wasDown)
	{
		ToggleFullscreen(msg->hwnd);
	}

	if ((altIsDown && (vKCode == VK_F4)) || ((vKCode == VK_ESCAPE)))
	{
		GlobalRunning = false;
	}
}

// Custom window message loop handle. This is where our custom input handles are triggered.
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

// Windows proc callback function. Handles all messages except input.
#if FADO_DEBUG
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#endif // FADO_DEBUG
internal LRESULT CALLBACK Win32MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{

#if FADO_DEBUG
	if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
	{
		return true;
	}
#endif // FADO_DEBUG

	LRESULT result = 0;

	switch (message)
	{
		case WM_DESTROY:
		case WM_CLOSE:
		{
			GlobalRunning = false;
		} break;

		case WM_SIZE:
		{
			if (GlobalWin32System && GlobalWin32System->world)
			{
				i32 newWidth = LOWORD(lParam);
				i32 newHeight = HIWORD(lParam);
				if (newWidth > 0 && newHeight > 0)
				{
					D3DResize(GlobalWin32System->world, newWidth, newHeight, SCREEN_NEAR, SCREEN_DEPTH);
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
			result = DefWindowProcW(window, message, wParam, lParam);
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
	//windowClass.hIcon = LoadIconW(NULL, MAKEINTRESOURCE(IDI_APP_ICON));
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
	d3dInitParams.window = win32System->window;
	d3dInitParams.screenWidth = screenWidth;
	d3dInitParams.screenHeight = screenHeight;
	d3dInitParams.vsync = VSYNC_ENABLED;
	d3dInitParams.fullScreen = FULL_SCREEN;
	d3dInitParams.screenDepth = SCREEN_DEPTH;
	d3dInitParams.screenNear = SCREEN_NEAR;

	// Initialize Dx11.
	win32System->world->shared->scratchArena = &memory->scratch;
	InitializeFD3D(win32System->world, &d3dInitParams);

#if FADO_DEBUG
	// Init ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplWin32_Init(win32System->window);
	ImGui_ImplDX11_Init(win32System->world->d3d.device, win32System->world->d3d.deviceContext);
	ImGui::StyleColorsDark();
#endif // FADO_DEBUG
}

// Called before starting the game loop.
// Loads all assets at startup.
internal void LoadAssets(FRenderWorld* world, FGameState* gameState)
{
	gameState->hPlaneMesh = LoadFModel(world, "Assets\\Models\\plane.fmodel");
	gameState->hCubeMesh = LoadFModel(world, "Assets\\Models\\cube.fmodel");
	gameState->hSphereMesh = LoadFModel(world, "Assets\\Models\\sphere.fmodel");
	gameState->hSkyBoxMesh = LoadFModel(world, "Assets\\Models\\skybox.fmodel");
	//HMesh hMonkey = LoadGLBIntoWorld(world, "models\\monkey.fmodel");

	gameState->hGridTexture = LoadFImage(world, "Assets\\Textures\\grid.fimage");
	gameState->hMosaicTexture = LoadFImage(world, "Assets\\Textures\\mosaic.fimage");
	gameState->hGraniteTexture = LoadFImage(world, "Assets\\Textures\\granite.fimage");
	gameState->hSkyBoxTexture = LoadFImage(world, "Assets\\Textures\\skybox_0.fimage");
	gameState->hWhiteTexture = LoadFImage(world, "Assets\\Textures\\white.fimage");

	//LoadFont(world, "AssetsSource\\Fonts\\bahnschrift.ttf", 25.0f, gameState->font);
	LoadFFont(world, "Assets\\Fonts\\arialbd.ffont", 25.0f, gameState->font);

	// Temporary test, using royalty free sounds:
	// https://pixabay.com/music/video-games-sinnesl%C3%B6schen-beam-117362/
	gameState->hMusic = LoadFSound(gameState->soundManager, gameState->shared->permenantArena, gameState->shared->scratchArena, "Assets\\Audio\\Music\\sinneschlosen-sinnesloschen-beam-117362.fsound");
	// https://pixabay.com/sound-effects/film-special-effects-impact-sound-effect-8-bit-retro-151796/
	gameState->hCollideSFX = LoadFSound(gameState->soundManager, gameState->shared->permenantArena, gameState->shared->scratchArena, "Assets\\Audio\\SFX\\lesiakower-impact-sound-effect-8-bit-retro-151796.fsound");
	// https://pixabay.com/sound-effects/technology-click-21156/
	gameState->hUIClickSFX = LoadFSound(gameState->soundManager, gameState->shared->permenantArena, gameState->shared->scratchArena, "Assets\\Audio\\SFX\\666herohero-click-21156.fsound");
	// https://pixabay.com/sound-effects/nature-fire-crackling-sounds-427410/
	gameState->hFireSFX = LoadFSound(gameState->soundManager, gameState->shared->permenantArena, gameState->shared->scratchArena, "Assets\\Audio\\SFX\\dragon-studio-fire-crackling-sounds-427410.fsound");
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

	c8 sourceGameCodeDLLFullPath[MAX_PATH] = "..\\Debug\\Game.dll";
	c8 tempGameCodeDLLFullPath[MAX_PATH] = "..\\Debug\\tempGame.dll";;
	Win32GameCode gameCode = Win32LoadGameCode(sourceGameCodeDLLFullPath, tempGameCodeDLLFullPath);

	// Create all memory for the game upfront, and use it across the game.
	FEngineMemory engineMemory = {};
	u32 totalSize = PERMANENT_ARENA_SIZE + SCRATCH_ARENA_SIZE;	// 80 MB
	void* base = VirtualAlloc(0, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	engineMemory.permanent = ArenaMake((u8*)base, PERMANENT_ARENA_SIZE);
	engineMemory.scratch = ArenaMake((u8*)base + PERMANENT_ARENA_SIZE, SCRATCH_ARENA_SIZE);

	// Game state
	FGameState* gameState = ArenaPushType(&engineMemory.permanent, FGameState);
	gameState->shared = ArenaPushType(&engineMemory.permanent, FSharedStuff);
	gameState->shared->transforms = ArenaPushType(&engineMemory.permanent, FTransformTable);
	gameState->shared->entityTable = ArenaPushType(&engineMemory.permanent, FEntityTable);
	gameState->shared->collisionWorld = ArenaPushType(&engineMemory.permanent, FCollisionWorld);
	gameState->shared->uiCommands = ArenaPushType(&engineMemory.permanent, FUICommandBucket);
	gameState->shared->permenantArena = &engineMemory.permanent;
	gameState->shared->scratchArena = &engineMemory.scratch;
	gameState->font = ArenaPushType(&engineMemory.permanent, FFont);

	// Renderer
	Win32System win32System = {};
	win32System.world = ArenaPushType(&engineMemory.permanent, FRenderWorld);
	win32System.world->shared = gameState->shared;
	Win32InitializeWindowAndD3D(&engineMemory, &win32System);

	// Input
	Win32LoadXInput();
	FGameInput* input = ArenaPushType(&engineMemory.permanent, FGameInput);

	// Sound
	Win32SoundState win32Sound = *ArenaPushType(&engineMemory.permanent, Win32SoundState);
	Win32InitSound(&win32Sound);
	FSoundManager soundManager = {};
	soundManager.active = true;
	soundManager.assetBank = ArenaPushType(&engineMemory.permanent, FSoundAssetBank);
	soundManager.masterVolume = 1.0f;
	for (i32 i = 0; i < SOUND_CATEGORY_COUNT; i++)
	{
		soundManager.categoryVolume[i] = 1.0f;
	}
	soundManager.mixBuffer = ArenaPushArray(&engineMemory.permanent, i16, (WIN32_SOUND_SAMPLES_PER_FRAME * SOUND_CHANNELS));
	gameState->soundManager = &soundManager;

	// Load all assets
	LoadAssets(win32System.world, gameState);

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

		// Check if we need to reload the game code (hot reload).
		FILETIME newDLLWriteTime = Win32GetLastWriteTime(sourceGameCodeDLLFullPath);
		if (CompareFileTime(&newDLLWriteTime, &gameCode.dllLastWriteTime) != 0)
		{
			Win32UnloadGameCode(&gameCode);
			gameCode = Win32LoadGameCode(sourceGameCodeDLLFullPath, tempGameCodeDLLFullPath);
		}

#if FADO_DEBUG
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
#endif // FADO_DEBUG

		// Handle keyboard and controller input.
		FGameControllerInput* keyboardInput = &input->controllers[0];
		keyboardInput->isConnected = true;
		Win32HandleWindowsMessageLoop(keyboardInput, deltaTime);
		Win32HandleControllerInput(win32System.window, input);

		// Update game.
		if (gameCode.gameUpdate)
		{
			gameCode.gameUpdate(gameState, input);
		}

		// Update sound
		// check how many buffers XAudio2 still has queued.
		XAUDIO2_VOICE_STATE state;
		win32Sound.sourceVoice->GetState(&state);
		// only mix and submit if XAudio2 is hungry.
		if (state.BuffersQueued < WIN32_SOUND_BUFFER_COUNT)
		{
			if (!soundManager.active)
			{
				Win32StopAll3DSounds();
				soundManager.active = true;
			}

			FSoundOutput output = {};
			output.samples = soundManager.mixBuffer;
			output.sampleCount = WIN32_SOUND_SAMPLES_PER_FRAME;
			FadoZeroMemory(output.samples, (output.sampleCount * SOUND_CHANNELS * sizeof(i16)));

			// Update 2D and 3D sounds in one loop.
			for (i32 i = 0; i < FMAX_SOUND_INSTANCES; ++i)
			{
				FSoundInstance* instance = &soundManager.assetBank->instances[i];
				if (!instance->active || !instance->playing)
				{
					continue;
				}

				if (instance->is3D)
				{
					// Udpate each 3D sound individually.
					Win32Update3DSoundInstance(&soundManager, instance, i, win32Sound.masterVoice);
				}
				else
				{
					SoundMixInstance(&soundManager, instance, i, &output);
				}
			}
			// Submit the 2D mixed sound output.
			Win32SubmitSound(&win32Sound, &output);
		}

		// Render
#if FADO_DEBUG
		DebugRender(win32System.world);
#else
		Render(win32System.world);
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