#include "fado_d3d.h"
#include "../code/glb/fado_glb.h"
#include "fado_math.h"

///////////////////////////
// Constants
///////////////////////////
// Unified vertex and pixel shaders entry points.
const char* k_vsEntryFuncName = "VertexShaderEntry";
const char* k_psEntryFuncName = "PixelShaderEntry";

////////////////////////////////////////////////////////////////////////////////
// FD3D
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeFD3D(FD3DInitParams* d3dInitParams, FMemoryArena* scratchArena)
{
	FD3D* d3d = d3dInitParams->d3d;

	// Store the vsync setting.
	d3d->vsyncEnabled = d3dInitParams->vsync;

	// Create a DirectX graphics interface factory.
	HRESULT result;
	IDXGIFactory* factory;
	result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(result))
	{
		return false;
	}

	// Use the factory to create an adapter for the primary graphics interface (video card).
	IDXGIAdapter* adapter;
	result = factory->EnumAdapters(0, &adapter);
	if (FAILED(result))
	{
		return false;
	}

	// Enumerate the primary adapter output (monitor).
	IDXGIOutput* adapterOutput;
	result = adapter->EnumOutputs(0, &adapterOutput);
	if (FAILED(result))
	{
		return false;
	}

	u32 numModes = 0;
	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	if (FAILED(result))
	{
		return false;
	}

	// Create a list to hold all the possible display modes for this monitor/video card combination.
	DXGI_MODE_DESC* displayModeList;
	displayModeList = (DXGI_MODE_DESC*)ArenaPushArray(scratchArena, numModes, DXGI_MODE_DESC);
	if (!displayModeList)
	{
		return false;
	}

	// Now fill the display mode list structures.
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
	if (FAILED(result))
	{
		return false;
	}

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
	if (FAILED(result))
	{
		return false;
	}

	// Store the dedicated video card memory in megabytes.
	d3d->videoCardMemory = (i32)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	// Convert the name of the video card to a character array and store it.
	u64 stringLength;
	i32 error;
	error = wcstombs_s(&stringLength, d3d->videoCardDescription, 128, adapterDesc.Description, 128);
	if (error != 0)
	{
		return false;
	}

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
	if (FAILED(result))
	{
		return false;
	}

	// Disable DXGI default Alt+Enter fullscreen.
	d3d->swapChain->GetParent(__uuidof(IDXGIFactory), (void**)&factory);
	factory->MakeWindowAssociation(d3dInitParams->window, DXGI_MWA_NO_ALT_ENTER);
	factory->Release();

	// Get the pointer to the back buffer.
	ID3D11Texture2D* backBufferPtr;
	result = d3d->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferPtr);
	if (FAILED(result))
	{
		return false;
	}

	// Create the render target view with the back buffer pointer.
	result = d3d->device->CreateRenderTargetView(backBufferPtr, NULL, &d3d->renderTargetView);
	if (FAILED(result))
	{
		return false;
	}

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
	if (FAILED(result))
	{
		return false;
	}

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
	if (FAILED(result))
	{
		return false;
	}

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
	if (FAILED(result))
	{
		return false;
	}

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
	if (FAILED(result))
	{
		return false;
	}

	// Now set the rasterizer state.
	d3d->deviceContext->RSSetState(d3d->rasterState);

	// Setup the viewport for rendering.
	d3d->viewport.Width = (float)d3dInitParams->screenWidth;
	d3d->viewport.Height = (float)d3dInitParams->screenHeight;
	d3d->viewport.MinDepth = 0.0f;
	d3d->viewport.MaxDepth = 1.0f;
	d3d->viewport.TopLeftX = 0.0f;
	d3d->viewport.TopLeftY = 0.0f;

	// Create the viewport.
	d3d->deviceContext->RSSetViewports(1, &d3d->viewport);

	// Setup the projection matrix.
	f32 fieldOfView = Pi32 / 4.0f;
	f32 screenAspect = (float)d3dInitParams->screenWidth / (float)d3dInitParams->screenHeight;

	// Create the projection matrix for 3D rendering.
	d3d->projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fieldOfView, screenAspect, d3dInitParams->screenNear, d3dInitParams->screenDepth);

	// Initialize the world matrix to the identity matrix.
	d3d->worldMatrix = DirectX::XMMatrixIdentity();

	// Create an orthographic projection matrix for 2D rendering.
	d3d->orthoMatrix = DirectX::XMMatrixOrthographicLH((float)d3dInitParams->screenWidth, (float)d3dInitParams->screenHeight, d3dInitParams->screenNear, d3dInitParams->screenDepth);

	return true;
}

