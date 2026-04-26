#ifndef FADO_D3D_H
#define FADO_D3D_H

/////////////
// LINKING //
/////////////
// The first library contains all the Direct3D functionality for setting up and drawing 3D graphics in DirectX 11. 
// The second library contains tools to interface with the hardware on the computer to obtain information about the refresh rate of the monitor, the video card being used, and so forth.
// The third library contains functionality for compiling shaders which we will cover in the next tutorial.
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "fado_d3d_types.h"

// Only 2 global functions called from the win32 platform layer.
bool32 Initialize(FRenderWorld *application, i32 screenWidth, i32 screenHeight, bool32 vsync, HWND window, bool32 fullScreen, float screenDepth, float screenNear);
bool32 Render(FRenderWorld *application);

#endif	// FADO_D3D_H
