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

// > TODO: Replace the input struct with an input API like DirectInput.
struct Win32Input
{
	bool32 keys[256];
};

struct Win32System
{
	LPCWSTR applicationName;
	HINSTANCE instance;
	HWND window;

	Win32Input input;
	FApplication application;
};

///////////////////////////////
// Globals //
///////////////////////////////
global_variable Win32System* GlobalApplicationHandle;

#endif // WIN32_FADO_H