internal void BeginScene(FD3D *d3d, v4 color)
{
	// Clear the back buffer.
	d3d->deviceContext->ClearRenderTargetView(d3d->renderTargetView, color.e);

	// Clear the depth buffer.
	d3d->deviceContext->ClearDepthStencilView(d3d->depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

internal void EndScene(FD3D* d3d)
{
	// Present the back buffer to the screen since rendering is complete.
	if (d3d->vsyncEnabled)
	{
		// Lock to screen refresh rate.
		d3d->swapChain->Present(1, 0);
	}
	else
	{
		// Present as fast as possible.
		d3d->swapChain->Present(0, 0);
	}
}

////////////////////////////////////////////////////////////////////////////////
// FColorShaderD3D
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeColorShader(FColorShader *colorShader, ID3D11Device* device, HWND window)
{
	// Set the filename of the hlsl shader.
	wchar hlslFileName[128];
	i32 error = wcscpy_s(hlslFileName, 128, L"src\\shaders\\color.hlsl");
	if (error != 0)
	{
		return false;
	}

	if (GetFileAttributesW(hlslFileName) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBoxW(NULL, L"File not found!", hlslFileName, MB_OK);
		return false;
	}

	ID3D10Blob* errorMessage;

	// Compile the vertex shader code.
	ID3D10Blob* vertexShaderBuffer;
	HRESULT result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_vsEntryFuncName, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&vertexShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBoxW(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Compile the pixel shader code.
	ID3D10Blob* pixelShaderBuffer;
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_psEntryFuncName, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&pixelShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBoxW(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Create the vertex shader from the buffer.
	result = device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &colorShader->vertexShader);
	if (FAILED(result))
	{
		return false;
	}

	// Create the pixel shader from the buffer.
	result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &colorShader->pixelShader);
	if (FAILED(result))
	{
		return false;
	}

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

	// Get a count of the elements in the layout.
	u32 numElements = 1;

	// Create the vertex input layout.
	result = device->CreateInputLayout(polygonLayout, numElements, vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &colorShader->layout);
	if (FAILED(result))
	{
		return false;
	}

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
	if (FAILED(result))
	{
		return false;
	}

	D3D11_BUFFER_DESC colorBufferDesc;
	colorBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	colorBufferDesc.ByteWidth = sizeof(FColorBuffer);
	colorBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	colorBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	colorBufferDesc.MiscFlags = 0;
	colorBufferDesc.StructureByteStride = 0;

	result = device->CreateBuffer(&colorBufferDesc, NULL, &colorShader->colorBuffer);
	if (FAILED(result))
	{
		return false;
	}

	return true;
}

internal void SetColorShaderParameters(FRenderWorld* world, u32 hColorDrawCall)
{
	// Transpose the matrices to prepare them for the shader.
	DXMatrix worldMatrix = XMMatrixTranspose(world->d3d.worldMatrix);
	DXMatrix viewMatrix = XMMatrixTranspose(world->camera.viewMatrix);
	DXMatrix projectionMatrix = XMMatrixTranspose(world->d3d.projectionMatrix);

	ID3D11DeviceContext* deviceContext = world->d3d.deviceContext;
	FColorShader* colorShader = &world->colorShader;

	// Lock the constant buffer so it can be written to.
	// Upload matrix buffer -> b0 on vertex shader.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = deviceContext->Map(colorShader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	Assert(!FAILED(result));

	// Get a pointer to the data in the constant buffer and opy the matrices into the constant buffer.
	FMatrixBuffer* dataPtr = (FMatrixBuffer*)mappedResource.pData;
	dataPtr->world = worldMatrix;
	dataPtr->view = viewMatrix;
	dataPtr->projection = projectionMatrix;

	// Unlock the constant buffer.
	deviceContext->Unmap(colorShader->matrixBuffer, 0);
	deviceContext->VSSetConstantBuffers(0, 1, &colorShader->matrixBuffer);

	result = deviceContext->Map(colorShader->colorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	FColorBuffer* colorDataPtr = (FColorBuffer*)mappedResource.pData;
	colorDataPtr->color = world->colorBucket.calls[hColorDrawCall].color;
	deviceContext->Unmap(colorShader->colorBuffer, 0);

	deviceContext->PSSetConstantBuffers(0, 1, &colorShader->colorBuffer);

	// Bind color buffer -> b1 on pixel shader.
	deviceContext->PSSetConstantBuffers(1, 1, &colorShader->colorBuffer);
}

////////////////////////////////////////////////////////////////////////////////
// FTextureShader
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeTextureShader(FTextureShader* textureShader, ID3D11Device* device, HWND window)
{
	// Set the filename of the hlsl shader.
	wchar hlslFileName[128];
	i32 error = wcscpy_s(hlslFileName, 128, L"src\\shaders\\texture.hlsl");
	if (error != 0)
	{
		return false;
	}

	if (GetFileAttributesW(hlslFileName) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBoxW(NULL, L"File not found!", hlslFileName, MB_OK);
	}

	// Compile the vertex shader code.
	ID3D10Blob* errorMessage;
	ID3D10Blob* vertexShaderBuffer;
	HRESULT result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_vsEntryFuncName, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&vertexShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBoxW(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Compile the pixel shader code.
	ID3D10Blob* pixelShaderBuffer;
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_psEntryFuncName, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&pixelShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBoxW(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Create the vertex shader from the buffer.
	result = device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &textureShader->vertexShader);
	if (FAILED(result))
	{
		return false;
	}

	// Create the pixel shader from the buffer.
	result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &textureShader->pixelShader);
	if (FAILED(result))
	{
		return false;
	}

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
	result = device->CreateInputLayout(polygonLayout, numElements, vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &textureShader->layout);
	if (FAILED(result))
	{
		return false;
	}

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
	result = device->CreateBuffer(&matrixBufferDesc, NULL, &textureShader->matrixBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Create a texture sampler state description.
	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	result = device->CreateSamplerState(&samplerDesc, &textureShader->sampleState);
	if (FAILED(result))
	{
		return false;
	}

	return true;
}

internal void SetTextureShaderParameters(FRenderWorld* world, HTexture hTexture)
{
	// Transpose the matrices to prepare them for the shader.
	DXMatrix worldMatrix = XMMatrixTranspose(world->d3d.worldMatrix);
	DXMatrix viewMatrix = XMMatrixTranspose(world->camera.viewMatrix);
	DXMatrix projectionMatrix = XMMatrixTranspose(world->d3d.projectionMatrix);

	ID3D11DeviceContext* deviceContext = world->d3d.deviceContext;
	FTextureShader* textureShader = &world->textureShader;

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = deviceContext->Map(textureShader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	Assert(!FAILED(result));

	// Get a pointer to the data in the constant buffer.
	FMatrixBuffer* dataPtr = (FMatrixBuffer*)mappedResource.pData;

	// Copy the matrices into the constant buffer.
	dataPtr->world = worldMatrix;
	dataPtr->view = viewMatrix;
	dataPtr->projection = projectionMatrix;

	// Unlock the constant buffer.
	deviceContext->Unmap(textureShader->matrixBuffer, 0);

	// Set the position of the constant buffer in the vertex shader.
	u32 bufferNumber = 0;

	// Finanly set the constant buffer in the vertex shader with the updated values.
	deviceContext->VSSetConstantBuffers(bufferNumber, 1, &textureShader->matrixBuffer);

	// Set shader texture resource in the pixel shader.
	deviceContext->PSSetShaderResources(0, 1, &world->textures[hTexture].textureView);
}

////////////////////////////////////////////////////////////////////////////////
// FLightShader
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeLitTextureShader(FLitTextureShader* litShader, ID3D11Device* device, HWND window)
{
	// Set the filename of the hlsl shader.
	wchar hlslFileName[128];
	i32 error = wcscpy_s(hlslFileName, 128, L"src\\shaders\\light.hlsl");
	if (error != 0)
	{
		return false;
	}

	if (GetFileAttributesW(hlslFileName) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBoxW(NULL, L"File not found!", hlslFileName, MB_OK);
	}

	// Compile the vertex shader code.
	ID3D10Blob* errorMessage;
	ID3D10Blob* vertexShaderBuffer;
	HRESULT result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_vsEntryFuncName, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&vertexShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBoxW(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Compile the pixel shader code.
	ID3D10Blob* pixelShaderBuffer;
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_psEntryFuncName, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&pixelShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBoxW(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Create the vertex shader from the buffer.
	result = device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &litShader->vertexShader);
	if (FAILED(result))
	{
		return false;
	}

	// Create the pixel shader from the buffer.
	result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &litShader->pixelShader);
	if (FAILED(result))
	{
		return false;
	}

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
	result = device->CreateInputLayout(polygonLayout, numElements, vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &litShader->layout);
	if (FAILED(result))
	{
		return false;
	}

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
	result = device->CreateBuffer(&matrixBufferDesc, NULL, &litShader->matrixBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Create a texture sampler state description.
	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	result = device->CreateSamplerState(&samplerDesc, &litShader->sampleState);
	if (FAILED(result))
	{
		return false;
	}

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
	result = device->CreateBuffer(&lightBufferDesc, NULL, &litShader->lightBuffer);
	if (FAILED(result))
	{
		return false;
	}

	return true;
}

internal void SetLitTextureShaderParameters(FRenderWorld* world, HTexture hTexture)
{
	FD3D* d3d = &world->d3d;
	DXMatrix worldMatrix = d3d->worldMatrix;
	DXMatrix viewMatrix = world->camera.viewMatrix;
	DXMatrix projectionMatrix = d3d->projectionMatrix;

	// Transpose the matrices to prepare them for the shader.
	worldMatrix = XMMatrixTranspose(worldMatrix);
	viewMatrix = XMMatrixTranspose(viewMatrix);
	projectionMatrix = XMMatrixTranspose(projectionMatrix);

	ID3D11DeviceContext* deviceContext = d3d->deviceContext;
	FLitTextureShader* lightShader = &world->litTextureShader;

	// Lock the constant buffer so it can be written to.
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = deviceContext->Map(lightShader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	Assert(!FAILED(result))

	// Get a pointer to the data in the constant buffer.
	FMatrixBuffer* dataPtr = (FMatrixBuffer*)mappedResource.pData;

	// Copy the matrices into the constant buffer.
	dataPtr->world = worldMatrix;
	dataPtr->view = viewMatrix;
	dataPtr->projection = projectionMatrix;

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

	// Set the position of the light constant buffer in the pixel shader.
	bufferNumber = 0;

	// Finally set the light constant buffer in the pixel shader with the updated values.
	deviceContext->PSSetConstantBuffers(bufferNumber, 1, &lightShader->lightBuffer);
}

////////////////////////////////////////////////////////////////////////////////
// FTexture / Targa
////////////////////////////////////////////////////////////////////////////////

bool32 LoadTarga32BitIntoTexture(const char* filename, FTexture* tex)
{
	// Open the targa file for reading in binary.
	FILE* filePtr;
	i32 error = fopen_s(&filePtr, filename, "rb");
	if (error != 0)
	{
		return false;
	}

	// Read in the file header.
	FTargaHeader targaFileHeader = {};
	u32 count = (u32)fread(&targaFileHeader, sizeof(FTargaHeader), 1, filePtr);
	if (count != 1)
	{
		return false;
	}

	// Get the important information from the header.
	tex->height = (i32)targaFileHeader.height;
	tex->width = (i32)targaFileHeader.width;
	i32 bpp = (i32)targaFileHeader.pixelDepth;

	// Allow both 24 and 32 bit. 24 will have a 255 value for alpha.
	if (bpp != 24 && bpp != 32)
	{
		return false;
	}

	// Calculate the size of the 32 bit image data.
	i32 bytesPerPixel = bpp / 8;
	i32 imageSize = tex->width * tex->height * bytesPerPixel;

	// Allocate memory for the targa image data.
	u8* targaImage = new u8[imageSize];

	// Read in the targa image data.
	count = (u32)fread(targaImage, 1, imageSize, filePtr);
	if (count != imageSize)
	{
		delete[] targaImage;
		return false;
	}

	// Close the file.
	error = fclose(filePtr);
	if (error != 0)
	{
		delete[] targaImage;
		return false;
	}

	// Allocate memory for the targa destination data.
	tex->targaData = new u8[tex->width * tex->height * 4];

	// Initialize the index into the targa destination data array.
	u32 index = 0;

	// Initialize the index into the targa image data.
	u32 k = (tex->width * tex->height * bytesPerPixel) - (tex->width * bytesPerPixel);

	// Now copy the targa image data into the targa destination array in the correct order since the targa format is stored upside down and also is not in RGBA order.
	for (i32 v = 0; v < tex->height; v++)
	{
		for (i32 u = 0; u < tex->width; u++)
		{
			tex->targaData[index + 0] = targaImage[k + 2];  // Red.
			tex->targaData[index + 1] = targaImage[k + 1];  // Green.
			tex->targaData[index + 2] = targaImage[k + 0];  // Blue
			// Alpha
			if (bytesPerPixel == 4)
			{
				tex->targaData[index + 3] = targaImage[k + 3];
			}
			else
			{
				tex->targaData[index + 3] = 255; // add alpha
			}

			// Increment the indexes into the targa data.
			k += bytesPerPixel;
			index += 4;
		}

		// Set the targa image data index back to the preceding row at the beginning of the column since its reading it in upside down.
		k -= (tex->width * bytesPerPixel * 2);
	}

	// Release the targa image data now that it was copied into the destination array.
	delete[] targaImage;
	targaImage = 0;

	return true;
}

bool32 InitializeTexture(FTexture* tex, ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* filename)
{
	bool32 result;
	D3D11_TEXTURE2D_DESC textureDesc;
	HRESULT hResult;
	u32 rowPitch;
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;

	// Load the targa image data into memory.
	result = LoadTarga32BitIntoTexture(filename, tex);
	if (!result)
	{
		return false;
	}

	// Setup the description of the texture.
	textureDesc.Height = tex->height;
	textureDesc.Width = tex->width;
	textureDesc.MipLevels = 0;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	// Create the empty texture.
	hResult = device->CreateTexture2D(&textureDesc, NULL, &tex->texture);
	if (FAILED(hResult))
	{
		return false;
	}

	// Set the row pitch of the targa image data.
	rowPitch = (tex->width * 4/*bytesPerPixel*/) * sizeof(unsigned char);

	// Copy the targa image data into the texture.
	deviceContext->UpdateSubresource(tex->texture, 0, NULL, tex->targaData, rowPitch, 0);

	// Setup the shader resource view description.
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	// Create the shader resource view for the texture.
	hResult = device->CreateShaderResourceView(tex->texture, &srvDesc, &tex->textureView);
	if (FAILED(hResult))
	{
		return false;
	}

	// Generate mipmaps for this texture.
	deviceContext->GenerateMips(tex->textureView);

	// Release the targa image data now that the image data has been loaded into the texture.
	delete[] tex->targaData;
	tex->targaData = 0;

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Model
////////////////////////////////////////////////////////////////////////////////

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
	if (FAILED(result))
	{
		Assert(0);
	}

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
	if (FAILED(result))
	{
		Assert(0);
	}
}

internal HTexture LoadTexture(FRenderWorld* world, ID3D11Device* device, ID3D11DeviceContext* context, const char* fileName)
{
	HTexture handle = world->texturesCount++;
	InitializeTexture(&world->textures[handle], device, context, fileName);
	return handle;
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
internal void DrawTexture(FRenderWorld* world, HMesh hMesh, HTexture hTex, DXMatrix worldMatrix)
{
	PushDrawCall(&world->textureBucket, hMesh, hTex, worldMatrix);
}
internal void DrawLit(FRenderWorld* world, HMesh hMesh, HTexture hTex, DXMatrix worldMatrix)
{
	PushDrawCall(&world->litTextureBucket, hMesh, hTex, worldMatrix);
}

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
		RenderMesh(&world->meshes[call->hMesh], d3d->deviceContext);
		SetColorShaderParameters(world, i);
		d3d->deviceContext->DrawIndexed(world->meshes[call->hMesh].indexCount, 0, 0);
	}

	// Clear bucket for next frame.
	world->colorBucket.count = 0;
}

internal void FlushTextureBucket(FRenderWorld* world)
{
	FD3D* d3d = &world->d3d;
	FTextureShader* shader = &world->textureShader;

	d3d->deviceContext->IASetInputLayout(shader->layout);
	d3d->deviceContext->VSSetShader(shader->vertexShader, NULL, 0);
	d3d->deviceContext->PSSetShader(shader->pixelShader, NULL, 0);
	d3d->deviceContext->PSSetSamplers(0, 1, &shader->sampleState);

	for (u32 i = 0; i < world->textureBucket.count; i++)
	{
		FDrawCall* call = &world->textureBucket.calls[i];
		d3d->worldMatrix = call->worldMatrix;
		FMeshBuffer* mesh = &world->meshes[call->hMesh];
		RenderMesh(mesh, d3d->deviceContext);
		SetTextureShaderParameters(world, call->hTexture);
		d3d->deviceContext->DrawIndexed(mesh->indexCount, 0, 0);
	}

	world->textureBucket.count = 0;
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
		RenderMesh(mesh, d3d->deviceContext);
		SetLitTextureShaderParameters(world, call->hTexture);
		d3d->deviceContext->DrawIndexed(mesh->indexCount, 0, 0);
	}

	world->litTextureBucket.count = 0;
}

internal HMesh LoadGLBIntoWorld(FRenderWorld* world, const char* filename)
{
	FGLBAsset* asset = ArenaPushSize(world->scratchArena, FGLBAsset);
	ZeroStruct(asset);

	if (!GLB_Load(world->scratchArena, filename, asset))
	{
		Assert(0);	// Check filename, it failed to load!
		return 0;
	}

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
			FLitTextureVertex* converted = (FLitTextureVertex*)ArenaPushArray(world->scratchArena, prim->vertexCount, FLitTextureVertex);
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

	ArenaReset(world->scratchArena);
	GLB_Free(asset);
	return handle;
}

////////////////////////////////////////////////////////////////////////////////
// FCamera
////////////////////////////////////////////////////////////////////////////////
internal void RenderCamera(FCamera* camera, FTransformTable* transforms)
{
	quat q = transforms->rotations[camera->hTransform];

	// Load quaternion directly into DirectXMath.
	DirectX::XMVECTOR quatVector = DirectX::XMVectorSet(q.x, q.y, q.z, q.w);

	// Build rotation matrix from quaternion.
	DXMatrix rotationMatrix = DirectX::XMMatrixRotationQuaternion(quatVector);

	// Position
	v3 pos = transforms->positions[camera->hTransform];
	DirectX::XMVECTOR positionVector = DirectX::XMVectorSet(pos.x, pos.y, pos.z, 0.0f);

	// Default forward and up, rotated by the quaternion matrix.
	DirectX::XMVECTOR lookAtVector = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	DirectX::XMVECTOR upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	lookAtVector = XMVector3TransformCoord(lookAtVector, rotationMatrix);
	upVector = XMVector3TransformCoord(upVector, rotationMatrix);

	lookAtVector = DirectX::XMVectorAdd(positionVector, lookAtVector);

	camera->viewMatrix = DirectX::XMMatrixLookAtLH(positionVector, lookAtVector, upVector);
}

////////////////////////////////////
/// Global Functions
////////////////////////////////////
bool32 Initialize(FRenderWorld* world, FD3DInitParams* d3dInitParams, FTransformTable* transforms)
{
	bool32 result = true;

	result = InitializeFD3D(d3dInitParams, world->scratchArena);
	if (!result)
	{
		MessageBoxW(d3dInitParams->window, L"Could not initialize Direct3D", L"Error", MB_OK);
		return result;
	}

	transforms->positions[world->camera.hTransform] = { 0.0f, 0.0f, -10.0f };

	HMesh hCube = LoadGLBIntoWorld(world, "src\\models\\cube.glb");
	HMesh hMonkey = LoadGLBIntoWorld(world, "src\\models\\monkey.glb");
	HMesh hSphere = LoadGLBIntoWorld(world, "src\\models\\sphere.glb");

	const char* textureFileName = "src\\textures\\mosaic_diffuseoriginal.tga";
	HTexture mosaicTexHandle = LoadTexture(world, world->d3d.device, world->d3d.deviceContext, textureFileName);

	// Init shaders
	result = InitializeColorShader(&world->colorShader, world->d3d.device, d3dInitParams->window);
	if (!result)
	{
		MessageBoxW(d3dInitParams->window, L"Could not initialize the color shader.", L"Error", MB_OK);
		return result;
	}
	result = InitializeTextureShader(&world->textureShader, world->d3d.device, d3dInitParams->window);
	if (!result)
	{
		MessageBoxW(d3dInitParams->window, L"Could not initialize the texture shader.", L"Error", MB_OK);
		return result;
	}
	result = InitializeLitTextureShader(&world->litTextureShader, world->d3d.device, d3dInitParams->window);
	if (!result)
	{
		MessageBoxW(d3dInitParams->window, L"Could not initialize the lit texture shader.", L"Error", MB_OK);
		return result;
	}

	world->litTextureShader.ambientColor = DXFloat4(0.5f, 0.35f, 0.25f, 1.0f);
	world->litTextureShader.diffuseColor = DXFloat4(0.75f, 0.75f, 1.0f, 1.0f);
	world->litTextureShader.lightDirection = DirectX::XMFLOAT3(1.75f, 0.0f, 1.0f);

	return result;
}

bool32 Render(FRenderWorld* world, FTransformTable* transforms)
{
	FD3D* d3d = &world->d3d;

	// Clear the buffers to begin the scene.
	BeginScene(d3d, v4{ 0.0f, 0.0f, 0.0f, 3.0f });

	// Generate the view matrix based on the camera's position.
	RenderCamera(&world->camera, transforms);

	FTexture* tex = &world->textures[0];

	local_presist f32 rot = 0.0f;
	rot -= 0.01f;

	DXMatrix rotMatrix = DirectX::XMMatrixRotationY(rot);
	DXMatrix transMatrix;

	// Render the first mesh, offseted to the left
	transMatrix = DirectX::XMMatrixTranslation(-1.5f, -1.5f, 0.0f);
	DrawTexture(world, 2, 0, DirectX::XMMatrixMultiply(rotMatrix, transMatrix));

	// Render the second mesh, offseted to the right and scaled down
	transMatrix = DirectX::XMMatrixTranslation(-1.5f, 1.5f, 0.0f);
	DrawColor(world, 0, {0.63f, 1, 0.21f, 1}, DirectX::XMMatrixMultiply(rotMatrix, transMatrix));

	transMatrix = DirectX::XMMatrixTranslation(1.5f, 1.5f, 0.0f);
	DrawColor(world, 0, { 1, 0.21f, 0.63f, 1 }, DirectX::XMMatrixMultiply(rotMatrix, transMatrix));

	DXMatrix scaleMatrix = DirectX::XMMatrixScaling(0.75f, 0.75f, 0.75f);
	transMatrix = DirectX::XMMatrixTranslation(1.5f, -1.5f, 0.0f);
	DrawLit(world, 2, 0, DirectX::XMMatrixMultiply(rotMatrix, transMatrix));

	// Flush all buckets — shader bound once per bucket, zero branching.
	FlushColorBucket(world);
	FlushTextureBucket(world);
	FlushLitTextureBucket(world);

	// Present the rendered scene to the screen.
	EndScene(d3d);

	return true;
}