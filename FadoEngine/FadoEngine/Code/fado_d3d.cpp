// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#define _CRT_SECURE_NO_WARNINGS

#include "fado_d3d.h"
#include "Tools/FadoConverter/fado_asset_format.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "Tools/FadoConverter/stb/stb_truetype.h"

#include "glb/fado_glb.h"
#include "fado_math.h"

#if FADO_DEBUG
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#endif // FADO_DEBUG

// ───────────────────────────
// Constants
// ───────────────────────────
// Unified vertex and pixel shaders entry points.
cc8* k_vsEntryFuncName = "VertexShaderEntry";
cc8* k_psEntryFuncName = "PixelShaderEntry";

// ────────────────────────────────────────────────────────────────────────
// FD3D
// ────────────────────────────────────────────────────────────────────────
internal void InitializeDX11(FD3DInitParams* d3dInitParams, FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;
	FSharedStuff* shared = world->shared;
	FMemoryArena* scratchArena = world->shared->scratchArena;

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
	displayModeList = (DXGI_MODE_DESC*)ArenaPushArray(scratchArena, numModes, DXGI_MODE_DESC);
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

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	result = d3d->device->CreateBlendState(&blendDesc, &d3d->uiBlendState);
	Assert(!FAILED(result));

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

	shared->camera.fovY = fieldOfView;
	shared->camera.aspect = screenAspect;
	shared->camera.nearZ = d3dInitParams->screenNear;
	shared->camera.farZ = d3dInitParams->screenDepth;

	// Create the projection matrix for 3D rendering.
	d3d->projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fieldOfView, screenAspect, d3dInitParams->screenNear, d3dInitParams->screenDepth);

	// Initialize the world matrix to the identity matrix.
	d3d->worldMatrix = DirectX::XMMatrixIdentity();

	// Not used currently.
	// TODO: check if we should just delete it.
	// Create an orthographic projection matrix for 2D rendering.
	//d3d->orthoMatrix = DirectX::XMMatrixOrthographicLH((f32)d3dInitParams->screenWidth, (f32)d3dInitParams->screenHeight, d3dInitParams->screenNear, d3dInitParams->screenDepth);
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
	FMatrixBuffer mat = {};
	mat.world = DirectX::XMMatrixTranspose(world->d3d.worldMatrix);
	mat.view = DirectX::XMMatrixTranspose(world->cameraView);
	mat.projection = DirectX::XMMatrixTranspose(world->d3d.projectionMatrix);
	return mat;
}

// ────────────────────────────────────────────────────────────────────────
// FColorShader
// ────────────────────────────────────────────────────────────────────────
internal void InitializeColorShader(FColorShader *colorShader, ID3D11Device* device, HWND window)
{
	// Compile the shader code.
	wchar hlslFileName[FMAX_PATH] = { L"FadoEngine\\Shaders\\color.hlsl" };
	ID3D10Blob* vertexShaderBuffer = nullptr;
	ID3D10Blob* pixelShaderBuffer = nullptr;
	LoadAndCompileShader(device, hlslFileName, vertexShaderBuffer, pixelShaderBuffer, colorShader->vertexShader, colorShader->pixelShader);

	// Create the vertex input layout description.
	// This setup needs to match the VertexType stucture in the model and in the shader.
	D3D11_INPUT_ELEMENT_DESC polygonLayout[1];
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	// Create the vertex input layout.
	HRESULT result = device->CreateInputLayout(polygonLayout, 1, vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &colorShader->layout);
	Assert(!FAILED(result));

	// Release the vertex shader buffer and pixel shader buffer since they are no longer needed.
	vertexShaderBuffer->Release();
	vertexShaderBuffer = 0;

	pixelShaderBuffer->Release();
	pixelShaderBuffer = 0;

	// Setup the description of the dynamic matrix constant buffer that is in the vertex shader.
	D3D11_BUFFER_DESC matrixBufferDesc;
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(FMatrixBuffer);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = device->CreateBuffer(&matrixBufferDesc, NULL, &colorShader->matrixBuffer);
	Assert(!FAILED(result));

	D3D11_BUFFER_DESC colorBufferDesc;
	colorBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	colorBufferDesc.ByteWidth = sizeof(FColorBuffer);
	colorBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	colorBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	colorBufferDesc.MiscFlags = 0;
	colorBufferDesc.StructureByteStride = 0;

	result = device->CreateBuffer(&colorBufferDesc, NULL, &colorShader->colorBuffer);
	Assert(!FAILED(result));
}

