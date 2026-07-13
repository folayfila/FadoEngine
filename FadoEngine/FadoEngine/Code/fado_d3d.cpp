// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#include "fado_d3d.h"
#include "fado_asset_format.h"
#include "fado_sound.h"
#include "fado_assets.h"
#include "fado_math.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "ThirdParty/stb/stb_truetype.h"
#include "ThirdParty/lz4/lz4.h"

#if FADO_DEBUG
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/backends/imgui_impl_win32.h"
#include "ThirdParty/imgui/backends/imgui_impl_dx11.h"
#endif // FADO_DEBUG

#include <stdio.h>

// ───────────────────────────
// Constants
// ───────────────────────────
// Unified vertex and pixel shaders entry points.
cc8* k_vsEntryFuncName = "VertexShaderEntry";
cc8* k_psEntryFuncName = "PixelShaderEntry";

// ────────────────────────────────────────────────────────────────────────
// mat4 to DXMatrix Helpers
// ────────────────────────────────────────────────────────────────────────
internal inline mat4 DXMatrixToMat4(DXMatrix m)
{
	mat4 result;
	fmemcpy(result.m, &m, sizeof(mat4));
	return result;
}

internal void BuildCameraProjection(FCamera* cam)
{
	switch (cam->type)
	{
	case Camera_Perspective:
	{
		cam->projection = DXMatrixToMat4(DirectX::XMMatrixPerspectiveFovLH(cam->fovY, cam->aspect, cam->nearZ, cam->farZ));
	} break;

	case Camera_Orthographic:
	{
		cam->projection = DXMatrixToMat4(DirectX::XMMatrixOrthographicLH(cam->orthoWidth, cam->orthoHeight, cam->nearZ, cam->farZ));
	} break;
	}
}

// ────────────────────────────────────────────────────────────────────────
// FD3D
// ────────────────────────────────────────────────────────────────────────
internal void InitializeDX11(FD3DInitParams* d3dInitParams, FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;
	FSharedStuff* shared = world->shared;
	FMemoryArena* scratchArena = &world->shared->arena->scratch;

	// Store the vsync setting.
	d3d->vsyncEnabled = d3dInitParams->vsync;

	// Create a DirectX graphics interface factory.
	HRESULT result;
	IDXGIFactory* factory;
	result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	Assert(!FAILED(result));

	// Use the factory to create an adapter for the primary graphics interface (video card).
	IDXGIAdapter* adapter;
	result = factory->EnumAdapters(0, &adapter);
	Assert(!FAILED(result));

	// Enumerate the primary adapter output (monitor).
	IDXGIOutput* adapterOutput;
	result = adapter->EnumOutputs(0, &adapterOutput);
	Assert(!FAILED(result));

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	u32 numModes = 0;
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	Assert(!FAILED(result));

	// Create a list to hold all the possible display modes for this monitor/video card combination.
	DXGI_MODE_DESC* displayModeList;
	displayModeList = ArenaPushArray(scratchArena, DXGI_MODE_DESC, numModes);
	Assert(displayModeList)

	// Now fill the display mode list structures.
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
	Assert(!FAILED(result));

	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	u32 numerator = 0;
	u32 denominator = 0;
	for (u32 i = 0; i < numModes; i++)
	{
		if (displayModeList[i].Width == (u32)d3dInitParams->screenWidth)
		{
			if (displayModeList[i].Height == (u32)d3dInitParams->screenHeight)
			{
				numerator = displayModeList[i].RefreshRate.Numerator;
				denominator = displayModeList[i].RefreshRate.Denominator;
			}
		}
	}

	// Get the adapter (video card) description.
	DXGI_ADAPTER_DESC adapterDesc;
	result = adapter->GetDesc(&adapterDesc);
	Assert(!FAILED(result));

	// Store the dedicated video card memory in megabytes.
	d3d->videoCardMemory = (i32)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	// Convert the name of the video card to a character array and store it.
	u64 stringLength;
	i32 error;
	error = wcstombs_s(&stringLength, d3d->videoCardDescription, 128, adapterDesc.Description, 128);
	Assert(!error);

	// Release the adapter output.
	adapterOutput->Release();
	adapterOutput = 0;

	// Release the adapter.
	adapter->Release();
	adapter = 0;

	// Release the factory.
	factory->Release();
	factory = 0;

	// Release displayModeList by resetting the scratch
	ArenaReset(scratchArena);

	// Initialize the swap chain description.
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};

	// Set to a single back buffer.
	swapChainDesc.BufferCount = 1;

	// Set the width and height of the back buffer.
	swapChainDesc.BufferDesc.Width = d3dInitParams->screenWidth;
	swapChainDesc.BufferDesc.Height = d3dInitParams->screenHeight;

	// Set regular 32-bit surface for the back buffer.
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Set the refresh rate of the back buffer.
	if (d3d->vsyncEnabled)
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
	}
	else
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}

	// Set the usage of the back buffer.
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// Set the handle for the window to render to.
	swapChainDesc.OutputWindow = d3dInitParams->window;

	// Turn multisampling off.
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;

	// Set to full screen or windowed mode.
	swapChainDesc.Windowed = !d3dInitParams->fullScreen;

	// Set the scan line ordering and scaling to unspecified.
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Allow DXGI to cooperate with the window resize more gracefully.
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Set the feature level to DirectX 11.
	D3D_FEATURE_LEVEL featureLevel;
	featureLevel = D3D_FEATURE_LEVEL_11_0;

	// Create the swap chain, Direct3D device, and Direct3D device context.
	result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1,
		D3D11_SDK_VERSION, &swapChainDesc, &d3d->swapChain, &d3d->device, NULL, &d3d->deviceContext);
	Assert(!FAILED(result));

	// Disable DXGI default Alt+Enter fullscreen.
	d3d->swapChain->GetParent(__uuidof(IDXGIFactory), (void**)&factory);
	factory->MakeWindowAssociation(d3dInitParams->window, DXGI_MWA_NO_ALT_ENTER);
	factory->Release();

	// Get the pointer to the back buffer.
	ID3D11Texture2D* backBufferPtr;
	result = d3d->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferPtr);
	Assert(!FAILED(result));

	// Create the render target view with the back buffer pointer.
	result = d3d->device->CreateRenderTargetView(backBufferPtr, NULL, &d3d->renderTargetView);
	Assert(!FAILED(result));

	// Release pointer to the back buffer as we no longer need it.
	backBufferPtr->Release();
	backBufferPtr = 0;

	// Initialize the description of the depth buffer.
	D3D11_TEXTURE2D_DESC depthBufferDesc = {};

	// Set up the description of the depth buffer.
	depthBufferDesc.Width = d3dInitParams->screenWidth;
	depthBufferDesc.Height = d3dInitParams->screenHeight;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.MiscFlags = 0;

	// Create the texture for the depth buffer using the filled out description.
	result = d3d->device->CreateTexture2D(&depthBufferDesc, NULL, &d3d->depthStencilBuffer);
	Assert(!FAILED(result));

	// Initialize the description of the stencil state.
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};

	// Set up the description of the stencil state.
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create the depth stencil state.
	result = d3d->device->CreateDepthStencilState(&depthStencilDesc, &d3d->depthStencilState);
	Assert(!FAILED(result));

	// Set the depth stencil state.
	d3d->deviceContext->OMSetDepthStencilState(d3d->depthStencilState, 1);

	// Initialize the depth stencil view.
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};

	// Set up the depth stencil view description.
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	result = d3d->device->CreateDepthStencilView(d3d->depthStencilBuffer, &depthStencilViewDesc, &d3d->depthStencilView);
	Assert(!FAILED(result));

	// Bind the render target view and depth stencil buffer to the output render pipeline.
	d3d->deviceContext->OMSetRenderTargets(1, &d3d->renderTargetView, d3d->depthStencilView);

	// Setup the raster description which will determine how and what polygons will be drawn.
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state from the description we just filled out.
	result = d3d->device->CreateRasterizerState(&rasterDesc, &d3d->rasterState);
	Assert(!FAILED(result));

	// No cull raster.
	rasterDesc.CullMode = D3D11_CULL_NONE;
	result = d3d->device->CreateRasterizerState(&rasterDesc, &d3d->noCullRasterState);
	Assert(!FAILED(result));

	// Now set the rasterizer state.
	d3d->deviceContext->RSSetState(d3d->rasterState);

	// Create ui vertex buffer and blend state.
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DYNAMIC;
	vbDesc.ByteWidth = sizeof(FUIVertex) * MAX_UI_VERTS;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = d3d->device->CreateBuffer(&vbDesc, nullptr, &d3d->uiVertexBuffer);
	Assert(!FAILED(result));

	// Opaque (default rendering)
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	result = d3d->device->CreateBlendState(&blendDesc, &d3d->opaqueBlendState);
	Assert(SUCCEEDED(result));

	// Alpha blending
	blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	result = d3d->device->CreateBlendState(&blendDesc, &d3d->transparentBlendState);
	Assert(!FAILED(result));

	D3D11_DEPTH_STENCIL_DESC transDepthDesc = {};
	transDepthDesc.DepthEnable = true;
	transDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // read only
	transDepthDesc.DepthFunc = D3D11_COMPARISON_LESS;
	transDepthDesc.StencilEnable = false;
	d3d->device->CreateDepthStencilState(&transDepthDesc, &d3d->transparentDepthState);

	D3D11_DEPTH_STENCIL_DESC uiDepthDesc = {};
	uiDepthDesc.DepthEnable = false;
	uiDepthDesc.StencilEnable = false;
	d3d->device->CreateDepthStencilState(&uiDepthDesc, &d3d->uiDepthStencilState);

	// Setup the viewport for rendering.
	shared->viewport.topLeftX = 0.0f;
	shared->viewport.topLeftY = 0.0f;
	shared->viewport.width = (f32)d3dInitParams->screenWidth;
	shared->viewport.height = (f32)d3dInitParams->screenHeight;
	shared->viewport.minDepth = 0.0f;
	shared->viewport.maxDepth = 1.0f;

	D3D11_VIEWPORT d3d11Viewport = {
		shared->viewport.topLeftX,
		shared->viewport.topLeftY,
		shared->viewport.width,
		shared->viewport.height,
		shared->viewport.minDepth,
		shared->viewport.maxDepth
	};

	// Create the viewport.
	d3d->deviceContext->RSSetViewports(1, &d3d11Viewport);

	// Setup the projection matrix.
	f32 fieldOfView = Pi32 / 4.0f;
	f32 screenAspect = (f32)d3dInitParams->screenWidth / (f32)d3dInitParams->screenHeight;

	// Create the projection matrix for 3D rendering.
	FCamera* cam = &shared->camera;
	cam->type = Camera_Perspective;
	cam->fovY = fieldOfView;
	cam->aspect = screenAspect;
	cam->orthoWidth = 20.0f;
	cam->orthoHeight = 11.25f;
	cam->nearZ = d3dInitParams->screenNear;
	cam->farZ = d3dInitParams->screenDepth;
	BuildCameraProjection(cam);

	// Initialize the world matrix to the identity matrix.
	d3d->worldMatrix = DirectX::XMMatrixIdentity();
}

