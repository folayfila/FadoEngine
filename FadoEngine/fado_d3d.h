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

/////////////
// Includes //
/////////////
#include <d3d11.h>
#include <directxmath.h>
#include "fado_types.h"

// F:Fado -> FadoD3D
struct fd3d
{
	bool32 vsyncEnabled;
	int32 VideoCardMemory;
	char VideoCardDescription[128];
	IDXGISwapChain* SwapChain;
	ID3D11Device* Device;
	ID3D11DeviceContext* DeviceContext;
	ID3D11RenderTargetView* RenderTargetView;
	ID3D11Texture2D* DepthStencilBuffer;
	ID3D11DepthStencilState* DepthStencilState;
	ID3D11DepthStencilView* DepthStencilView;
	ID3D11RasterizerState* RasterState;
	DirectX::XMMATRIX ProjectionMatrix;
	DirectX::XMMATRIX WorldMatrix;
	DirectX::XMMATRIX OrthoMatrix;
	D3D11_VIEWPORT Viewport;


	bool32 InitializeDirect3D(int32 screenWidth, int32 screenHeight, bool32 vsync,
		HWND Window, bool32 fullScreen, float screenDepth, float screenNear);
	void ShutdownDirect3D();
	void BeginScene(color_rgba color);
	void EndScene();
	void SetBackBufferRenderTarget();
	void ResetViewport();
};


#endif	// FADO_D3D_H
