// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef WIN32_FADO_H
#define WIN32_FADO_H

/*
** Windows Platform Layer **
* Starting point of the code, the game loop and where everything is setup and initialized
* for windows (currently the engine is only supported on windows).
*/

// ─────────────────────────────────────────────
#include "fado_d3d.h"
#include "fado.h"

// ─────────────────────────────────────────────
#define WIN32_LEAN_AND_MEAN
#define FULL_SCREEN false
#define VSYNC_ENABLED true
#define SCREEN_DEPTH 1000.0f
#define SCREEN_NEAR 0.3f
// ─────────────────────────────────────────────

// Holds the game code dll and the main update function.
// Loaded on startup and reloaded if the dll is rebuilt during runtime.
struct Win32GameCode
{
	HMODULE gameCodeDLL;
	FILETIME dllLastWriteTime;

	FGameUpdate* gameUpdate;
	b32 isValid;
};

// Main application.
// Holds a pointer to the renderer world and the game code.
struct Win32System
{
	HINSTANCE instance;
	HWND window;
	FRenderWorld* world;
};

// ─────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────
global_variable Win32System* GlobalWin32System;
global_variable WINDOWPLACEMENT GlobalWindowPosition = { sizeof(GlobalWindowPosition) };
global_variable b32 GlobalShowCursor = true;
global_variable b32 GlobalRunning = true;
global_variable LARGE_INTEGER GlobalPerfCountFrequency;

#endif // WIN32_FADO_H