// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_D3D_H
#define FADO_D3D_H

/*
* ** Renderer **
* DX11 requires these pieces regardless of how you structure your code:
*
* D3D — bootstraps the API: device, swap chain, render target, depth buffer, viewport. You always need this.
* Models — vertex buffer + index buffer on the GPU. You need something that puts geometry on the GPU.
* Camera — just math, generates a view matrix. You always need a view matrix.
* Shaders — compiles HLSL, creates the input layout, uploads constants. You always need shaders.
*/


// ───────────
// Includes //
// ───────────
#include <d3d11.h>
#include <directxmath.h>
#include <d3dcompiler.h>
#include "fado_types.h"
#include "fado_ui.h"

// ──────────
// LINKING //
// ──────────
// The first library contains all the Direct3D functionality for setting up and drawing 3D graphics in DirectX 11. 
// The second library contains tools to interface with the hardware on the computer to obtain information about the refresh rate of the monitor, the video card being used, and so forth.
// The third library contains functionality for compiling shaders.
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
// ──────────

typedef DirectX::XMFLOAT2 DXFloat2;
typedef DirectX::XMFLOAT3 DXFloat3;
typedef DirectX::XMFLOAT4 DXFloat4;
typedef DirectX::XMMATRIX DXMatrix;

// ─────────────────────────────────
/// FD3D
// // Core Direct3D 11 objects required for rendering.
// ─────────────────────────────────
struct FD3D
{
	// Matrices:
	DXMatrix				 projectionMatrix;			// Perspective projection for 3D rendering.
	DXMatrix				 worldMatrix;				// Default world transform (identity).
//	DXMatrix					 orthoMatrix;					// Orthographic projection for UI/2D rendering.

	// Device Layer:
	ID3D11Device*			 device;					// Creates all GPU resources.
	ID3D11DeviceContext*	 deviceContext;				// Issues draw calls and binds render state.

	// Presentation:
	IDXGISwapChain*			 swapChain;					// Owns the back buffer and presents frames to the window.
	ID3D11RenderTargetView*  renderTargetView;			// View of the back buffer used as the color render target.

	// Depth Testing:
	ID3D11Texture2D*		 depthStencilBuffer;		// Texture storing depth/stencil data.
	ID3D11DepthStencilState* depthStencilState;			// Controls depth and stencil testing behavior.
	ID3D11DepthStencilView*	 depthStencilView;			// View of the depth buffer bound during rendering.

	// Rasterization:
	ID3D11RasterizerState*	 rasterState;				// Controls culling, fill mode, depth clipping, etc.

	// UI Rendering:
	ID3D11Buffer*			 uiVertexBuffer;			//Dynamic vertex buffer for UI geometry.
	ID3D11BlendState*		 uiBlendState;				// Alpha blending state for UI transparency.
	ID3D11DepthStencilState* uiDepthStencilState;		// Depth-disabled state used when rendering UI.

	// Misc:
	b32						 vsyncEnabled;				// Present synchronized to monitor refresh.
	i32						 videoCardMemory;			// Dedicated GPU memory in MB.
	c8						 videoCardDescription[128];	// GPU name/description.
};

// Used to pack all the required fields to initialize dx11. Used in the platform layer.
struct FD3DInitParams
{
	HWND window;
	i32 screenWidth;
	i32 screenHeight;
	b32 vsync;
	b32 fullScreen;
	f32 screenDepth;
	f32 screenNear;
};

#if FADO_DEBUG
// ─────────────────────────────────
/// Debug lines
#define MAX_DEBUG_LINES 4096

struct FDebugLine
{
	v3    start;
	v3    end;
	v4    color;
};

struct FDebugVertex
{
	v3 position;
};

struct FDebugLineBucket
{
	FDebugLine  lines[MAX_DEBUG_LINES];
	u32         count;

	ID3D11Buffer* vertexBuffer;   // dynamic, re-uploaded each frame
};
#endif // FADO_DEBUG

// ─────────────────────────────────
/// Shaders
// ─────────────────────────────────

struct FMatrixBuffer
{
	DXMatrix world;
	DXMatrix view;
	DXMatrix projection;
};

// ────────────
// Color Shader
// A simple rbga color shader. Unlit.
struct FColorShader
{
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* layout;
	ID3D11Buffer* matrixBuffer;
	ID3D11Buffer* colorBuffer;
};

struct FColorBuffer
{
	DXFloat4 color;
};

// ────────────
// Unlit Texture Shader
struct FUnlitTextureShader
{
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* layout;
	ID3D11Buffer* matrixBuffer;
	ID3D11SamplerState* sampleState;	// This pointer will be used to interface with the texture shader.
};

