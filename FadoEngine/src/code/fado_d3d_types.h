#ifndef FADO_D3D_TYPES_H
#define FADO_D3D_TYPES_H

/*
* DX11 requires these pieces regardless of how you structure your code:
*
* FD3D — bootstraps the API: device, swap chain, render target, depth buffer, viewport. You always need this.
* FModelD3D — vertex buffer + index buffer on the GPU. You need something that puts geometry on the GPU.
* FCameraD3D — just math, generates a view matrix. You always need a view matrix.
* FColorShaderD3D — compiles HLSL, creates the input layout, uploads constants. You always need shaders.
*/


/////////////
// Includes //
/////////////
#include <d3d11.h>
#include <directxmath.h>
#include <d3dcompiler.h>
#include <fstream>
#include "fado_types.h"

typedef DirectX::XMMATRIX DXMatrix;
typedef DirectX::XMFLOAT4 DXFloat4;
typedef DirectX::XMFLOAT3 DXFloat3;
typedef DirectX::XMFLOAT2 DXFloat2;

////////////////////////////////////
/// FD3D
/// The Direct3D struct is what we will use to invoke our HLSL shaders for drawing the 3D models that are on the GPU.
////////////////////////////////////
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

////////////////////////////////////
/// Shaders
////////////////////////////////////

struct FMatrixBuffer
{
	DXMatrix world;
	DXMatrix view;
	DXMatrix projection;
};

///////////////
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


///////////////
// Texture Shader
struct FUnlitTextureShader
{
	ID3D11VertexShader* vertexShader;
	ID3D11PixelShader* pixelShader;
	ID3D11InputLayout* layout;
	ID3D11Buffer* matrixBuffer;
	ID3D11SamplerState* sampleState;	// This pointer will be used to interface with the texture shader.
};

///////////////
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

struct FTexture
{
	ID3D11Texture2D* texture;
	ID3D11ShaderResourceView* textureView;
	i32 width;
	i32 height;
	u8* targaData;
};

// > TODO: Replace targa with something more logical :)
#pragma pack(push, 1)
struct FTargaHeader
{
	u8 idLength;         /* 00h  Size of Image ID field */
	u8 colorMapType;     /* 01h  Color map type */
	u8 imageType;        /* 02h  Image type code */
	u16 cMapStart;       /* 03h  Color map origin */
	u16 cMapLength;      /* 05h  Color map length */
	u8 cMapDepth;        /* 07h  Depth of color map entries */
	u16 xOffset;         /* 08h  X origin of image */
	u16 yOffset;         /* 0Ah  Y origin of image */
	u16 width;           /* 0Ch  Width of image */
	u16 height;          /* 0Eh  Height of image */
	u8 pixelDepth;       /* 10h  Image pixel size */
	u8 imageDescriptor;  /* 11h  Image descriptor byte */
};
#pragma pack(pop)

///////////////
// Model
struct FMeshBuffer
{
	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;
	u32 vertexCount;
	u32 indexCount;
	u32 vertexStride;
};

////////////////////////////////////
/// FCameraD3D
struct FCamera
{
	DXMatrix viewMatrix;
	HTransform hTransform;
};


////////////////////////////////////
/// FRenderWorld
/// The appilcaton that holds all d3d required stuff, texturtes and meshes.
////////////////////////////////////

#define MAX_DRAW_CALLS 265

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

	// Mesh and texture pools.
	FMeshBuffer meshes[MAX_MESHES];		// models
	FTexture textures[MAX_TEXTURES];	
	u32 meshCount;
	u32 texturesCount;
};

#endif // !FADO_D3D_TYPES_H
