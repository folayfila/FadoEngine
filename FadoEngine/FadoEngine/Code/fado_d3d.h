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
#include "fado_sprite_anim.h"

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
	DXMatrix				 worldMatrix;				// Default world transform (identity).
	// camera and view are in FCamera

	// Device Layer:
	ID3D11Device*			 device;					// Creates all GPU resources.
	ID3D11DeviceContext*	 deviceContext;				// Issues draw calls and binds render state.

	// Presentation:
	IDXGISwapChain*			 swapChain;					// Owns the back buffer and presents frames to the window.
	ID3D11RenderTargetView*  renderTargetView;			// View of the back buffer used as the color render target.

	// Depth Buffer:
	ID3D11Texture2D*		 depthStencilBuffer;		// Texture storing depth/stencil data.
	ID3D11DepthStencilView*	 depthStencilView;			// View of the depth buffer bound during rendering.

	// Depth/stencil states
	ID3D11DepthStencilState* depthStencilState;			// Controls depth and stencil testing behavior.
	ID3D11DepthStencilState* transparentDepthState;		// Depth testing enabled, depth writes disabled for transparent rendering.
	ID3D11DepthStencilState* uiDepthStencilState;		// Depth-disabled state used when rendering UI.

	// Blend states
	ID3D11BlendState*		 opaqueBlendState;			// Solid state, no blending.
	ID3D11BlendState*		 transparentBlendState;		// Alpha blending for transparent materials/UI.

	// Rasterization:
	ID3D11RasterizerState*	 rasterState;				// Controls culling, fill mode, depth clipping, etc.
	ID3D11RasterizerState*	 noCullRasterState;			// No culling. Used to flip sprites.

	// UI Rendering:
	ID3D11Buffer*			 uiVertexBuffer;			//Dynamic vertex buffer for UI geometry.

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
// ─────────────────────────────────
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

// One main shader. It can be colored, tinted with a texture and supports transparency.
struct FMaterialShader
{
	// Compiled shaders.
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;

	// Vertex input layout.
	ID3D11InputLayout* layout;

	// Texture sampling state.
	ID3D11SamplerState* sampleState;

	// GPU constant buffers.
	ID3D11Buffer* matrixBuffer;		// VS b0
	ID3D11Buffer* lightBuffer;		// PS b0
	ID3D11Buffer* materialBuffer;	// PS b1
};

// ────────────
// GPU Constant Buffers
// ────────────

// Constant buffers are uploaded from the CPU and bound to HLSL `cbuffer`s
// (registers b0, b1, etc.). Every constant buffer must have a size that is
// a multiple of 16 bytes to satisfy Direct3D 11 requirements.

// VS b0
struct FMatrixBuffer
{
	DXMatrix world;
	DXMatrix view;
	DXMatrix projection;
};

// PS b0
struct FLightBuffer
{
	DXFloat4 ambientColor;
	DXFloat4 diffuseColor;
	DXFloat3 lightDirection;
	f32 padding; // Pad to 16-byte alignment.
};

// PS b1
struct FMaterialBuffer
{
	DXFloat4 color;		// Material tint.
	b32 hasTexture;		// Sample texture if true.
	b32 isLit;			// Apply lighting if true.
	f32 pad[2];			// Pad to 16-byte alignment.
	v4  spriteRect;		// sprite atlas coords.
};

// ────────────
// CPU-side Shader Data

// Global directional light.
struct FDirectionalLight
{
	DXFloat4 ambientColor;
	DXFloat4 diffuseColor;
	DXFloat3 lightDirection;
};

// Vertex layout.
// Must exactly match:
//   - D3D11_INPUT_ELEMENT_DESC
//   - VertexInput in material.hlsl
struct FTextureVertex
{
	DXFloat3 position;
	DXFloat3 normal;
	DXFloat2 texture;
};

// ─────────────────────────────────
// UI Shader
// ─────────────────────────────────
#define MAX_UI_VERTS (MAX_UI_COMMANDS * 6) // 6 verts per quad (2 tris)

// Simple shader for rendering 2D UI.
//
// Supports:
// - Textured quads
// - Vertex colors
// - Alpha blending
//
// The vertex shader transforms vertices into clip space using a single
// constant buffer. The pixel shader samples a texture and multiplies it
// by the vertex color.

struct FUIShader
{
	// Compiled shaders.
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;

	// Vertex layout.
	ID3D11InputLayout* layout;

	// VS b0 - UI transform/projection data.
	ID3D11Buffer* constantBuffer;

	// Texture sampling state (PS s0).
	ID3D11SamplerState* samplerState;
};

// Vertex layout.
// Must exactly match:
//   - D3D11_INPUT_ELEMENT_DESC
//   - VertexInput in the UI HLSL shader.
struct FUIVertex
{
	v2 pos;		// Screen/local position.
	v2 coords;	// Texture UVs.
	v4 color;	// Vertex tint.
};

// ─────────────────────────────────
// Loaded Texturs
// ─────────────────────────────────

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
// ─────────────────────────────────

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

// Draw call: one mesh, one material, one world matrix.
struct FDrawCall
{
	DXMatrix worldMatrix;
	HMesh hMesh;
	FMaterial material;
	v4 spriteRect;  // used for sprites only. altas coords.
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
	FSharedStuff* shared;

	// Shaders — one of each, initialized once.
	FMaterialShader materialShader;
	FUIShader uiShader;

	FDirectionalLight dirLight;

	// Opaque and Transparent buckets. We draw the opaque bucket first.
	FRenderBucket opaqueBucket;
	FRenderBucket transparentBucket;
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
// ────────────────────────────────────────────────────────────────

void	 InitializeFD3D  (FRenderWorld* world, FD3DInitParams* d3dInitParams);
void	 Render			 (FRenderWorld* world);

// Loaders
HTexture LoadFImage	(FRenderWorld* world, cc8* fileName);
HMesh	 LoadFModel	(FRenderWorld* world, cc8* filename);
HTexture LoadFFont	(FRenderWorld* world, cc8* filename, f32 fontSize, FFont* outFont);
HSound   LoadFSound (struct FSoundManager* SoundManager, FEngineMemory* arena, cc8* filename);

HSpriteSheet RegisterSpriteSheet (FRenderWorld* world, HTexture hTex, u32 frameWidth, u32 frameHeight);

// Generates a simple quad and adds it to world->meshes.
HMesh GetQuad(FRenderWorld* world);

// Generates a simple blob on the XZ plane and adds it to world->meshes.
HMesh GetGroundQuad(FRenderWorld* world);

// Resize the swap chain buffers, recreate the render target view (depth/stencil buffer and view), update the viewport and projection matrix aspect ratio.
void D3DResize(FRenderWorld* world, i32 width, i32 height, f32 screenNear, f32 screenDepth);

#if FADO_DEBUG
void DebugRender(FRenderWorld* world);
#endif // FADO_DEBUG

#endif	// FADO_D3D_H