// Clear the back and depth buffer.
internal void BeginScene(FD3D *d3d, v4 color)
{
	d3d->deviceContext->ClearRenderTargetView(d3d->renderTargetView, color.e);
	d3d->deviceContext->ClearDepthStencilView(d3d->depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

// Present the back buffer to the screen since rendering is complete.
internal void EndScene(FD3D* d3d)
{
#if FADO_DEBUG
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif // FADO_DEBUG

	// SyncInterval:
	// - 1: Lock to screen refresh rate.
	// - 0: Present as fast as possible.
	d3d->swapChain->Present(d3d->vsyncEnabled ? 1 : 0, 0);
}

// ────────────────────────────────────────────────────────────────────────
// Shaders
// ────────────────────────────────────────────────────────────────────────
internal void LoadAndCompileShader(ID3D11Device* device, wchar* hlslFileName, ID3D10Blob*& vertexShaderBuffer, ID3D10Blob*& pixelShaderBuffer,
	ID3D11VertexShader*& vertexShader, ID3D11PixelShader*& pixelShader)
{
	// Set the filename of the hlsl shader.
	Assert(GetFileAttributesW(hlslFileName) != INVALID_FILE_ATTRIBUTES);

	// Compile the vertex shader code.
	ID3D10Blob* errorMessage = nullptr;
	HRESULT result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_vsEntryFuncName, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&vertexShaderBuffer, &errorMessage);
	Assert(!FAILED(result));

	// Compile the pixel shader code.
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_psEntryFuncName, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&pixelShaderBuffer, &errorMessage);
	Assert(!FAILED(result));

	// Create the vertex shader from the buffer.
	result = device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &vertexShader);
	Assert(!FAILED(result));

	// Create the pixel shader from the buffer.
	result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &pixelShader);
	Assert(!FAILED(result));
}

internal FMatrixBuffer GetShadersTransposeMatrices(FRenderWorld* world)
{
	FCamera* cam = &world->shared->camera;

	FMatrixBuffer mat = {};
	mat.world = DirectX::XMMatrixTranspose(world->d3d.worldMatrix);
	mat.view =  DirectX::XMMatrixTranspose((DXMatrix )cam->view.m);
	mat.projection = DirectX::XMMatrixTranspose((DXMatrix)cam->projection.m);
	return mat;
}