// ───────────
// Lit Textured Shader (texture shader that also calcules light)
struct FLitTextureShader
{
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* layout;
	ID3D11SamplerState* sampleState;
	ID3D11Buffer* matrixBuffer;
	ID3D11Buffer* lightBuffer;
	DXFloat4 ambientColor;
	DXFloat4 diffuseColor;
	DXFloat3 lightDirection;
};

struct FLitTextureVertex
{
	DXFloat3 position;
	DXFloat3 normal;
	DXFloat2 texture;
};

struct FLightBuffer
{
	DXFloat4 ambientColor;
	DXFloat4 diffuseColor;
	DXFloat3 lightDirection;
	f32 padding;  // Added extra padding so structure is a multiple of 16 for CreateBuffer function requirements.
};

// ───────────
// UI Shader
#define MAX_UI_VERTS (MAX_UI_COMMANDS * 6) // 6 verts per quad (2 tris)

struct FUIShader
{
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* layout;
	ID3D11Buffer* constantBuffer;
	ID3D11SamplerState* samplerState;
};

struct FUIVertex
{
	v2 pos;
	v2 coords;
	v4 color;
};

// ───────────
// Loaded Texturs

// Image load results from custom format ".fasset".
// - Check "fado_asset_format.h" for more details.
struct FImageLoadResult
{
	u32 width;
	u32 height;
	u32 channels;
	u32 format;
	u8* pixels;		// points into the arena, NOT separately allocated.
};

struct FTexture
{
	ID3D11ShaderResourceView* textureView;
	i32 width;
	i32 height;
};

// ─────────────────────────────────
// Mesh / Model

// Raw GPU buffers — internal detail, not a top-level asset handle.
struct FMeshBuffer
{
	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;
	u32 vertexCount;
	u32 indexCount;
	u32 vertexStride;
};

// ──────────────────────────────────
/// Renderer
// ──────────────────────────────────

#define MAX_DRAW_CALLS 265

// Simple (non-model) draw call: one mesh, one texture, one world matrix.
// > TODO: Remove the color from here once we introduce materials.
struct FDrawCall
{
	HMesh hMesh;
	HTexture hTexture;
	DXMatrix worldMatrix;
	DXFloat4 color;
};

// Draw calls are pushed into the bucket and flushed every frame.
struct FRenderBucket
{
	FDrawCall calls[MAX_DRAW_CALLS];
	u32 count;
};

// Aka world
// This struct includes everything that is displayed on the screen,
// shaders, meshes, textures and the camera matrix.
struct FRenderWorld
{
	FD3D d3d;
	DXMatrix cameraView;	// Currenlty only one, will probably replace with a custom camera and different types.
	FSharedStuff* shared;

	// Shaders — one of each, initialized once.
	FColorShader colorShader;
	FUnlitTextureShader unlitTextureShader;
	FLitTextureShader litTextureShader;
	FUIShader uiShader;

	// Render buckets — sorted by shader type, no branching at draw time.
	FRenderBucket colorBucket;
	FRenderBucket unlitTextureBucket;
	FRenderBucket litTextureBucket;
	// ui bucket is in "shared", because the game uses it too.

#if FADO_DEBUG
	FDebugLineBucket debugBucket;
#endif

	// GPU buffer pool (shared by both simple meshes and baked models).
	FMeshBuffer meshes[FMAX_MESHES];
	u32 meshCount;

	// Standalone texture pool (for .fasset images loaded independently).
	FTexture textures[FMAX_TEXTURES];
	u32 texturesCount;
};

// ────────────────────────────────────────────────────────────────
// Public API

void	 InitializeFD3D  (FRenderWorld* world, FD3DInitParams* d3dInitParams);
void	 Render			 (FRenderWorld* world);

// Loaders
HTexture LoadFImage	(FRenderWorld* world, cc8* fileName);
HMesh	 LoadFModel	(FRenderWorld* world, cc8* filename);
HTexture LoadFFont	(FRenderWorld* world, cc8* filename, f32 fontSize, FFont* outFont);
HSound   LoadFSound	(struct FSoundManager* SoundManager, FMemoryArena* permanent, FMemoryArena* scratch, cc8* filename);

// Resize the swap chain buffers, recreate the render target view (depth/stencil buffer and view), update the viewport and projection matrix aspect ratio.
void D3DResize(FRenderWorld* world, i32 width, i32 height, f32 screenNear, f32 screenDepth);

#if FADO_DEBUG
void DebugRender(FRenderWorld* world);
#endif // FADO_DEBUG

#endif	// FADO_D3D_H
