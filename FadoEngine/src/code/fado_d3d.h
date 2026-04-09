#ifndef FADO_D3D_H
#define FADO_D3D_H

/*
* DX11 requires these pieces regardless of how you structure your code:
*
* FD3D — bootstraps the API: device, swap chain, render target, depth buffer, viewport. You always need this.
* FModelD3D — vertex buffer + index buffer on the GPU. You need something that puts geometry on the GPU.
* FCameraD3D — just math, generates a view matrix. You always need a view matrix.
* FColorShaderD3D — compiles HLSL, creates the input layout, uploads constants. You always need shaders.
*/

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
#include <d3dcompiler.h>
#include <fstream>
#include "fado_types.h"

////////////////////////////////////
/// FD3D
/// The Direct3D struct is what we will use to invoke our HLSL shaders for drawing the 3D models that are on the GPU.
////////////////////////////////////
struct FD3D
{
	bool32 vsyncEnabled;
	int32 videoCardMemory;
	char videoCardDescription[128];
	IDXGISwapChain* swapChain;
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;
	ID3D11RenderTargetView* renderTargetView;
	ID3D11Texture2D* depthStencilBuffer;
	ID3D11DepthStencilState* depthStencilState;
	ID3D11DepthStencilView* depthStencilView;
	ID3D11RasterizerState* rasterState;
	DirectX::XMMATRIX projectionMatrix;
	DirectX::XMMATRIX worldMatrix;
	DirectX::XMMATRIX orthoMatrix;
	D3D11_VIEWPORT viewport;
};

////////////////////////////////////
/// FColorShader
/// The ColorShader struct is what we will use to invoke our HLSL shaders for drawing the 3D models that are on the GPU.
////////////////////////////////////
struct FMatrixBuffer
{
	DirectX::XMMATRIX world;
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX projection;
};

struct FColorShaderD3D
{
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* layout;
	ID3D11Buffer* matrixBuffer;
};


////////////////////////////////////
/// FModelD3D
/// the Model struct is responsible for encapsulating the geometry for 3D models.
////////////////////////////////////
struct FVertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;
};

struct FModelD3D
{
	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;
	int32 vertexCount;
	int32 indexCount;
};

////////////////////////////////////
/// FCameraD3D
/// The ColorShader is what we will use to invoke our HLSL shaders for drawing the 3D models that are on the GPU.
////////////////////////////////////
struct FCameraD3D
{
	vec3 position;
	vec3 rotation;
	DirectX::XMMATRIX viewMatrix;
};


////////////////////////////////////
/// FApplication
/// The appilcaton containing all required D3D stuff that is used.
////////////////////////////////////
struct FApplication
{
	FD3D direct3D;
	FCameraD3D camera;
	FModelD3D model;
	FColorShaderD3D colorShader;
};

////////////////////////////////////
/// Global Functions
////////////////////////////////////
bool32 Render(FApplication *application);
bool32 Initialize(FApplication *application, int32 screenWidth, int32 screenHeight, bool32 vsync, HWND window, bool32 fullScreen, float screenDepth, float screenNear);

#endif	// FADO_D3D_H
