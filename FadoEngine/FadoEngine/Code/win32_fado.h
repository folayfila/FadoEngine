#ifndef WIN32_FADO_H
#define WIN32_FADO_H

///////////////////////////////
// PRE-PROCESSING DIRECTIVES //
///////////////////////////////
#define WIN32_LEAN_AND_MEAN
#define FULL_SCREEN false
#define VSYNC_ENABLED true
#define SCREEN_DEPTH 1000.0f
#define SCREEN_NEAR 0.3f

///////////////////////////////
// INCLUDES //
///////////////////////////////
#include "fado_d3d.h"
#include "../../Game/code/fado.h"

struct Win32GameCode
{
	HMODULE gameCodeDLL;
	FILETIME dllLastWriteTime;

	FGameUpdate* gameUpdate;
	b32 isValid;
};

struct Win32System
{
	HINSTANCE instance;
	HWND window;
	FEngineMemory* engineMemory;
	FGameState* gameState;
	FRenderWorld* world;
	Win32GameCode* gameCode;
};

///////////////////////////////
// Globals //
///////////////////////////////
global_variable Win32System* GlobalWin32System;
global_variable WINDOWPLACEMENT GlobalWindowPosition = { sizeof(GlobalWindowPosition) };
global_variable b32 GlobalShowCursor = true;
global_variable b32 GlobalRunning = true;
global_variable LARGE_INTEGER GlobalPerfCountFrequency;

#endif // WIN32_FADO_H