internal void SetColorShaderParameters(FRenderWorld* world, u32 hColorDrawCall)
{
	// Transpose the matrices to prepare them for the shader.
	FMatrixBuffer mat = GetShadersTransposeMatrices(world);

	ID3D11DeviceContext* deviceContext = world->d3d.deviceContext;
	FColorShader* colorShader = &world->colorShader;

	// Lock the constant buffer so it can be written to.
	// Upload matrix buffer -> b0 on vertex shader.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = deviceContext->Map(colorShader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	Assert(!FAILED(result));

	// Get a pointer to the data in the constant buffer and opy the matrices into the constant buffer.
	FMatrixBuffer* dataPtr = (FMatrixBuffer*)mappedResource.pData;
	dataPtr->world = mat.world;
	dataPtr->view = mat.view;
	dataPtr->projection = mat.projection;

	// Unlock the constant buffer.
	deviceContext->Unmap(colorShader->matrixBuffer, 0);
	deviceContext->VSSetConstantBuffers(0, 1, &colorShader->matrixBuffer);

	result = deviceContext->Map(colorShader->colorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	Assert(!FAILED(result));
	FColorBuffer* colorDataPtr = (FColorBuffer*)mappedResource.pData;
	colorDataPtr->color = world->colorBucket.calls[hColorDrawCall].color;
	deviceContext->Unmap(colorShader->colorBuffer, 0);

	deviceContext->PSSetConstantBuffers(0, 1, &colorShader->colorBuffer);

	// Bind color buffer -> b1 on pixel shader.
	deviceContext->PSSetConstantBuffers(1, 1, &colorShader->colorBuffer);
}

// ────────────────────────────────────────────────────────────────────────
// FUnlitTextureShader
// ────────────────────────────────────────────────────────────────────────
internal void InitializeUnlitTextureShader(FUnlitTextureShader* unlitTexShader, ID3D11Device* device, HWND window)
{
	// Compile the shader code.
	wchar hlslFileName[FMAX_PATH] = { L"FadoEngine\\Shaders\\unlit_texture.hlsl" };
	ID3D10Blob* vertexShaderBuffer = nullptr;
	ID3D10Blob* pixelShaderBuffer = nullptr;
	LoadAndCompileShader(device, hlslFileName, vertexShaderBuffer, pixelShaderBuffer, unlitTexShader->vertexShader, unlitTexShader->pixelShader);

	// Create the vertex input layout description.
	// This setup needs to match the VertexType stucture in the model and in the shader.
	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "TEXCOORD";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = 24; // >> IMPORTANT: skip position(12) + normal(12)
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	u32 numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	// Create the vertex input layout.
	HRESULT result = device->CreateInputLayout(polygonLayout, numElements, vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &unlitTexShader->layout);
	Assert(!FAILED(result));

	// Release the vertex shader buffer and pixel shader buffer since they are no longer needed.
	vertexShaderBuffer->Release();
	vertexShaderBuffer = 0;

	pixelShaderBuffer->Release();
	pixelShaderBuffer = 0;

	// Setup the description of the dynamic matrix constant buffer that is in the vertex shader.
	D3D11_BUFFER_DESC matrixBufferDesc;
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(FMatrixBuffer);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = device->CreateBuffer(&matrixBufferDesc, NULL, &unlitTexShader->matrixBuffer);
	Assert(!FAILED(result));

	// Create a texture sampler state description.
	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.MaxAnisotropy = 8;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

	// Create the texture sampler state.
	result = device->CreateSamplerState(&samplerDesc, &unlitTexShader->sampleState);
	Assert(!FAILED(result));
}

internal void SetUnlitTextureShaderParameters(FRenderWorld* world, HTexture hTexture)
{
	// Transpose the matrices to prepare them for the shader.
	FMatrixBuffer mat = GetShadersTransposeMatrices(world);

	ID3D11DeviceContext* deviceContext = world->d3d.deviceContext;
	FUnlitTextureShader* textureShader = &world->unlitTextureShader;

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = deviceContext->Map(textureShader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	Assert(!FAILED(result));

	// Get a pointer to the data in the constant buffer.
	FMatrixBuffer* dataPtr = (FMatrixBuffer*)mappedResource.pData;

	// Copy the matrices into the constant buffer.
	dataPtr->world = mat.world;
	dataPtr->view = mat.view;
	dataPtr->projection = mat.projection;

	// Unlock the constant buffer.
	deviceContext->Unmap(textureShader->matrixBuffer, 0);

	// Set the position of the constant buffer in the vertex shader.
	u32 bufferNumber = 0;

	// Finanly set the constant buffer in the vertex shader with the updated values.
	deviceContext->VSSetConstantBuffers(bufferNumber, 1, &textureShader->matrixBuffer);

	// Set shader texture resource in the pixel shader.
	deviceContext->PSSetShaderResources(0, 1, &world->textures[hTexture].textureView);
}

// ────────────────────────────────────────────────────────────────────────
// FLitTexture
// ────────────────────────────────────────────────────────────────────────
internal void InitializeLitTextureShader(FLitTextureShader* litTexShader, ID3D11Device* device, HWND window)
{
	// Compile the shader code.
	wchar hlslFileName[FMAX_PATH] = { L"FadoEngine\\Shaders\\lit_texture.hlsl" };
	ID3D10Blob* vertexShaderBuffer = nullptr;
	ID3D10Blob* pixelShaderBuffer = nullptr;
	LoadAndCompileShader(device, hlslFileName, vertexShaderBuffer, pixelShaderBuffer, litTexShader->vertexShader, litTexShader->pixelShader);

	// Create the vertex input layout description.
	// This setup needs to match the VertexType stucture in the model and in the shader.
	D3D11_INPUT_ELEMENT_DESC polygonLayout[3];
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "NORMAL";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	polygonLayout[2].SemanticName = "TEXCOORD";
	polygonLayout[2].SemanticIndex = 0;
	polygonLayout[2].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[2].InputSlot = 0;
	polygonLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[2].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	u32 numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	// Create the vertex input layout.
	HRESULT result = device->CreateInputLayout(polygonLayout, numElements, vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &litTexShader->layout);
	Assert(!FAILED(result));

	// Release the vertex shader buffer and pixel shader buffer since they are no longer needed.
	vertexShaderBuffer->Release();
	vertexShaderBuffer = 0;

	pixelShaderBuffer->Release();
	pixelShaderBuffer = 0;

	// Setup the description of the dynamic matrix constant buffer that is in the vertex shader.
	D3D11_BUFFER_DESC matrixBufferDesc;
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(FMatrixBuffer);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = device->CreateBuffer(&matrixBufferDesc, NULL, &litTexShader->matrixBuffer);
	Assert(!FAILED(result));

	// Create a texture sampler state description.
	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.MaxAnisotropy = 8;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

	// Create the texture sampler state.
	result = device->CreateSamplerState(&samplerDesc, &litTexShader->sampleState);
	Assert(!FAILED(result));

	// Setup the description of the light dynamic constant buffer that is in the pixel shader.
	// Note that ByteWidth always needs to be a multiple of 16 if using D3D11_BIND_CONSTANT_BUFFER or CreateBuffer will fail.
	D3D11_BUFFER_DESC lightBufferDesc;
	lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightBufferDesc.ByteWidth = sizeof(FLightBuffer);
	lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightBufferDesc.MiscFlags = 0;
	lightBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = device->CreateBuffer(&lightBufferDesc, NULL, &litTexShader->lightBuffer);
	Assert(!FAILED(result));
}

internal void SetLitTextureShaderParameters(FRenderWorld* world, HTexture hTexture)
{
	// Transpose the matrices to prepare them for the shader.
	FMatrixBuffer mat = GetShadersTransposeMatrices(world);

	ID3D11DeviceContext* deviceContext = world->d3d.deviceContext;
	FLitTextureShader* lightShader = &world->litTextureShader;

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = deviceContext->Map(lightShader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	Assert(!FAILED(result))

	// Get a pointer to the data in the constant buffer.
	FMatrixBuffer* dataPtr = (FMatrixBuffer*)mappedResource.pData;

	// Copy the matrices into the constant buffer.
	dataPtr->world = mat.world;
	dataPtr->view = mat.view;
	dataPtr->projection = mat.projection;

	// Unlock the constant buffer.
	deviceContext->Unmap(lightShader->matrixBuffer, 0);

	// Set the position of the constant buffer in the vertex shader.
	u32 bufferNumber = 0;

	// Finanly set the constant buffer in the vertex shader with the updated values.
	deviceContext->VSSetConstantBuffers(bufferNumber, 1, &lightShader->matrixBuffer);

	// Set shader texture resource in the pixel shader.
	deviceContext->PSSetShaderResources(0, 1, &world->textures[hTexture].textureView);

	// Lock the light constant buffer so it can be written to.
	result = deviceContext->Map(lightShader->lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	Assert(!FAILED(result))

	// Get a pointer to the data in the constant buffer.
	FLightBuffer* lightDataPtr = (FLightBuffer*)mappedResource.pData;

	// Copy the lighting variables into the constant buffer.
	lightDataPtr->ambientColor = lightShader->ambientColor;
	lightDataPtr->diffuseColor = lightShader->diffuseColor;
	lightDataPtr->lightDirection = lightShader->lightDirection;
	lightDataPtr->padding = 0.0f;

	// Unlock the constant buffer.
	deviceContext->Unmap(lightShader->lightBuffer, 0);

	// Finally set the light constant buffer in the pixel shader with the updated values.
	deviceContext->PSSetConstantBuffers(0, 1, &lightShader->lightBuffer);
}

// ────────────────────────────────────────────────────────────────────────
// FUIShader
// ────────────────────────────────────────────────────────────────────────
internal void InitializeUIShader(FUIShader* uiShader, ID3D11Device* device, HWND window)
{
	// Compile the shader code.
	wchar hlslFileName[FMAX_PATH] = { L"FadoEngine\\Shaders\\ui.hlsl" };
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
internal void PushDrawCall(FRenderBucket* bucket, HMesh hMesh, HTexture hTexture, DXMatrix worldMatrix, DXFloat4 color = {})
{
	Assert(bucket->count < MAX_DRAW_CALLS);
	FDrawCall* call = &bucket->calls[bucket->count++];
	call->hMesh = hMesh;
	call->hTexture = hTexture;
	call->worldMatrix = worldMatrix;
	call->color = color;
}

internal void DrawColor(FRenderWorld* world, HMesh hMesh, DXFloat4 color, DXMatrix worldMatrix)
{
	PushDrawCall(&world->colorBucket, hMesh, INVALID_HANDLE, worldMatrix, color);
}
internal void DrawUnlitTextrue(FRenderWorld* world, HMesh hMesh, HTexture hTex, DXMatrix worldMatrix)
{
	PushDrawCall(&world->unlitTextureBucket, hMesh, hTex, worldMatrix);
}
internal void DrawLitTexture(FRenderWorld* world, HMesh hMesh, HTexture hTex, DXMatrix worldMatrix)
{
	PushDrawCall(&world->litTextureBucket, hMesh, hTex, worldMatrix);
}

// ────────────────────────────────────────────────────────────────────────
// Flush buckets
// ────────────────────────────────────────────────────────────────────────
internal void FlushColorBucket(FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;
	FColorShader* shader = &world->colorShader;

	d3d->deviceContext->IASetInputLayout(shader->layout);
	d3d->deviceContext->VSSetShader(shader->vertexShader, NULL, 0);
	d3d->deviceContext->PSSetShader(shader->pixelShader, NULL, 0);

	for (u32 i = 0; i < world->colorBucket.count; ++i)
	{
		FDrawCall* call = &world->colorBucket.calls[i];
		d3d->worldMatrix = call->worldMatrix;
		FMeshBuffer* mesh = &world->meshes[call->hMesh];
		SetColorShaderParameters(world, i);
		RenderMesh(mesh, d3d->deviceContext);
		d3d->deviceContext->DrawIndexed(mesh->indexCount, 0, 0);
	}

	// Clear bucket for next frame.
	world->colorBucket.count = 0;
}

internal void FlushTextureBucket(FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;
	FUnlitTextureShader* shader = &world->unlitTextureShader;

	d3d->deviceContext->IASetInputLayout(shader->layout);
	d3d->deviceContext->VSSetShader(shader->vertexShader, NULL, 0);
	d3d->deviceContext->PSSetShader(shader->pixelShader, NULL, 0);
	d3d->deviceContext->PSSetSamplers(0, 1, &shader->sampleState);

	for (u32 i = 0; i < world->unlitTextureBucket.count; i++)
	{
		FDrawCall* call = &world->unlitTextureBucket.calls[i];
		d3d->worldMatrix = call->worldMatrix;
		FMeshBuffer* mesh = &world->meshes[call->hMesh];
		SetUnlitTextureShaderParameters(world, call->hTexture);
		RenderMesh(mesh, d3d->deviceContext);
		d3d->deviceContext->DrawIndexed(mesh->indexCount, 0, 0);
	}

	world->unlitTextureBucket.count = 0;
}

internal void FlushLitTextureBucket(FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;
	FLitTextureShader* shader = &world->litTextureShader;

	d3d->deviceContext->IASetInputLayout(shader->layout);
	d3d->deviceContext->VSSetShader(shader->vertexShader, NULL, 0);
	d3d->deviceContext->PSSetShader(shader->pixelShader, NULL, 0);
	d3d->deviceContext->PSSetSamplers(0, 1, &shader->sampleState);

	for (u32 i = 0; i < world->litTextureBucket.count; i++)
	{
		FDrawCall* call = &world->litTextureBucket.calls[i];
		d3d->worldMatrix = call->worldMatrix;
		FMeshBuffer* mesh = &world->meshes[call->hMesh];
		SetLitTextureShaderParameters(world, call->hTexture);
		RenderMesh(mesh, d3d->deviceContext);
		d3d->deviceContext->DrawIndexed(mesh->indexCount, 0, 0);
	}

	world->litTextureBucket.count = 0;
}

// ────────────────────────────────────────────────────────────────────────
// FlushUIBucket
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
	{ return; }

	FUIVertex* verts = ArenaPushArray(world->shared->scratchArena, MAX_UI_VERTS, FUIVertex);
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

	f32 blend_factor[4] = { 0, 0, 0, 0 };
	deviceContext->OMSetBlendState(world->d3d.uiBlendState, blend_factor, 0xFFFFFFFF);
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
			case EUICommandType::Rect:
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
	deviceContext->OMSetBlendState(nullptr, blend_factor, 0xFFFFFFFF);

	bucket->count = 0;
	ArenaReset(world->shared->scratchArena);
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

	world->cameraView = DirectX::XMMatrixLookAtLH(positionVector, lookAtVector, upVector);
}

// Returns a DXMatrix by building it from the Entity's transform.
internal DXMatrix BuildEntityWorldMatrix(HEntity hEntity, FSharedStuff* shared)
{
	FEntity* entity = &shared->entityTable->entities[hEntity];
	FTransformTable* transforms = shared->transforms;

	v3 scale = transforms->scales[entity->hTransform];
	DXMatrix scaleMatrix = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);

	quat rot = transforms->rotations[entity->hTransform];
	DirectX::XMVECTOR quatVector = DirectX::XMVectorSet(rot.x, rot.y, rot.z, rot.w);
	DXMatrix rotMatrix = DirectX::XMMatrixRotationQuaternion(quatVector);

	v3 pos = transforms->positions[entity->hTransform];
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
	FILE* file = fopen(fileName, "rb");
	Assert(file);

	// Read and validate generic asset header.
	FAssetHeader assetHeader = {};
	fread(&assetHeader, sizeof(assetHeader), 1, file);
	Assert(assetHeader.magic == FASSET_MAGIC && assetHeader.assetType == FASSET_TYPE_IMAGE);

	// Read image-specific header.
	FImageHeader header = {};
	fread(&header, sizeof(header), 1, file);

	// Read per-mip offset and size tables.
	u32* mipOffsets = ArenaPushArray(world->shared->scratchArena, header.mipCount, u32);
	u32* mipSizes = ArenaPushArray(world->shared->scratchArena, header.mipCount, u32);
	fread(mipOffsets, sizeof(u32), header.mipCount, file);
	fread(mipSizes, sizeof(u32), header.mipCount, file);

	// Read all compressed mip data in one shot.
	u8* allMipData = ArenaPushArray(world->shared->scratchArena, header.dataSize, u8);
	fread(allMipData, header.dataSize, 1, file);
	fclose(file);

	DXGI_FORMAT dxgiFormat = (header.format == FIMAGE_FORMAT_BC3)
		? DXGI_FORMAT_BC3_UNORM
		: DXGI_FORMAT_R8G8B8A8_UNORM;

	// Create texture with full mip chain.
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = header.width;
	texDesc.Height = header.height;
	texDesc.MipLevels = header.mipCount;
	texDesc.ArraySize = 1;
	texDesc.Format = dxgiFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.MiscFlags = 0; // no GENERATE_MIPS flag needed

	// Fill one D3D11_SUBRESOURCE_DATA per mip level.
	D3D11_SUBRESOURCE_DATA* mipData = ArenaPushArray(world->shared->scratchArena, header.mipCount, D3D11_SUBRESOURCE_DATA);

	u32 mipWidth = header.width;
	u32 mipHeight = header.height;
	for (u32 mip = 0; mip < header.mipCount; ++mip)
	{
		mipData[mip].pSysMem = allMipData + mipOffsets[mip];

		if (header.format == FIMAGE_FORMAT_BC3)
		{
			u32 blockW = (mipWidth + 3) / 4;
			mipData[mip].SysMemPitch = blockW * 16; // 16 bytes per BC3 block
			mipData[mip].SysMemSlicePitch = 0;
		}
		else
		{
			mipData[mip].SysMemPitch = mipWidth * 4;
			mipData[mip].SysMemSlicePitch = 0;
		}

		if (mipWidth > 1) { mipWidth >>= 1; }
		if (mipHeight > 1) { mipHeight >>= 1; }
	}

	ID3D11Texture2D* tex;
	HRESULT result = world->d3d.device->CreateTexture2D(&texDesc, mipData, &tex);
	Assert(!FAILED(result));

	ID3D11ShaderResourceView* srv;
	result = world->d3d.device->CreateShaderResourceView(tex, nullptr, &srv);
	Assert(!FAILED(result));
	tex->Release();

	// Store in world texture pool.
	Assert(world->texturesCount < FMAX_TEXTURES);
	HTexture handle = world->texturesCount++;
	world->textures[handle].textureView = srv;
	world->textures[handle].width = (i32)header.width;
	world->textures[handle].height = (i32)header.height;

	ArenaReset(world->shared->scratchArena);
	return handle;
}

// ────────────────────────────────────────────────────────────────────────
// Model loader — .glb model
// > TODO: Check if we can replace it with a .fasset
// ────────────────────────────────────────────────────────────────────────
HMesh LoadGLBModel(FRenderWorld* world, cc8* fileName)
{
	FGLBAsset* asset = ArenaPushType(world->shared->scratchArena, FGLBAsset);
	ZeroStruct(asset);

	// Load the .glb file
	Assert(GLB_Load(fileName, asset));

	HMesh handle = 0;

	// Walk every mesh -> every primitive
	for (u32 mi = 0; mi < asset->meshCount; mi++)
	{
		FGLBMesh* mesh = &asset->meshes[mi];

		for (u32 pi = 0; pi < mesh->primitiveCount; pi++)
		{
			FGLBPrimitive* prim = &mesh->primitives[pi];
			if (!prim->vertices || !prim->indices || prim->vertexCount == 0)
			{
				continue;
			}

			// NOTE: FGLBVertex and the texture struct need to match in layout.
			// FLitTextureVertex contains all attributes currently imported from GLB (position, normal, uv),
			// so it can be uploaded regardless of which shader will eventually render the mesh.
			FLitTextureVertex* converted = (FLitTextureVertex*)ArenaPushArray(world->shared->scratchArena, prim->vertexCount, FLitTextureVertex);
			if (converted)
			{
				for (u32 v = 0; v < prim->vertexCount; v++)
				{
					converted[v].position = { prim->vertices[v].px, prim->vertices[v].py, prim->vertices[v].pz };
					converted[v].normal = { prim->vertices[v].nx, prim->vertices[v].ny, prim->vertices[v].nz };
					converted[v].texture = { prim->vertices[v].u, prim->vertices[v].v };
				}
				handle = world->meshCount++;
				FMeshBuffer* mesh = &world->meshes[handle];
				mesh->vertexStride = sizeof(FLitTextureVertex);
				UploadMesh(mesh, world->d3d.device, converted, prim->vertexCount, mesh->vertexStride, prim->indices, prim->indexCount);
			}
		}
	}

	ArenaReset(world->shared->scratchArena);
	GLB_Free(asset);
	return handle;
}

HTexture LoadFont(FRenderWorld* world, cc8* filename, f32 fontSize, FFont* outFont)
{
	// Read font file into memory
	FILE* file = fopen(filename, "rb");
	if (!file)
	{
		return false;
	}

	fseek(file, 0, SEEK_END);
	u32 size = ftell(file);
	fseek(file, 0, SEEK_SET);
	u8* fontBuffer = ArenaPushSize(world->shared->scratchArena, u8, size);
	fread(fontBuffer, 1, size, file);
	fclose(file);

	// Bake atlas (single channel, 512x512 — can be adjusted)
	i32 atlasW = 512, atlasH = 512;
	u8* atlasPixels = ArenaPushSize(world->shared->scratchArena, u8, (atlasW * atlasH));

	stbtt_bakedchar bakedChars[96];
	i32 result = stbtt_BakeFontBitmap(fontBuffer, 0, fontSize,
		atlasPixels, atlasW, atlasH, 32, 96, bakedChars);
	Assert(result > 0); // didn't fit, increase atlas size

	// Convert single-channel to RGBA (so it uses same shader/pipeline as rects)
	u32* rgbaPixels = ArenaPushSize(world->shared->scratchArena, u32, (atlasW * atlasH * 4));
	for (i32 i = 0; i < (atlasW * atlasH); ++i)
	{
		u8 a = atlasPixels[i];
		rgbaPixels[i] = (a << 24) | (0x00FFFFFF); // white with alpha = coverage
	}

	// Upload as texture (reuse your existing texture creation path)
	outFont->atlas = CreateTextureFromPixels(world, rgbaPixels, atlasW, atlasH);
	outFont->size = fontSize;

	// Convert baked chars to our glyph format
	for (u32 i = 0; i < GLYPHS_COUNT; ++i)
	{
		stbtt_bakedchar* bc = &bakedChars[i];
		FFontGlyph* glyph = &outFont->glyphs[i];

		glyph->coords.u0 = bc->x0 / (f32)atlasW;
		glyph->coords.v0 = bc->y0 / (f32)atlasH;
		glyph->coords.u1 = bc->x1 / (f32)atlasW;
		glyph->coords.v1 = bc->y1 / (f32)atlasH;

		glyph->width  = bc->x1 - bc->x0;
		glyph->height = bc->y1 - bc->y0;

		glyph->offset.x = bc->xoff;
		glyph->offset.y = bc->yoff;

		glyph->xadvance = bc->xadvance;
	}

	ArenaReset(world->shared->scratchArena);
	return outFont->atlas;
}

// ────────────────────────────────────────────────────────────────────────
// Global Functions
// ────────────────────────────────────────────────────────────────────────
void InitializeFD3D(FRenderWorld* world, FD3DInitParams* d3dInitParams)
{
	InitializeDX11(d3dInitParams, world);

	// Init shaders
	InitializeColorShader(&world->colorShader, world->d3d.device, d3dInitParams->window);
	InitializeUnlitTextureShader(&world->unlitTextureShader, world->d3d.device, d3dInitParams->window);
	InitializeLitTextureShader(&world->litTextureShader, world->d3d.device, d3dInitParams->window);
	InitializeUIShader(&world->uiShader, world->d3d.device, d3dInitParams->window);

	SetUIProjection(world);

	world->litTextureShader.ambientColor = { 0.5f, 0.35f, 0.25f, 1.0f };
	world->litTextureShader.diffuseColor = { 1.75f, 1.0f, 1.0f, 1.0f };
	world->litTextureShader.lightDirection = { 1.75f, -1.0f, 1.0f };

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
	BeginScene(d3d, v4{ 0.0f, 0.0f, 0.0f, 3.0f });

	// Generate the view matrix based on the camera's position.
	RenderCamera(world);

	// Draw all entites
	// TODO: draw only those that need to be drawn (in view).
	FEntityTable* entityTable = world->shared->entityTable;
	for (u32 i = 0; i < world->shared->entityTable->count; ++i)
	{
		FEntity* e = &entityTable->entities[i];
		DXMatrix worldMatrix = BuildEntityWorldMatrix(i, world->shared);

		switch (e->shaderType)
		{
			case EShaderTypes::Color:
			{
				DXFloat4 color = { e->color.r , e->color.g , e->color.b , e->color.a };
				DrawColor(world, e->hMesh, color, worldMatrix);
			} break;

			case EShaderTypes::UnlitTexture:
			{
				DrawUnlitTextrue(world, e->hMesh, e->hTexture, worldMatrix);
			} break;

			case EShaderTypes::LitTexture:
			{
				DrawLitTexture(world, e->hMesh, e->hTexture, worldMatrix);
			} break;

			default:
			{} break;
		}
	}

	// Flush all buckets — shader bound once per bucket.
	FlushColorBucket(world);
	FlushTextureBucket(world);
	FlushLitTextureBucket(world);
	FlushUIBucket(world);

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
	d3d->projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, 0.3f, 1000.0f);

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

	FEntity* selected = GetEntity(world->shared->entityTable, world->shared->selectedEntity);
	HTransform hTransform = selected->hTransform;
	FTransformTable* transforms = world->shared->transforms;

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
		FLitTextureShader* light = &world->litTextureShader;

		snprintf(label, sizeof(label), "Ambient Color");
		ImGui::DragFloat4(label, &light->ambientColor.x, 0.1f);

		snprintf(label, sizeof(label), "Diffuse Color");
		ImGui::DragFloat4(label, &light->diffuseColor.x, 0.1f);

		snprintf(label, sizeof(label), "Direction");
		ImGui::DragFloat3(label, &light->lightDirection.x, 0.1f);
	}

	// Selected Entity
	snprintf(label, sizeof(label), "Selected entity Transform_%d", hTransform);
	if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
	{
		snprintf(label, sizeof(label), "Position_%d", hTransform);
		ImGui::DragFloat3(label, &transforms->positions[hTransform].x, 0.1f);

		snprintf(label, sizeof(label), "Rotation_%d", hTransform);
		v3 rot = QuatToEuler(transforms->rotations[hTransform]);
		if (ImGui::DragFloat3(label, &rot.x, 0.1f))
		{
			transforms->rotations[hTransform] = QuatFromEuler(rot);
		}

		snprintf(label, sizeof(label), "Scale_%d", hTransform);
		ImGui::DragFloat3(label, &transforms->scales[hTransform].x, 0.1f);
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
	FColorShader* shader = &world->colorShader;
	d3d->deviceContext->IASetInputLayout(shader->layout);
	d3d->deviceContext->VSSetShader(shader->vertexShader, NULL, 0);
	d3d->deviceContext->PSSetShader(shader->pixelShader, NULL, 0);

	// Switch topology to lines
	d3d->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	u32 stride = sizeof(FDebugVertex);
	u32 offset = 0;
	d3d->deviceContext->IASetVertexBuffers(0, 1, &bucket->vertexBuffer, &stride, &offset);

	// Draw each line with its own colour via the color constant buffer
	d3d->worldMatrix = DirectX::XMMatrixIdentity();
	for (u32 i = 0; i < bucket->count; ++i)
	{
		// Upload color for this line
		D3D11_MAPPED_SUBRESOURCE colorMapped;
		d3d->deviceContext->Map(shader->colorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &colorMapped);
		FColorBuffer* cb = (FColorBuffer*)colorMapped.pData;
		cb->color = { bucket->lines[i].color.r, bucket->lines[i].color.g,
					  bucket->lines[i].color.b, bucket->lines[i].color.a };
		d3d->deviceContext->Unmap(shader->colorBuffer, 0);
		d3d->deviceContext->PSSetConstantBuffers(1, 1, &shader->colorBuffer);

		// Upload identity world matrix (lines are already in world space)
		D3D11_MAPPED_SUBRESOURCE matMapped;
		d3d->deviceContext->Map(shader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &matMapped);
		FMatrixBuffer* mb = (FMatrixBuffer*)matMapped.pData;
		mb->world = DirectX::XMMatrixTranspose(d3d->worldMatrix);
		mb->view = DirectX::XMMatrixTranspose(world->cameraView);
		mb->projection = DirectX::XMMatrixTranspose(d3d->projectionMatrix);
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
	BeginScene(d3d, v4{ 0.0f, 0.0f, 0.0f, 3.0f });

	// Generate the view matrix based on the camera's position.
	RenderCamera(world);

	FEntityTable* entityTable = world->shared->entityTable;
	for (u32 i = 0; i < entityTable->count; ++i)
	{
		FEntity* e = &entityTable->entities[i];
		DXMatrix worldMatrix = BuildEntityWorldMatrix(i, world->shared);

		switch (e->shaderType)
		{
		case EShaderTypes::Color:
		{
			DXFloat4 color = { e->color.r , e->color.g , e->color.b , e->color.a };
			DrawColor(world, e->hMesh, color, worldMatrix);
		} break;

		case EShaderTypes::UnlitTexture:
		{
			DrawUnlitTextrue(world, e->hMesh, e->hTexture, worldMatrix);
		} break;

		case EShaderTypes::LitTexture:
		{
			DrawLitTexture(world, e->hMesh, e->hTexture, worldMatrix);
		} break;

		default:
		{} break;
		}
	}

	// Flush all buckets — shader bound once per bucket, zero branching.
	FlushColorBucket(world);
	FlushTextureBucket(world);
	FlushLitTextureBucket(world);
	FlushUIBucket(world);

	FCollisionWorld* collisionWorld = world->shared->collisionWorld;
	for (u32 i = 0; i < collisionWorld->colliders.count; ++i)
	{
		FCollider* c = &collisionWorld->colliders.colliders[i];
		v4 color = (c->flags & ECollisionFlags::Trigger)   ? v4{ 0, 1, 0, 1 }    // green
				 : (c->flags & ECollisionFlags::Static)	   ? v4{ 0, 0, 1, 1 }	 // blue
				 : (c->flags & ECollisionFlags::Kinematic) ? v4{ 1, 0, 1, 1 }	 // purple
				 : (c->flags & ECollisionFlags::Dynamic)   ? v4{ 1, 0.5, 0, 1 }  // orange
				 : (c->flags & ECollisionFlags::Physics)   ? v4{ 1, 0, 0, 1 }	 // red
														   : v4{ 1, 1, 1, 1 };	 // white

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