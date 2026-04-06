#ifndef WIN32_FADO_H
#define WIN32_FADO_H

///////////////////////////////
// PRE-PROCESSING DIRECTIVES //
///////////////////////////////
#define WIN32_LEAN_AND_MEAN
#define FULL_SCREEN 1
#define VSYNC_ENABLED true;
#define SCREEN_DEPTH 1000.0f;
#define SCREEN_NEAR = 0.3f;

///////////////////////////////
// INCLUDES //
///////////////////////////////
#include <Windows.h>
#include "FadoTypes.h"

// > TODO: Replace the input struct with an input API like DirectInput.
struct Win32Input
{
	bool32 Keys[256];
};

struct Win32Application
{
};

struct Win32System
{
	LPCWSTR ApplicationName;
	HINSTANCE Instance;
	HWND Window;

	Win32Input Input;
	Win32Application Application;
};

///////////////////////////////
// Globals //
///////////////////////////////
global_variable Win32System* ApplicationHandle;

#endif // WIN32_FADO_H