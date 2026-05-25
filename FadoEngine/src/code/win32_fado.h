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
#include "fado.h"

struct Win32System
{
	FRenderWorld world;

	HINSTANCE instance;
	HWND window;
};

///////////////////////////////
// Globals //
///////////////////////////////
global_variable Win32System* GlobalApplicationHandle;
global_variable WINDOWPLACEMENT GlobalWindowPosition = { sizeof(GlobalWindowPosition) };
global_variable bool32 GlobalShowCursor = true;
global_variable bool32 GlobalRunning = true;
global_variable LARGE_INTEGER GlobalPerfCountFrequency;

#endif // WIN32_FADO_H