// ────────────────────────────────────────────────────────────────────────
// FMaterialShader
// ────────────────────────────────────────────────────────────────────────
internal void InitializeMaterialShader(FMaterialShader* shader, ID3D11Device* device)
{
	wchar hlslFileName[FMAX_PATH] = { L"Shaders\\material.hlsl" };
	ID3D10Blob* vsBuffer = nullptr;
	ID3D10Blob* psBuffer = nullptr;
	LoadAndCompileShader(device, hlslFileName, vsBuffer, psBuffer,
		shader->vertexShader, shader->pixelShader);

	D3D11_INPUT_ELEMENT_DESC layout[3];
	layout[0].SemanticName = "POSITION";
	layout[0].SemanticIndex = 0;
	layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	layout[0].InputSlot = 0;
	layout[0].AlignedByteOffset = 0;
	layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	layout[0].InstanceDataStepRate = 0;
	
	layout[1].SemanticName = "NORMAL";
	layout[1].SemanticIndex = 0;
	layout[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	layout[1].InputSlot = 0;
	layout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	layout[1].InstanceDataStepRate = 0;
	
	layout[2].SemanticName = "TEXCOORD";
	layout[2].SemanticIndex = 0;
	layout[2].Format = DXGI_FORMAT_R32G32_FLOAT;
	layout[2].InputSlot = 0;
	layout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	layout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	layout[2].InstanceDataStepRate = 0;

	u32 numElements = ArrayCount(layout);
	HRESULT result = device->CreateInputLayout(layout, numElements, vsBuffer->GetBufferPointer(), vsBuffer->GetBufferSize(), &shader->layout);
	Assert(!FAILED(result));

	vsBuffer->Release();
	psBuffer->Release();

	// Registers
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	bufferDesc.ByteWidth = sizeof(FMatrixBuffer);
	result = device->CreateBuffer(&bufferDesc, NULL, &shader->matrixBuffer);

	bufferDesc.ByteWidth = sizeof(FLightBuffer);
	result = device->CreateBuffer(&bufferDesc, NULL, &shader->lightBuffer);

	bufferDesc.ByteWidth = sizeof(FMaterialBuffer);
	result = device->CreateBuffer(&bufferDesc, NULL, &shader->materialBuffer);

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.MaxAnisotropy = 8;
	samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	device->CreateSamplerState(&samplerDesc, &shader->sampleState);
}

internal void SetMaterialShaderParameters(FRenderWorld* world, FDrawCall* call)
{
	ID3D11DeviceContext* deviceContext = world->d3d.deviceContext;
	FMaterialShader* shader = &world->materialShader;

	D3D11_MAPPED_SUBRESOURCE mapped;

	// b0 — matrices
	deviceContext->Map(shader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	FMatrixBuffer* matrixBuffer = (FMatrixBuffer*)mapped.pData;
	*matrixBuffer = GetShadersTransposeMatrices(world);

	deviceContext->Unmap(shader->matrixBuffer, 0);
	deviceContext->VSSetConstantBuffers(0, 1, &shader->matrixBuffer);

	// b0 — light (PS)
	deviceContext->Map(shader->lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	FLightBuffer* lightBuffer = (FLightBuffer*)mapped.pData;
	lightBuffer->ambientColor = world->dirLight.ambientColor;
	lightBuffer->diffuseColor = world->dirLight.diffuseColor;
	lightBuffer->lightDirection = world->dirLight.lightDirection;
	lightBuffer->padding = 0.0f;

	deviceContext->Unmap(shader->lightBuffer, 0);
	deviceContext->PSSetConstantBuffers(0, 1, &shader->lightBuffer);

	// b1 — material (PS)
	FMaterial* mat = &call->material;

	deviceContext->Map(shader->materialBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	FMaterialBuffer* matBuffer = (FMaterialBuffer*)mapped.pData;
	matBuffer->color = { mat->color.x, mat->color.y, mat->color.z, mat->color.a };
	matBuffer->hasTexture = (mat->texture != INVALID_HANDLE) ? 1 : 0;
	matBuffer->isLit = mat->flags & Material_Lit;
	matBuffer->pad[0] = matBuffer->pad[1] = 0.0f;
	matBuffer->spriteRect = call->spriteRect;

	deviceContext->Unmap(shader->materialBuffer, 0);
	deviceContext->PSSetConstantBuffers(1, 1, &shader->materialBuffer);

	// Texture — bind white texture if none
	HTexture whiteTex = world->shared->assets->hWhiteTexture;
	HTexture hTex = mat->texture == INVALID_HANDLE ? whiteTex : mat->texture;
	deviceContext->PSSetShaderResources(0, 1, &world->textures[hTex].textureView);
}

// ────────────────────────────────────────────────────────────────────────
// FUIShader
// ────────────────────────────────────────────────────────────────────────
internal void InitializeUIShader(FUIShader* uiShader, ID3D11Device* device, HWND window)
{
	// Compile the shader code.
	wchar hlslFileName[FMAX_PATH] = { L"Shaders\\ui.hlsl" };
	ID3D10Blob* vertexShaderBuffer = nullptr;
	ID3D10Blob* pixelShaderBuffer = nullptr;
	LoadAndCompileShader(device, hlslFileName, vertexShaderBuffer, pixelShaderBuffer, uiShader->vertexShader, uiShader->pixelShader);

	// Create the vertex input layout description.
	// This setup needs to match the VertexType stucture in the model and in the shader.
	D3D11_INPUT_ELEMENT_DESC polygonLayout[3];
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "TEXCOORD";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = 8;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	polygonLayout[2].SemanticName = "COLOR";
	polygonLayout[2].SemanticIndex = 0;
	polygonLayout[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polygonLayout[2].InputSlot = 0;
	polygonLayout[2].AlignedByteOffset = 16;
	polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[2].InstanceDataStepRate = 0;

	// Create the vertex input layout.
	u32 numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);
	HRESULT result = device->CreateInputLayout(polygonLayout, numElements, vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &uiShader->layout);
	Assert(!FAILED(result));

	// Release the vertex shader buffer and pixel shader buffer since they are no longer needed.
	vertexShaderBuffer->Release();
	vertexShaderBuffer = 0;

	pixelShaderBuffer->Release();
	pixelShaderBuffer = 0;

	// Setup the description of the constant buffer.
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.ByteWidth = sizeof(mat4/*MatrixBuffer*/);
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = device->CreateBuffer(&cbDesc, nullptr, &uiShader->constantBuffer);
	Assert(!FAILED(result));

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	result = device->CreateSamplerState(&samplerDesc, &uiShader->samplerState);
	Assert(!FAILED(result));
}

internal void MakeUIOrtho(mat4* out, f32 left, f32 right, f32 top, f32 bottom)
{
	// Scale screen coordinates into clip space.
	out->e[0][0] = 2.0f / (right - left);
	out->e[1][1] = 2.0f / (top - bottom);  // flipped: y=0 top

	// Map depth from [0, 1] into clip space.
	out->e[2][2] = 0.5f;

	// Translate screen origin into clip space.
	out->e[3][0] = -(right + left) / (right - left);
	out->e[3][1] = -(top + bottom) / (top - bottom);
	out->e[3][2] = 0.5f;
	out->e[3][3] = 1.0f;
}

internal void SetUIProjection(FRenderWorld* world)
{
	// Create a projection where (0,0) is the top-left of the viewport. (MatrixBuffer).
	mat4 uiProjection = {};
	MakeUIOrtho(&uiProjection, 0.0f, world->shared->viewport.width, 0.0f, world->shared->viewport.height);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT result = world->d3d.deviceContext->Map(world->uiShader.constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	Assert(!FAILED(result));

	fmemcpy(mapped.pData, &uiProjection, sizeof(uiProjection));
	world->d3d.deviceContext->Unmap(world->uiShader.constantBuffer, 0);
}

internal void PushQuad(FUIVertex* verts, u32* count, v4 rect, v4 coords, v4 color)
{
	f32 w = rect.width;
	f32 h = rect.height;
	// triangle 1
	verts[(*count)++] = { rect.x,		rect.y,		  coords.u0, coords.v0, color.r, color.g, color.b, color.a };
	verts[(*count)++] = { (rect.x + w), rect.y,		  coords.u1, coords.v0, color.r, color.g, color.b, color.a };
	verts[(*count)++] = { (rect.x + w), (rect.y + h), coords.u1, coords.v1, color.r, color.g, color.b, color.a };
	// triangle 2
	verts[(*count)++] = { rect.x,	    rect.y,		  coords.u0, coords.v0, color.r, color.g, color.b, color.a };
	verts[(*count)++] = { (rect.x + w), (rect.y + h), coords.u1, coords.v1, color.r, color.g, color.b, color.a };
	verts[(*count)++] = { rect.x,	    (rect.y + h), coords.u0, coords.v1, color.r, color.g, color.b, color.a };
}

internal HTexture CreateTextureFromPixels(FRenderWorld* world, void* pixels, i32 width, i32 height)
{
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = pixels;
	data.SysMemPitch = width * 4;
	data.SysMemSlicePitch = 0;

	ID3D11Texture2D* tex;
	HRESULT result = world->d3d.device->CreateTexture2D(&texDesc, &data, &tex);
	Assert(!FAILED(result));

	ID3D11ShaderResourceView* srv;
	result = world->d3d.device->CreateShaderResourceView(tex, nullptr, &srv);
	Assert(!FAILED(result));
	tex->Release();

	Assert(world->texturesCount < FMAX_TEXTURES);
	HTexture handle = world->texturesCount++;
	world->textures[handle].textureView = srv;
	world->textures[handle].width = width;
	world->textures[handle].height = height;

	return handle;
}

// ────────────────────────────────────────────────────────────────────────
// Model
// ────────────────────────────────────────────────────────────────────────

internal void UploadMesh(FMeshBuffer *mesh, ID3D11Device* device, void* vertices, u32 vCount, u32 vertexStride, u32* indices, u32 iCount)
{
	mesh->vertexCount = vCount;
	mesh->indexCount = iCount;
	mesh->vertexStride = vertexStride;

	// Set up the description of the static vertex buffer.
	D3D11_BUFFER_DESC vertexBufferDesc;
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = vertexStride * vCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	D3D11_SUBRESOURCE_DATA vertexData;
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now create the vertex buffer.
	HRESULT result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &mesh->vertexBuffer);
	Assert(!FAILED(result));

	// Set up the description of the static index buffer.
	D3D11_BUFFER_DESC indexBufferDesc;
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(u32) * iCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	D3D11_SUBRESOURCE_DATA indexData;
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	// Create the index buffer.
	result = device->CreateBuffer(&indexBufferDesc, &indexData, &mesh->indexBuffer);
	Assert(!FAILED(result));
}

internal void RenderMesh(FMeshBuffer* mesh, ID3D11DeviceContext* deviceContext)
{
	// Set the vertex buffer to active in the input assembler so it can be rendered.
	u32 offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &mesh->vertexBuffer, &mesh->vertexStride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetIndexBuffer(mesh->indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ────────────────────────────────────────────────────────────────────────
// Draw call helpers
// ────────────────────────────────────────────────────────────────────────
// Add a draw call to the passed bucket.
internal void PushDrawCall(FRenderWorld* world, DXMatrix worldMatrix, HMesh hMesh, FMaterial material, v4 spriteRect)
{
	// Push to right bucket based on the material color's alpha channel.
	FRenderBucket* bucket = &world->opaqueBucket;
	if (material.color.a < 1.0f || (material.flags & Material_Transparent))
	{
		bucket = &world->transparentBucket;
	}

	Assert(bucket->count < MAX_DRAW_CALLS);
	FDrawCall* call = &bucket->calls[bucket->count++];
	call->worldMatrix = worldMatrix;
	call->hMesh = hMesh;
	call->material = material;
	call->spriteRect = spriteRect;
}

internal void PushBlobShadow2D(FRenderWorld* world, v3 entityPos, v2 size = V2One())
{
	// Same quad, same billboard orientation as your sprites — just offset and squashed.
	DXMatrix scale = DirectX::XMMatrixScaling(size.x, size.y * 0.4f, 1.0f); // flattened ellipse
	DXMatrix translate = DirectX::XMMatrixTranslation(entityPos.x, entityPos.y - size.y * 0.5f, entityPos.z + 0.01f);
	DXMatrix worldMatrix = scale * translate;

	FAssetsHandles* assets = world->shared->assets;

	FMaterial mat = {};
	mat.color = { 0.0f, 0.0f, 0.0f, 0.4f };
	mat.texture = assets->hShadowTexture;
	mat.flags = Material_Transparent;

	PushDrawCall(world, worldMatrix, assets->hQuadMesh, mat, v4{0,0,1,1});
}

internal void PushBlobShadow(FRenderWorld* world, v3 entityPos, f32 radius = 2.0f, f32 groundY = 0.0f)
{
	f32 heightFactor = Max(0.25f, (1.0f - (entityPos.y * 0.01f)));
	radius *= heightFactor;

	// Flatten to ground level, slightly above the floor to avoid z-fighting.
	DXMatrix scale = DirectX::XMMatrixScaling(radius, 1.0f, radius);
	DXMatrix translate = DirectX::XMMatrixTranslation(entityPos.x, groundY + 0.01f, entityPos.z);
	DXMatrix worldMatrix = scale * translate;

	FAssetsHandles* assets = world->shared->assets;

	FMaterial mat = {};
	mat.color = { 0.0f, 0.0f, 0.0f, 0.5f };   // black, semi-transparent
	mat.texture = assets->hShadowTexture;
	mat.flags = Material_Transparent;

	PushDrawCall(world, worldMatrix, assets->hGroundQuad, mat, v4{0,0,1,1} /* full texture, no atlas */);
}

// ────────────────────────────────────────────────────────────────────────
//  Buckets
// ────────────────────────────────────────────────────────────────────────

// Opaque buckets
internal void FlushOpaqueBucket(FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;
	d3d->deviceContext->RSSetState(d3d->rasterState);


	for (u32 i = 0; i < world->opaqueBucket.count; i++)
	{
		FDrawCall* call = &world->opaqueBucket.calls[i];
		d3d->worldMatrix = call->worldMatrix;
		SetMaterialShaderParameters(world, call);
		FMeshBuffer* mesh = &world->meshes[call->hMesh];
		RenderMesh(mesh, d3d->deviceContext);
		d3d->deviceContext->DrawIndexed(mesh->indexCount, 0, 0);
	}
	world->opaqueBucket.count = 0;
}

// ────────────────────────────────────────────────────────────────────────
// Transparent buckets
// ────────────────────────────────────────────────────────────────────────
internal void FlushTransparentBucket(FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;
	d3d->deviceContext->RSSetState(d3d->noCullRasterState);

	for (u32 i = 0; i < world->transparentBucket.count; i++)
	{
		FDrawCall* call = &world->transparentBucket.calls[i];
		d3d->worldMatrix = call->worldMatrix;
		SetMaterialShaderParameters(world, call);
		FMeshBuffer* mesh = &world->meshes[call->hMesh];
		RenderMesh(mesh, d3d->deviceContext);
		d3d->deviceContext->DrawIndexed(mesh->indexCount, 0, 0);
	}
	world->transparentBucket.count = 0;
}

// ────────────────────────────────────────────────────────────────────────
// UI Bucket
// 
// Draws all queued UI commands (rects + text-as-rects) for this frame.
// - UI has no concept of depth: layering is purely by submission order (first pushed = bottom, last pushed = top),
//   so depth testing is disabled for this entire pass. Without this, UI quads would be
//   depth-tested against each other (and the 3D scene) and incorrectly discarded/hidden, since most UI sits at the same Z.
// - Alpha blending is enabled so partially-transparent quads composite correctly over whatever was drawn before them in this same pass.
// ────────────────────────────────────────────────────────────────────────
internal void FlushUIBucket(FRenderWorld* world)
{
	FUICommandBucket* bucket = world->shared->uiCommands;
	if (bucket->count == 0)
	{
		return;
	}

	FUIVertex* verts = ArenaPushArray(&world->shared->arena->scratch, FUIVertex, MAX_UI_VERTS);
	u32 vertCount = 0;

	// Set pipeline state once
	ID3D11DeviceContext* deviceContext = world->d3d.deviceContext;
	u32 stride = sizeof(FUIVertex), offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &world->d3d.uiVertexBuffer, &stride, &offset);
	deviceContext->IASetInputLayout(world->uiShader.layout);
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->VSSetShader(world->uiShader.vertexShader, nullptr, 0);
	deviceContext->PSSetShader(world->uiShader.pixelShader, nullptr, 0);
	deviceContext->VSSetConstantBuffers(0, 1, &world->uiShader.constantBuffer);

	f32 blendFactor[4] = { 0, 0, 0, 0 };
	deviceContext->OMSetBlendState(world->d3d.transparentBlendState, blendFactor, 0xFFFFFFFF);
	deviceContext->OMSetDepthStencilState(world->d3d.uiDepthStencilState, 0);

	// Draw per texture group
	ID3D11ShaderResourceView* currentTexture = nullptr;

	for (u32 i = 0; i < bucket->count; ++i)
	{
		FUICommand* cmd = &bucket->commands[i];

		ID3D11ShaderResourceView* cmdTexture = nullptr;
		v4 rect, coords, color;

		switch (cmd->type)
		{
		case UICommand_Rect:
		{
			cmdTexture = world->textures[cmd->rect.hTexture].textureView;
			rect = cmd->rect.rect;
			coords = cmd->rect.coords;
			color = cmd->rect.color;
		} break;

		default:
		{ continue; }
		}

		// Flush if texture changes.
		if ((cmdTexture != currentTexture) && (vertCount > 0))
		{
			D3D11_MAPPED_SUBRESOURCE mapped = {};
			deviceContext->Map(world->d3d.uiVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			fmemcpy(mapped.pData, verts, sizeof(FUIVertex) * vertCount);
			deviceContext->Unmap(world->d3d.uiVertexBuffer, 0);
			deviceContext->PSSetShaderResources(0, 1, &currentTexture);
			deviceContext->Draw(vertCount, 0);
			vertCount = 0;
		}

		currentTexture = cmdTexture;
		PushQuad(verts, &vertCount, rect, coords, color);
	}

	// Final flush
	if (vertCount > 0)
	{
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		deviceContext->Map(world->d3d.uiVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		fmemcpy(mapped.pData, verts, sizeof(FUIVertex) * vertCount);
		deviceContext->Unmap(world->d3d.uiVertexBuffer, 0);
		deviceContext->PSSetShaderResources(0, 1, &currentTexture);
		deviceContext->Draw(vertCount, 0);
	}

	deviceContext->OMSetDepthStencilState(world->d3d.depthStencilState, 1);
	deviceContext->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);

	bucket->count = 0;
	ArenaReset(&world->shared->arena->scratch);
}

// ────────────────────────────────────────────────────────────────────────
// Flush all buckets
// ────────────────────────────────────────────────────────────────────────
internal void FlushBuckets(FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;
	ID3D11DeviceContext* deviceContext = d3d->deviceContext;
	FMaterialShader* shader = &world->materialShader;

	deviceContext->IASetInputLayout(shader->layout);
	deviceContext->VSSetShader(shader->vertexShader, NULL, 0);
	deviceContext->PSSetShader(shader->pixelShader, NULL, 0);
	deviceContext->PSSetSamplers(0, 1, &shader->sampleState);

	// 1. Opaque 3D
	f32 blendFactor[4] = { 0, 0, 0, 0 };
	deviceContext->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF); // blending off
	deviceContext->OMSetDepthStencilState(d3d->depthStencilState, 1); // depth on
	FlushOpaqueBucket(world);

	// 2. Transparent 3D — reuse uiBlendState, keep depth READ on, WRITE off
	deviceContext->OMSetBlendState(d3d->transparentBlendState, blendFactor, 0xFFFFFFFF);
	deviceContext->OMSetDepthStencilState(d3d->transparentDepthState, 1);
	FlushTransparentBucket(world);

	// 3. UI
	FlushUIBucket(world);
}

// ────────────────────────────────────────────────────────────────────────
// FCamera
// ────────────────────────────────────────────────────────────────────────
// Updates the camera view matrix and the shared FCamera.
internal void RenderCamera(FRenderWorld* world)
{
	FSharedStuff* shared = world->shared;
	quat rot = shared->transforms->rotations[shared->camera.handle];

	// Update shared CameraView.
	FCamera* cam = &shared->camera;
	cam->forward = QuatForward(rot);
	cam->right = QuatRight(rot);
	cam->up = QuatUp(rot);

	// Load quaternion directly into DirectXMath.
	DirectX::XMVECTOR quatVector = DirectX::XMVectorSet(rot.x, rot.y, rot.z, rot.w);

	// Build rotation matrix from quaternion.
	DXMatrix rotationMatrix = DirectX::XMMatrixRotationQuaternion(quatVector);

	// Position
	v3 pos = shared->transforms->positions[shared->camera.handle];
	DirectX::XMVECTOR positionVector = DirectX::XMVectorSet(pos.x, pos.y, pos.z, 0.0f);

	// Default forward, up, right, rotated by the quaternion matrix.
	DirectX::XMVECTOR forwardVector = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	DirectX::XMVECTOR upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMVECTOR rightVector = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

	forwardVector = DirectX::XMVector3TransformCoord(forwardVector, rotationMatrix);
	upVector = DirectX::XMVector3TransformCoord(upVector, rotationMatrix);
	rightVector = DirectX::XMVector3TransformCoord(rightVector, rotationMatrix);

	DirectX::XMVECTOR lookAtVector = DirectX::XMVectorAdd(positionVector, forwardVector);

	DXMatrix camMatrix = DirectX::XMMatrixLookAtLH(positionVector, lookAtVector, upVector);
	cam->view = DXMatrixToMat4(camMatrix);
	BuildCameraProjection(cam);
}

// Returns a DXMatrix by building it from the Entity's transform.
internal DXMatrix BuildEntityWorldMatrix(HEntity entityID, FSharedStuff* shared)
{
	FEntity* entity = &shared->entityTable->entities[entityID];
	FTransforms* transforms = shared->transforms;

	v3 scale = transforms->scales[entityID];
	DXMatrix scaleMatrix = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);

	quat rot = transforms->rotations[entityID];
	DirectX::XMVECTOR quatVector = DirectX::XMVectorSet(rot.x, rot.y, rot.z, rot.w);
	DXMatrix rotMatrix = DirectX::XMMatrixRotationQuaternion(quatVector);

	v3 pos = transforms->positions[entityID];
	DXMatrix transMatrix = DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

	DXMatrix resultMatrix = DirectX::XMMatrixMultiply(scaleMatrix, rotMatrix);
	resultMatrix = DirectX::XMMatrixMultiply(resultMatrix, transMatrix);
	return resultMatrix;
}

// ────────────────────────────────────────────────────────────────────────
// Texture loader — .fasset image
// ────────────────────────────────────────────────────────────────────────
HTexture LoadFImage(FRenderWorld* world, cc8* fileName)
{
	FILE* file = nullptr;
	fopen_s(&file, fileName, "rb");
	Assert(file);

	FAssetHeader assetHeader = {};
	fread(&assetHeader, sizeof(assetHeader), 1, file);
	Assert((assetHeader.magic == FASSET_MAGIC) && (assetHeader.assetType == FASSET_TYPE_IMAGE));

	FImageHeader header = {};
	fread(&header, sizeof(header), 1, file);

	u32* mipOffsets = nullptr;
	u32* mipSizes = nullptr;

	if (header.format != FIMAGE_FORMAT_RGBA8)
	{
		mipOffsets = ArenaPushArray(&world->shared->arena->scratch, u32, header.mipCount);
		mipSizes = ArenaPushArray(&world->shared->arena->scratch, u32, header.mipCount);

		fread(mipOffsets, sizeof(u32), header.mipCount, file);
		fread(mipSizes, sizeof(u32), header.mipCount, file);
	}

	// Read compressed data
	u8* compressedData = ArenaPushArray(&world->shared->arena->scratch, u8, header.dataSize);
	fread(compressedData, header.dataSize, 1, file);
	fclose(file);

	// Decompress
	u8* imageData = ArenaPushArray(&world->shared->arena->scratch, u8, header.uncompressedSize);
	LZ4_decompress_safe((cc8*)compressedData, (c8*)imageData, (i32)header.dataSize, (i32)header.uncompressedSize);

	DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
	switch (header.format)
	{
	case FIMAGE_FORMAT_BC1:
		dxgiFormat = DXGI_FORMAT_BC1_UNORM;
		break;

	case FIMAGE_FORMAT_BC3:
		dxgiFormat = DXGI_FORMAT_BC3_UNORM;
		break;

	case FIMAGE_FORMAT_RGBA8:
		dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;

	default:
		Assert(false);
	}

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = header.width;
	texDesc.Height = header.height;
	texDesc.MipLevels = header.mipCount;
	texDesc.ArraySize = 1;
	texDesc.Format = dxgiFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA* subresources = ArenaPushArray(&world->shared->arena->scratch, D3D11_SUBRESOURCE_DATA, header.mipCount);

	if (header.format == FIMAGE_FORMAT_RGBA8)
	{
		subresources[0].pSysMem = imageData;
		subresources[0].SysMemPitch = header.width * 4;
		subresources[0].SysMemSlicePitch = 0;
	}
	else
	{
		u32 mipWidth = header.width;
		u32 mipHeight = header.height;

		for (u32 mip = 0; mip < header.mipCount; ++mip)
		{
			subresources[mip].pSysMem = imageData + mipOffsets[mip];
			subresources[mip].SysMemSlicePitch = 0;

			if (header.format == FIMAGE_FORMAT_BC3)
			{
				subresources[mip].SysMemPitch = ((mipWidth + 3) / 4) * 16;
			}
			else // BC1
			{
				subresources[mip].SysMemPitch = ((mipWidth + 3) / 4) * 8;
			}

			if (mipWidth > 1) mipWidth >>= 1;
			if (mipHeight > 1) mipHeight >>= 1;
		}
	}

	ID3D11Texture2D* tex = nullptr;
	HRESULT result = world->d3d.device->CreateTexture2D(&texDesc, subresources, &tex);
	Assert(!FAILED(result));

	ID3D11ShaderResourceView* srv = nullptr;
	result = world->d3d.device->CreateShaderResourceView(tex, nullptr, &srv);
	Assert(!FAILED(result));

	tex->Release();

	Assert(world->texturesCount < FMAX_TEXTURES);

	HTexture handle = world->texturesCount++;
	world->textures[handle].textureView = srv;
	world->textures[handle].width = (i32)header.width;
	world->textures[handle].height = (i32)header.height;

	ArenaReset(&world->shared->arena->scratch);

	return handle;
}

// ────────────────────────────────────────────────────────────────────────
// Model loader — .fmodel
// ────────────────────────────────────────────────────────────────────────
HMesh LoadFModel(FRenderWorld* world, cc8* fileName)
{
	FILE* file = nullptr;
	fopen_s(&file, fileName, "rb");
	if (!file)
	{
		return INVALID_HANDLE;
	}

	FAssetHeader header = {};
	fread(&header, sizeof(header), 1, file);
	Assert(header.magic == FASSET_MAGIC);
	Assert(header.assetType == FASSET_TYPE_MODEL);

	FModelHeader modelHeader = {};
	fread(&modelHeader, sizeof(modelHeader), 1, file);

	FMeshDesc* descs = ArenaPushArray(&world->shared->arena->scratch, FMeshDesc, modelHeader.meshCount);
	fread(descs, sizeof(FMeshDesc), modelHeader.meshCount, file);

	// Read + decompress vertex blob
	u8* vbCompressed = ArenaPushSize(&world->shared->arena->scratch, u8, modelHeader.vertexDataSize);
	fread(vbCompressed, 1, modelHeader.vertexDataSize, file);
	u8* vbData = ArenaPushSize(&world->shared->arena->scratch, u8, modelHeader.vertexDataUncompressed);
	LZ4_decompress_safe((const char*)vbCompressed, (c8*)vbData,
		(i32)modelHeader.vertexDataSize, (i32)modelHeader.vertexDataUncompressed);

	// Read + decompress index blob
	u8* ibCompressed = ArenaPushSize(&world->shared->arena->scratch, u8, modelHeader.indexDataSize);
	fread(ibCompressed, 1, modelHeader.indexDataSize, file);
	u8* ibData = ArenaPushSize(&world->shared->arena->scratch, u8, modelHeader.indexDataUncompressed);
	LZ4_decompress_safe((const char*)ibCompressed, (char*)ibData,
		(i32)modelHeader.indexDataSize, (i32)modelHeader.indexDataUncompressed);

	fclose(file);

	u32 handle = world->meshCount;
	Assert(world->meshCount < FMAX_MESHES);

	for (u32 i = 0; i < modelHeader.meshCount; i++)
	{
		FMeshDesc* desc = &descs[i];
		FTextureVertex* verts = (FTextureVertex*)(vbData + desc->vertexOffset);
		u32* indices = (u32*)(ibData + desc->indexOffset);

		FMeshBuffer* mesh = &world->meshes[world->meshCount++];
		mesh->vertexStride = sizeof(FTextureVertex);
		UploadMesh(mesh, world->d3d.device, verts, desc->vertexCount,
			mesh->vertexStride, indices, desc->indexCount);
	}

	ArenaReset(&world->shared->arena->scratch);
	return handle;
}

// ────────────────────────────────────────────────────────────────────────
// Font loader — .ffont
// ────────────────────────────────────────────────────────────────────────
HTexture LoadFFont(FRenderWorld* world, cc8* filename, f32 fontSize, FFont* outFont)
{
	FILE* file = nullptr;
	fopen_s(&file, filename, "rb");
	if (!file)
	{
		return false;
	}

	FAssetHeader header = {};
	fread(&header, sizeof(header), 1, file);
	Assert(header.magic == FASSET_MAGIC);
	Assert(header.assetType == FASSET_TYPE_FONT);

	FFontHeader fontHeader = {};
	fread(&fontHeader, sizeof(fontHeader), 1, file);

	// Read compressed data
	u8* compressedData = ArenaPushSize(&world->shared->arena->scratch, u8, fontHeader.dataSize);
	fread(compressedData, 1, fontHeader.dataSize, file);
	fclose(file);

	// Decompress into fontBuffer
	u8* fontBuffer = ArenaPushSize(&world->shared->arena->scratch, u8, fontHeader.uncompressedSize);
	LZ4_decompress_safe((const char*)compressedData, (char*)fontBuffer,
		(i32)fontHeader.dataSize, (i32)fontHeader.uncompressedSize);

	// Everything below unchanged
	i32 atlasW = 512, atlasH = 512;
	u8* atlasPixels = ArenaPushSize(&world->shared->arena->scratch, u8, (atlasW * atlasH));

	stbtt_bakedchar bakedChars[96];
	i32 result = stbtt_BakeFontBitmap(fontBuffer, 0, fontSize,
		atlasPixels, atlasW, atlasH, 32, 96, bakedChars);
	Assert(result > 0);

	u32* rgbaPixels = ArenaPushSize(&world->shared->arena->scratch, u32, (atlasW * atlasH * 4));
	for (i32 i = 0; i < (atlasW * atlasH); ++i)
	{
		u8 a = atlasPixels[i];
		rgbaPixels[i] = (a << 24) | (0x00FFFFFF);
	}

	outFont->atlas = CreateTextureFromPixels(world, rgbaPixels, atlasW, atlasH);
	outFont->size = fontSize;

	for (u32 i = 0; i < GLYPHS_COUNT; ++i)
	{
		stbtt_bakedchar* bc = &bakedChars[i];
		FFontGlyph* glyph = &outFont->glyphs[i];

		glyph->coords.u0 = bc->x0 / (f32)atlasW;
		glyph->coords.v0 = bc->y0 / (f32)atlasH;
		glyph->coords.u1 = bc->x1 / (f32)atlasW;
		glyph->coords.v1 = bc->y1 / (f32)atlasH;
		glyph->width = bc->x1 - bc->x0;
		glyph->height = bc->y1 - bc->y0;
		glyph->offset.x = bc->xoff;
		glyph->offset.y = bc->yoff;
		glyph->xadvance = bc->xadvance;
	}

	ArenaReset(&world->shared->arena->scratch);
	return outFont->atlas;
}


// ────────────────────────────────────────────────────────────────────────
// Sound loader — .fsound
// ────────────────────────────────────────────────────────────────────────
HSound LoadFSound(FSoundManager* SoundManager, FEngineMemory* arena, cc8* filename)
{
	FILE* file;
	fopen_s(&file, filename, "rb");
	Assert(file);

	FAssetHeader header = {};
	fread(&header, sizeof(header), 1, file);
	Assert(header.magic == FASSET_MAGIC);
	Assert(header.assetType == FASSET_TYPE_SOUND);

	FSoundHeader sndHeader = {};
	fread(&sndHeader, sizeof(sndHeader), 1, file);

	u8* compressed = ArenaPushSize(&arena->scratch, u8, sndHeader.dataSize);
	fread(compressed, 1, sndHeader.dataSize, file);
	fclose(file);

	// decompress into permanent arena (sound stays alive)
	i16* pcm = ArenaPushSize(&arena->permanent, i16, sndHeader.uncompressedSize);
	LZ4_decompress_safe((cc8*)compressed,
		(c8*)pcm,
		(i32)sndHeader.dataSize,
		(i32)sndHeader.uncompressedSize);

	Assert(SoundManager->assetBank->assetsCount < FMAX_SOUND_ASSETS);
	HSound handle = SoundManager->assetBank->assetsCount++;
	FSoundBuffer* soundBuf = &SoundManager->assetBank->assets[handle];
	soundBuf->samples = pcm;
	soundBuf->sampleCount = sndHeader.sampleCount;
	soundBuf->channels = sndHeader.channels;
	soundBuf->sampleRate = sndHeader.sampleRate;

	ArenaReset(&arena->scratch);
	return handle;
}

// ────────────────────────────────────────────────────────────────────────
// Sprite Sheets - They use an already loaded texture (sheet)
// ────────────────────────────────────────────────────────────────────────
HSpriteSheet RegisterSpriteSheet(FRenderWorld* world, HTexture hTex, u32 frameWidth, u32 frameHeight)
{
	FSpriteSheetTable* spriteSheetTable = world->shared->spriteSheetTable;
	Assert(spriteSheetTable->count < MAX_SPRITESHEETS);
	HSpriteSheet handle = spriteSheetTable->count++;
	FSpriteSheet* sheet = &spriteSheetTable->sheets[handle];

	FTexture* tex = &world->textures[hTex];

	sheet->hTex = hTex;
	sheet->frameWidth = frameWidth;
	sheet->frameHeight = frameHeight;
	sheet->cols = tex->width / frameWidth;
	sheet->rows = tex->height / frameHeight;
	sheet->clipsCount = 0;

	return handle;
}

// ────────────────────────────────────────────────────────────────────────
// Build a simple quad mesh.
// ────────────────────────────────────────────────────────────────────────
HMesh GetQuad(FRenderWorld* world)
{
	HMesh quad = world->meshCount++;

	FTextureVertex verts[] =
	{
		{ {-0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
		{ { 0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
		{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },
		{ {-0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },
	};

	u32 indices[] = { 0, 1, 2, 0, 2, 3 };
	UploadMesh(&world->meshes[quad], world->d3d.device, verts, 4, sizeof(FTextureVertex), indices, 6);
	return quad;
}

// ────────────────────────────────────────────────────────────────────────
// Build a simple blob mesh.
// Flat quad in the XZ plane, facing up (+Y). Used for ground decals like blob shadows.
// ────────────────────────────────────────────────────────────────────────
HMesh GetGroundQuad(FRenderWorld* world)
{
	HMesh quad = world->meshCount++;

	FTextureVertex verts[] =
	{
		{ {-0.5f, 0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		{ { 0.5f, 0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
		{ { 0.5f, 0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
		{ {-0.5f, 0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
	};

	u32 indices[] = { 0, 1, 2, 0, 2, 3 };
	UploadMesh(&world->meshes[quad], world->d3d.device, verts, 4, sizeof(FTextureVertex), indices, 6);
	return quad;
}

// ────────────────────────────────────────────────────────────────────────
// Global Functions
// ────────────────────────────────────────────────────────────────────────
void InitializeFD3D(FRenderWorld* world, FD3DInitParams* d3dInitParams)
{
	InitializeDX11(d3dInitParams, world);

	// Init shaders
	InitializeMaterialShader(&world->materialShader, world->d3d.device);
	InitializeUIShader(&world->uiShader, world->d3d.device, d3dInitParams->window);

	SetUIProjection(world);

	world->dirLight.ambientColor = { 0.5f, 0.35f, 0.25f, 1.0f };
	world->dirLight.diffuseColor = { 1.75f, 1.0f, 1.0f, 1.0f };
	world->dirLight.lightDirection = { 1.75f, -1.0f, 1.0f };

#if FADO_DEBUG
	// 2 verts per line, MAX_DEBUG_LINES lines
	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = sizeof(FDebugVertex) * MAX_DEBUG_LINES * 2;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT result = world->d3d.device->CreateBuffer(&desc, NULL, &world->debugBucket.vertexBuffer);
	Assert(!FAILED(result));

	world->debugBucket.count = 0;
#endif // FADO_DEBUG
}

void Render(FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;

	// Clear the buffers to begin the scene.
	BeginScene(d3d, v4{ 0.0f, 0.0f, 0.0f, 1.0f });

	// Generate the view matrix based on the camera's position.
	RenderCamera(world);

	// Draw all entites
	// TODO: draw only those that need to be drawn (in view).
	FEntityTable* entTable = world->shared->entityTable;
	f32 blobRadius = 2.5f;
	for (u32 i = 0; i < entTable->count; ++i)
	{
		FEntity* entity = &entTable->entities[i];
		if (entity->hMesh == INVALID_HANDLE)
		{
			continue;
		}

		DXMatrix worldMatrix = BuildEntityWorldMatrix(i, world->shared);
		PushDrawCall(world, worldMatrix, entity->hMesh, entity->material, entity->spriteRect);

		if (entity->material.flags & Material_CastShadow)
		{
			if (entity->material.flags & Material_Transparent)
			{
				PushBlobShadow2D(world, world->shared->transforms->positions[i]);
			}
			else
			{
				blobRadius = (2.5f * GetEntityScaleAverage(world->shared, i));
				PushBlobShadow(world, world->shared->transforms->positions[i], blobRadius);
			}
		}
	}

	// Flush all buckets.
	FlushBuckets(world);

	// Present the rendered scene to the screen.
	EndScene(d3d);
}

void D3DResize(FRenderWorld* world, i32 width, i32 height, f32 screenNear, f32 screenDepth)
{
	FD3D* d3d = &world->d3d;

	if (!d3d->swapChain)
	{
		return;
	}

#if FADO_DEBUG
	ImGui_ImplDX11_InvalidateDeviceObjects();
#endif // FADO_DEBUG

	// Release old views/buffers — they hold references to the old back buffer.
	d3d->deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	d3d->renderTargetView->Release();
	d3d->depthStencilView->Release();
	d3d->depthStencilBuffer->Release();

	// Resize the swap chain's buffers to the new size.
	d3d->swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

	// Recreate render target view from the new back buffer.
	ID3D11Texture2D* backBuffer;
	d3d->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	d3d->device->CreateRenderTargetView(backBuffer, nullptr, &d3d->renderTargetView);
	backBuffer->Release();

	// Recreate depth/stencil buffer at the new size.
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	d3d->device->CreateTexture2D(&depthDesc, nullptr, &d3d->depthStencilBuffer);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = depthDesc.Format;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	d3d->device->CreateDepthStencilView(d3d->depthStencilBuffer, &dsvDesc, &d3d->depthStencilView);

	d3d->deviceContext->OMSetRenderTargets(1, &d3d->renderTargetView, d3d->depthStencilView);

	// Update viewport.
	FSharedStuff* shared = world->shared;
	shared->viewport.topLeftX = 0.0f;
	shared->viewport.topLeftY = 0.0f;
	shared->viewport.width = (f32)width;
	shared->viewport.height = (f32)height;
	shared->viewport.minDepth = 0.0f;
	shared->viewport.maxDepth = 1.0f;

	D3D11_VIEWPORT d3d11Viewport = {
		shared->viewport.topLeftX,
		shared->viewport.topLeftY,
		shared->viewport.width,
		shared->viewport.height,
		shared->viewport.minDepth,
		shared->viewport.maxDepth
	};

	// Create the viewport.
	d3d->deviceContext->RSSetViewports(1, &d3d11Viewport);

	// Update projection matrix aspect ratio.
	f32 aspect = (f32)width / (f32)height;
	f32 fovY = Pi32 / 4.0f; // match whatever FOV you used at init
	shared->camera.projection = DXMatrixToMat4(DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, 0.3f, 1000.0f));

	SetUIProjection(world);

#if FADO_DEBUG
	ImGui_ImplDX11_CreateDeviceObjects();
#endif // FADO_DEBUG
}


// ─────────────────────────────────
/// Debug Only
#if FADO_DEBUG
#include "fado_collision.h"

// Call from game code to queue a line for this frame
internal void DebugDrawLine(FRenderWorld* world, v3 start, v3 end, v4 color)
{
	FDebugLineBucket* bucket = &world->debugBucket;
	if (bucket->count >= MAX_DEBUG_LINES) { return; }
	FDebugLine* line = &bucket->lines[bucket->count++];
	line->start = start;
	line->end = end;
	line->color = color;
}

// Draw an AABB as 12 coloured edges
internal void DebugDrawAABB(FRenderWorld* world, const FAABB& box, v4 color)
{
	v3 c[8] =
	{
		{ box.min.x, box.min.y, box.min.z }, // 0 left  bottom front
		{ box.max.x, box.min.y, box.min.z }, // 1 right bottom front
		{ box.max.x, box.min.y, box.max.z }, // 2 right bottom back
		{ box.min.x, box.min.y, box.max.z }, // 3 left  bottom back
		{ box.min.x, box.max.y, box.min.z }, // 4 left  top    front
		{ box.max.x, box.max.y, box.min.z }, // 5 right top    front
		{ box.max.x, box.max.y, box.max.z }, // 6 right top    back
		{ box.min.x, box.max.y, box.max.z }, // 7 left  top    back
	};

	// Bottom ring
	DebugDrawLine(world, c[0], c[1], color);
	DebugDrawLine(world, c[1], c[2], color);
	DebugDrawLine(world, c[2], c[3], color);
	DebugDrawLine(world, c[3], c[0], color);
	// Top ring
	DebugDrawLine(world, c[4], c[5], color);
	DebugDrawLine(world, c[5], c[6], color);
	DebugDrawLine(world, c[6], c[7], color);
	DebugDrawLine(world, c[7], c[4], color);
	// Verticals
	DebugDrawLine(world, c[0], c[4], color);
	DebugDrawLine(world, c[1], c[5], color);
	DebugDrawLine(world, c[2], c[6], color);
	DebugDrawLine(world, c[3], c[7], color);
}

// Draw an OBB as 12 coloured edges
internal void DebugDrawOBB(FRenderWorld* world, const FOBB& box, v4 color)
{
	v3 ex = box.axes[0] * box.halfExtents.x;
	v3 ey = box.axes[1] * box.halfExtents.y;
	v3 ez = box.axes[2] * box.halfExtents.z;

	v3 c[8] =
	{
		{ box.center.x - ex.x - ey.x - ez.x, box.center.y - ex.y - ey.y - ez.y, box.center.z - ex.z - ey.z - ez.z }, // 0
		{ box.center.x + ex.x - ey.x - ez.x, box.center.y + ex.y - ey.y - ez.y, box.center.z + ex.z - ey.z - ez.z }, // 1
		{ box.center.x + ex.x - ey.x + ez.x, box.center.y + ex.y - ey.y + ez.y, box.center.z + ex.z - ey.z + ez.z }, // 2
		{ box.center.x - ex.x - ey.x + ez.x, box.center.y - ex.y - ey.y + ez.y, box.center.z - ex.z - ey.z + ez.z }, // 3
		{ box.center.x - ex.x + ey.x - ez.x, box.center.y - ex.y + ey.y - ez.y, box.center.z - ex.z + ey.z - ez.z }, // 4
		{ box.center.x + ex.x + ey.x - ez.x, box.center.y + ex.y + ey.y - ez.y, box.center.z + ex.z + ey.z - ez.z }, // 5
		{ box.center.x + ex.x + ey.x + ez.x, box.center.y + ex.y + ey.y + ez.y, box.center.z + ex.z + ey.z + ez.z }, // 6
		{ box.center.x - ex.x + ey.x + ez.x, box.center.y - ex.y + ey.y + ez.y, box.center.z - ex.z + ey.z + ez.z }, // 7
	};

	// Bottom ring
	DebugDrawLine(world, c[0], c[1], color);
	DebugDrawLine(world, c[1], c[2], color);
	DebugDrawLine(world, c[2], c[3], color);
	DebugDrawLine(world, c[3], c[0], color);
	// Top ring
	DebugDrawLine(world, c[4], c[5], color);
	DebugDrawLine(world, c[5], c[6], color);
	DebugDrawLine(world, c[6], c[7], color);
	DebugDrawLine(world, c[7], c[4], color);
	// Verticals
	DebugDrawLine(world, c[0], c[4], color);
	DebugDrawLine(world, c[1], c[5], color);
	DebugDrawLine(world, c[2], c[6], color);
	DebugDrawLine(world, c[3], c[7], color);
}

internal void ShowDebugGui(FRenderWorld* world)
{
	ImGui::Begin("Inspector");

	u32 selected =world->shared->selectedEntity;
	FTransforms* transforms = world->shared->transforms;

	c8 label[64];

	// Camera
	snprintf(label, sizeof(label), "Camera Transform");
	if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
	{
		snprintf(label, sizeof(label), "Position");
		ImGui::DragFloat3(label, &transforms->positions[0].x, 0.1f);
	}

	// Light
	snprintf(label, sizeof(label), "Light");
	if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
	{
		FDirectionalLight* light = &world->dirLight;

		snprintf(label, sizeof(label), "Ambient Color");
		ImGui::DragFloat4(label, &light->ambientColor.x, 0.1f);

		snprintf(label, sizeof(label), "Diffuse Color");
		ImGui::DragFloat4(label, &light->diffuseColor.x, 0.1f);

		snprintf(label, sizeof(label), "Direction");
		ImGui::DragFloat3(label, &light->lightDirection.x, 0.1f);
	}

	// Selected Entity
	snprintf(label, sizeof(label), "Selected entity Transform_%d", selected);
	if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
	{
		snprintf(label, sizeof(label), "Position_%d", selected);
		ImGui::DragFloat3(label, &transforms->positions[selected].x, 0.1f);

		snprintf(label, sizeof(label), "Rotation_%d", selected);
		v3 rot = QuatToEuler(transforms->rotations[selected]);
		if (ImGui::DragFloat3(label, &rot.x, 0.1f))
		{
			transforms->rotations[selected] = QuatFromEuler(rot);
		}

		snprintf(label, sizeof(label), "Scale_%d", selected);
		ImGui::DragFloat3(label, &transforms->scales[selected].x, 0.1f);
	}
	ImGui::End();
}


internal void FlushDebugLineBucket(FRenderWorld* world)
{
	FDebugLineBucket* bucket = &world->debugBucket;
	if (bucket->count == 0) { return; }

	FD3D* d3d = &world->d3d;

	// Upload all line verts into the dynamic buffer
	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT result = d3d->deviceContext->Map(bucket->vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	Assert(!FAILED(result));

	FDebugVertex* verts = (FDebugVertex*)mapped.pData;
	for (u32 i = 0; i < bucket->count; ++i)
	{
		verts[i * 2 + 0].position = bucket->lines[i].start;
		verts[i * 2 + 1].position = bucket->lines[i].end;
	}
	d3d->deviceContext->Unmap(bucket->vertexBuffer, 0);

	// Reuse the color shader — it only needs POSITION
	FMaterialShader* shader = &world->materialShader;
	d3d->deviceContext->IASetInputLayout(shader->layout);
	d3d->deviceContext->VSSetShader(shader->vertexShader, NULL, 0);
	d3d->deviceContext->PSSetShader(shader->pixelShader, NULL, 0);

	// Switch topology to lines
	d3d->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	u32 stride = sizeof(FDebugVertex);
	u32 offset = 0;
	d3d->deviceContext->IASetVertexBuffers(0, 1, &bucket->vertexBuffer, &stride, &offset);

	// Draw each line with its own colour via the color constant buffer
	FCamera* cam = &world->shared->camera;
	d3d->worldMatrix = DirectX::XMMatrixIdentity();
	DXMatrix cameraView = (DXMatrix)cam->view.m;
	for (u32 i = 0; i < bucket->count; ++i)
	{
		// Upload color for this line
		D3D11_MAPPED_SUBRESOURCE colorMapped;
		d3d->deviceContext->Map(shader->materialBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &colorMapped);
		FMaterialBuffer* cb = (FMaterialBuffer*)colorMapped.pData;
		cb->color = { bucket->lines[i].color.r, bucket->lines[i].color.g,
					  bucket->lines[i].color.b, bucket->lines[i].color.a };
		d3d->deviceContext->Unmap(shader->materialBuffer, 0);
		d3d->deviceContext->PSSetConstantBuffers(1, 1, &shader->materialBuffer);

		// Upload identity world matrix (lines are already in world space)
		D3D11_MAPPED_SUBRESOURCE matMapped;
		d3d->deviceContext->Map(shader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &matMapped);
		FMatrixBuffer* mb = (FMatrixBuffer*)matMapped.pData;
		mb->world = DirectX::XMMatrixTranspose(d3d->worldMatrix);
		mb->view = DirectX::XMMatrixTranspose(cameraView);
		mb->projection = DirectX::XMMatrixTranspose((DXMatrix)cam->projection.m);
		d3d->deviceContext->Unmap(shader->matrixBuffer, 0);
		d3d->deviceContext->VSSetConstantBuffers(0, 1, &shader->matrixBuffer);

		d3d->deviceContext->Draw(2, i * 2);
	}

	// Restore triangle topology for next bucket
	d3d->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	bucket->count = 0;
}

void DebugRender(FRenderWorld* world)
{
	ShowDebugGui(world);
	world->shared->canSelect = !ImGui::GetIO().WantCaptureMouse;

	// Clear the buffers to begin the scene.
	FD3D* d3d = &world->d3d;
	BeginScene(d3d, v4{ 0.0f, 0.0f, 0.0f, 1.0f });

	// Generate the view matrix based on the camera's position.
	RenderCamera(world);

	FEntityTable* entTable = world->shared->entityTable;
	f32 blobRadius = 2.5f;
	for (u32 i = 0; i < entTable->count; ++i)
	{
		FEntity* entity = &entTable->entities[i];
		if (entity->hMesh == INVALID_HANDLE)
		{
			continue;
		}

		DXMatrix worldMatrix = BuildEntityWorldMatrix(i, world->shared);
		PushDrawCall(world, worldMatrix, entity->hMesh, entity->material, entity->spriteRect);

		if (entity->material.flags & Material_CastShadow)
		{
			if (entity->material.flags & Material_Transparent)
			{
				PushBlobShadow2D(world, world->shared->transforms->positions[i]);
			}
			else
			{
				blobRadius = (2.5f * GetEntityScaleAverage(world->shared, i));
				PushBlobShadow(world, world->shared->transforms->positions[i], blobRadius);
			}
		}
	}

	// Flush all buckets — shader bound once per bucket, zero branching.
	FlushBuckets(world);

	FCollisionWorld* collisionWorld = world->shared->collisionWorld;
	for (u32 i = 0; i < collisionWorld->colliders.count; ++i)
	{
		FCollider* c = &collisionWorld->colliders.colliders[i];
		v4 color = (c->flags & Collision_Trigger)   ? FColor::Green()   // green
				 : (c->flags & Collision_Static)	? FColor::Blue()	// blue
				 : (c->flags & Collision_Kinematic) ? FColor::Purple()	// purple
				 : (c->flags & Collision_Dynamic)   ? FColor::Orange()  // orange
				 : (c->flags & Collision_Physics)   ? FColor::Red()		// red
													: FColor::White();	// white

		if(c->useOBB)
		{
			DebugDrawOBB(world, c->worldOBB, color);
		}
		else
		{
			DebugDrawAABB(world, c->worldAABB, color);
		}
	}
	FlushDebugLineBucket(world);

	// Present the rendered scene to the screen.
	EndScene(d3d);
}

#endif // FADO_DEBUG