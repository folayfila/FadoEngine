#ifndef FADO_D3D_H
#define FADO_D3D_H

/*
* DX11 requires these pieces regardless of how you structure your code:
*
* D3D — bootstraps the API: device, swap chain, render target, depth buffer, viewport. You always need this.
* Model — vertex buffer + index buffer on the GPU. You need something that puts geometry on the GPU.
* Camera — just math, generates a view matrix. You always need a view matrix.
* Shaders — compiles HLSL, creates the input layout, uploads constants. You always need shaders.
*/


// ───────────
// Includes //
// ───────────
#include <d3d11.h>
#include <directxmath.h>
#include <d3dcompiler.h>
#include <fstream>
#include "fado_types.h"

// ──────────
// LINKING //
// ──────────
// The first library contains all the Direct3D functionality for setting up and drawing 3D graphics in DirectX 11. 
// The second library contains tools to interface with the hardware on the computer to obtain information about the refresh rate of the monitor, the video card being used, and so forth.
// The third library contains functionality for compiling shaders which we will cover in the next tutorial.
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
// ──────────

typedef DirectX::XMMATRIX DXMatrix;
typedef DirectX::XMFLOAT4 DXFloat4;
typedef DirectX::XMFLOAT3 DXFloat3;
typedef DirectX::XMFLOAT2 DXFloat2;

// ─────────────────────────────────
/// FD3D
/// The Direct3D struct is what we will use to invoke our HLSL shaders for drawing the 3D models that are on the GPU.
// ─────────────────────────────────
struct FD3D
{
	DXMatrix projectionMatrix;
	DXMatrix worldMatrix;
	DXMatrix orthoMatrix;
	IDXGISwapChain* swapChain;
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;
	ID3D11RenderTargetView* renderTargetView;
	ID3D11Texture2D* depthStencilBuffer;
	ID3D11DepthStencilState* depthStencilState;
	ID3D11DepthStencilView* depthStencilView;
	ID3D11RasterizerState* rasterState;
	D3D11_VIEWPORT viewport;
	bool32 vsyncEnabled;
	i32 videoCardMemory;
	char videoCardDescription[128];
};

// Used to pack all the required fields to initialize dx11. Used in the platform layer.
struct FD3DInitParams
{
	FD3D* d3d;
	HWND window;
	i32 screenWidth;
	i32 screenHeight;
	bool32 vsync;
	bool32 fullScreen;
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
// Texture Shader
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
// Loaded Texturs

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
/// FCameraD3D
struct FCamera
{
	DXMatrix viewMatrix;
	HTransform hTransform;
};


// ──────────────────────────────────
/// FRenderWorld
/// The appilcaton that holds all d3d required stuff, texturtes and meshes.
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

struct FRenderBucket
{
	FDrawCall calls[MAX_DRAW_CALLS];
	u32 count;
};

struct FRenderWorld
{
	FD3D d3d;
	FCamera camera;
	FMemoryArena* scratchArena;

	// Shaders — one of each, initialized once.
	FColorShader colorShader;
	FUnlitTextureShader unlitTextureShader;
	FLitTextureShader litTextureShader;

	// Render buckets — sorted by shader type, no branching at draw time.
	FRenderBucket colorBucket;
	FRenderBucket unlitTextureBucket;
	FRenderBucket litTextureBucket;

#if FADO_DEBUG
	FDebugLineBucket debugBucket;
#endif

	// GPU buffer pool (shared by both simple meshes and baked models).
	FMeshBuffer meshes[MAX_MESHES];
	u32 meshCount;

	// Standalone texture pool (for .fasset images loaded independently).
	FTexture textures[MAX_TEXTURES];
	u32 texturesCount;
};

// ────────────────────────────────────────────────────────────────
// Public API

bool32	 InitializeFD3D  (FRenderWorld* world, FD3DInitParams* d3dInitParams, FTransformTable* transforms);
void	 Render			 (FRenderWorld* world, FEntityTable* entities, FTransformTable* transforms);
HTexture LoadFImage		 (FRenderWorld* world, const char* fileName);
HMesh	 LoadGLBModel	 (FRenderWorld* world, const char* filename);

// Resize the swap chain buffers, recreate the render target view (and depth/stencil buffer and view), update the viewport and projection matrix aspect ratio.
void D3DResize(FD3D* d3d, i32 width, i32 height, f32 screenNear, f32 screenDepth);

#if FADO_DEBUG
struct FCollisionWorld;
void DebugRender(FRenderWorld* world, FEntityTable* entityTable, FTransformTable* transforms, FCollisionWorld* collisionWorld);
#endif // FADO_DEBUG


#endif	// FADO_D3D_H
