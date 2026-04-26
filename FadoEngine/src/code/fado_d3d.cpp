#include "fado_d3d.h"
#include "../code/glb/fado_glb.h"

///////////////////////////
// Constants
///////////////////////////
// Unified vertex and pixel shaders entry points.
const char* k_vsEntryFuncName = "VertexShaderEntry";
const char* k_psEntryFuncName = "PixelShaderEntry";

////////////////////////////////////////////////////////////////////////////////
// FD3D
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeFD3D(FD3D* fdirect3D, i32 screenWidth, i32 screenHeight, bool32 vsync, HWND Window, bool32 fullScreen, f32 screenDepth, f32 screenNear)
{
	HRESULT result;
	IDXGIFactory* factory;
	IDXGIAdapter* adapter;
	IDXGIOutput* adapterOutput;
	u32 numModes, numerator, denominator;
	u64 stringLength;
	DXGI_MODE_DESC* displayModeList;
	DXGI_ADAPTER_DESC adapterDesc;
	i32 error;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	D3D_FEATURE_LEVEL featureLevel;
	ID3D11Texture2D* backBufferPtr;
	D3D11_TEXTURE2D_DESC depthBufferDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	D3D11_RASTERIZER_DESC rasterDesc;
	f32 fieldOfView, screenAspect;

	// Store the vsync setting.
	fdirect3D->vsyncEnabled = vsync;

	// Create a DirectX graphics interface factory.
	result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(result))
	{
		return false;
	}

	// Use the factory to create an adapter for the primary graphics interface (video card).
	result = factory->EnumAdapters(0, &adapter);
	if (FAILED(result))
	{
		return false;
	}

	// Enumerate the primary adapter output (monitor).
	result = adapter->EnumOutputs(0, &adapterOutput);
	if (FAILED(result))
	{
		return false;
	}

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	if (FAILED(result))
	{
		return false;
	}

	// Create a list to hold all the possible display modes for this monitor/video card combination.
	displayModeList = new DXGI_MODE_DESC[numModes];
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
	for (u32 i = 0; i < numModes; i++)
	{
		if (displayModeList[i].Width == (u32)screenWidth)
		{
			if (displayModeList[i].Height == (u32)screenHeight)
			{
				numerator = displayModeList[i].RefreshRate.Numerator;
				denominator = displayModeList[i].RefreshRate.Denominator;
			}
		}
	}

	// Get the adapter (video card) description.
	result = adapter->GetDesc(&adapterDesc);
	if (FAILED(result))
	{
		return false;
	}

	// Store the dedicated video card memory in megabytes.
	fdirect3D->videoCardMemory = (i32)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	// Convert the name of the video card to a character array and store it.
	error = wcstombs_s(&stringLength, fdirect3D->videoCardDescription, 128, adapterDesc.Description, 128);
	if (error != 0)
	{
		return false;
	}

	// Release the display mode list.
	delete[] displayModeList;
	displayModeList = 0;

	// Release the adapter output.
	adapterOutput->Release();
	adapterOutput = 0;

	// Release the adapter.
	adapter->Release();
	adapter = 0;

	// Release the factory.
	factory->Release();
	factory = 0;

	// Initialize the swap chain description.
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	// Set to a single back buffer.
	swapChainDesc.BufferCount = 1;

	// Set the width and height of the back buffer.
	swapChainDesc.BufferDesc.Width = screenWidth;
	swapChainDesc.BufferDesc.Height = screenHeight;

	// Set regular 32-bit surface for the back buffer.
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Set the refresh rate of the back buffer.
	if (fdirect3D->vsyncEnabled)
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
	swapChainDesc.OutputWindow = Window;

	// Turn multisampling off.
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;

	// Set to full screen or windowed mode.
	swapChainDesc.Windowed = !fullScreen;

	// Set the scan line ordering and scaling to unspecified.
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Don't set the advanced flags.
	swapChainDesc.Flags = 0;

	// Set the feature level to DirectX 11.
	featureLevel = D3D_FEATURE_LEVEL_11_0;

	// Create the swap chain, Direct3D device, and Direct3D device context.
	result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1,
		D3D11_SDK_VERSION, &swapChainDesc, &fdirect3D->swapChain, &fdirect3D->device, NULL, &fdirect3D->deviceContext);
	if (FAILED(result))
	{
		return false;
	}

	// Get the pointer to the back buffer.
	result = fdirect3D->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferPtr);
	if (FAILED(result))
	{
		return false;
	}

	// Create the render target view with the back buffer pointer.
	result = fdirect3D->device->CreateRenderTargetView(backBufferPtr, NULL, &fdirect3D->renderTargetView);
	if (FAILED(result))
	{
		return false;
	}

	// Release pointer to the back buffer as we no longer need it.
	backBufferPtr->Release();
	backBufferPtr = 0;

	// Initialize the description of the depth buffer.
	ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

	// Set up the description of the depth buffer.
	depthBufferDesc.Width = screenWidth;
	depthBufferDesc.Height = screenHeight;
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
	result = fdirect3D->device->CreateTexture2D(&depthBufferDesc, NULL, &fdirect3D->depthStencilBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Initialize the description of the stencil state.
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

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
	result = fdirect3D->device->CreateDepthStencilState(&depthStencilDesc, &fdirect3D->depthStencilState);
	if (FAILED(result))
	{
		return false;
	}

	// Set the depth stencil state.
	fdirect3D->deviceContext->OMSetDepthStencilState(fdirect3D->depthStencilState, 1);

	// Initialize the depth stencil view.
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	// Set up the depth stencil view description.
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	result = fdirect3D->device->CreateDepthStencilView(fdirect3D->depthStencilBuffer, &depthStencilViewDesc, &fdirect3D->depthStencilView);
	if (FAILED(result))
	{
		return false;
	}

	// Bind the render target view and depth stencil buffer to the output render pipeline.
	fdirect3D->deviceContext->OMSetRenderTargets(1, &fdirect3D->renderTargetView, fdirect3D->depthStencilView);

	// Setup the raster description which will determine how and what polygons will be drawn.
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
	result = fdirect3D->device->CreateRasterizerState(&rasterDesc, &fdirect3D->rasterState);
	if (FAILED(result))
	{
		return false;
	}

	// Now set the rasterizer state.
	fdirect3D->deviceContext->RSSetState(fdirect3D->rasterState);

	// Setup the viewport for rendering.
	fdirect3D->viewport.Width = (float)screenWidth;
	fdirect3D->viewport.Height = (float)screenHeight;
	fdirect3D->viewport.MinDepth = 0.0f;
	fdirect3D->viewport.MaxDepth = 1.0f;
	fdirect3D->viewport.TopLeftX = 0.0f;
	fdirect3D->viewport.TopLeftY = 0.0f;

	// Create the viewport.
	fdirect3D->deviceContext->RSSetViewports(1, &fdirect3D->viewport);

	// Setup the projection matrix.
	fieldOfView = Pi32 / 4.0f;
	screenAspect = (float)screenWidth / (float)screenHeight;

	// Create the projection matrix for 3D rendering.
	fdirect3D->projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fieldOfView, screenAspect, screenNear, screenDepth);

	// Initialize the world matrix to the identity matrix.
	fdirect3D->worldMatrix = DirectX::XMMatrixIdentity();

	// Create an orthographic projection matrix for 2D rendering.
	fdirect3D->orthoMatrix = DirectX::XMMatrixOrthographicLH((float)screenWidth, (float)screenHeight, screenNear, screenDepth);

	return true;
}

internal void BeginScene(FD3D *fdirect3D, color_rgba color)
{
	f32 colorArr[4];

	// Setup the color to clear the buffer to.
	colorArr[0] = color.r;
	colorArr[1] = color.g;
	colorArr[2] = color.b;
	colorArr[3] = color.a;

	// Clear the back buffer.
	fdirect3D->deviceContext->ClearRenderTargetView(fdirect3D->renderTargetView, colorArr);

	// Clear the depth buffer.
	fdirect3D->deviceContext->ClearDepthStencilView(fdirect3D->depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

internal void EndScene(FD3D* fdirect3D)
{
	// Present the back buffer to the screen since rendering is complete.
	if (fdirect3D->vsyncEnabled)
	{
		// Lock to screen refresh rate.
		fdirect3D->swapChain->Present(1, 0);
	}
	else
	{
		// Present as fast as possible.
		fdirect3D->swapChain->Present(0, 0);
	}
}

internal void SetBackBufferRenderTarget(FD3D *fd3d)
{
	// Bind the render target view and depth stencil buffer to the output render pipeline.
	fd3d->deviceContext->OMSetRenderTargets(1, &fd3d->renderTargetView, fd3d->depthStencilView);
}

internal void ResetViewport(FD3D *fdirect3D)
{
	// Set the viewport.
	fdirect3D->deviceContext->RSSetViewports(1, &fdirect3D->viewport);
}

////////////////////////////////////////////////////////////////////////////////
// FColorShaderD3D
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeColorShader(FColorShader *colorShader, ID3D11Device* device, HWND window)
{
	bool32 result;
	wchar hlslFileName[128];
	i32 error;

	// > TODO: Write a good relative path loader or sth.
	// Set the filename of the hlsl shader.
	error = wcscpy_s(hlslFileName, 128, L"src\\shaders\\color.hlsl");
	if (error != 0)
	{
		return false;
	}

	if (GetFileAttributes(hlslFileName) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBox(NULL, L"File not found!", hlslFileName, MB_OK);
	}

	// Initialize the vertex and pixel (hlsl) shaders.
	ID3D10Blob* errorMessage;
	ID3D10Blob* vertexShaderBuffer;
	ID3D10Blob* pixelShaderBuffer;
	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
	u32 numElements;
	D3D11_BUFFER_DESC matrixBufferDesc;

	// Compile the vertex shader code.
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_vsEntryFuncName, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&vertexShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBox(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Compile the pixel shader code.
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_psEntryFuncName, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&pixelShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBox(window, hlslFileName, L"Missing Shader File", MB_OK);
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
	// This setup needs to match the VertexType stucture in the ModelClass and in the shader.
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "COLOR";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

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

	return true;
}

internal bool32 SetColorShaderParameters(FColorShader* colorShader, ID3D11DeviceContext* deviceContext, DirectX::XMMATRIX worldMatrix, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	FMatrixBuffer* dataPtr;
	u32 bufferNumber;

	// Transpose the matrices to prepare them for the shader.
	worldMatrix = XMMatrixTranspose(worldMatrix);
	viewMatrix = XMMatrixTranspose(viewMatrix);
	projectionMatrix = XMMatrixTranspose(projectionMatrix);

	// Lock the constant buffer so it can be written to.
	result = deviceContext->Map(colorShader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		return false;
	}

	// Get a pointer to the data in the constant buffer.
	dataPtr = (FMatrixBuffer*)mappedResource.pData;

	// Copy the matrices into the constant buffer.
	dataPtr->world = worldMatrix;
	dataPtr->view = viewMatrix;
	dataPtr->projection = projectionMatrix;

	// Unlock the constant buffer.
	deviceContext->Unmap(colorShader->matrixBuffer, 0);

	// Set the position of the constant buffer in the vertex shader.
	bufferNumber = 0;

	// Finanly set the constant buffer in the vertex shader with the updated values.
	deviceContext->VSSetConstantBuffers(bufferNumber, 1, &colorShader->matrixBuffer);

	return true;
}

internal bool32 RenderColorShader(FColorShader* colorShader, ID3D11DeviceContext* deviceContext, i32 indexCount, DirectX::XMMATRIX world, DirectX::XMMATRIX view, DirectX::XMMATRIX projection)
{
	// Set the shader parameters that it will use for rendering.
	if (!SetColorShaderParameters(colorShader, deviceContext, world, view, projection))
	{
		return false;
	}

	// Now render the prepared buffers with the shader.
	// Set the vertex input layout.
	deviceContext->IASetInputLayout(colorShader->layout);

	// Set the vertex and pixel shaders that will be used to render this triangle.
	deviceContext->VSSetShader(colorShader->vertexShader, NULL, 0);
	deviceContext->PSSetShader(colorShader->pixelShader, NULL, 0);

	// Render the triangle.
	deviceContext->DrawIndexed(indexCount, 0, 0);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// FTextureShaderD3D
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeTextureShader(FTextureShader* textureShader, ID3D11Device* device, HWND window)
{
	bool32 result;
	wchar hlslFileName[128];
	i32 error;

	// > TODO: Write a good relative path loader or sth.
	// Set the filename of the hlsl shader.
	error = wcscpy_s(hlslFileName, 128, L"src\\shaders\\texture.hlsl");
	if (error != 0)
	{
		return false;
	}

	if (GetFileAttributes(hlslFileName) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBox(NULL, L"File not found!", hlslFileName, MB_OK);
	}

	// Initialize the vertex and pixel (hlsl) shaders.
	ID3D10Blob* errorMessage;
	ID3D10Blob* vertexShaderBuffer;
	ID3D10Blob* pixelShaderBuffer;
	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
	u32 numElements;
	D3D11_BUFFER_DESC matrixBufferDesc;

	// Compile the vertex shader code.
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_vsEntryFuncName, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&vertexShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBox(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Compile the pixel shader code.
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_psEntryFuncName, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&pixelShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBox(window, hlslFileName, L"Missing Shader File", MB_OK);
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
	// This setup needs to match the VertexType stucture in the ModelClass and in the shader.
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
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

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

internal bool32 SetTextureShaderParameters(FTextureShader* textureShader, ID3D11DeviceContext* deviceContext, DirectX::XMMATRIX worldMatrix, DirectX::XMMATRIX viewMatrix,
	DirectX::XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	FMatrixBuffer* dataPtr;
	u32 bufferNumber;

	// Transpose the matrices to prepare them for the shader.
	worldMatrix = XMMatrixTranspose(worldMatrix);
	viewMatrix = XMMatrixTranspose(viewMatrix);
	projectionMatrix = XMMatrixTranspose(projectionMatrix);

	// Lock the constant buffer so it can be written to.
	result = deviceContext->Map(textureShader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		return false;
	}

	// Get a pointer to the data in the constant buffer.
	dataPtr = (FMatrixBuffer*)mappedResource.pData;

	// Copy the matrices into the constant buffer.
	dataPtr->world = worldMatrix;
	dataPtr->view = viewMatrix;
	dataPtr->projection = projectionMatrix;

	// Unlock the constant buffer.
	deviceContext->Unmap(textureShader->matrixBuffer, 0);

	// Set the position of the constant buffer in the vertex shader.
	bufferNumber = 0;

	// Finanly set the constant buffer in the vertex shader with the updated values.
	deviceContext->VSSetConstantBuffers(bufferNumber, 1, &textureShader->matrixBuffer);

	// Set shader texture resource in the pixel shader.
	deviceContext->PSSetShaderResources(0, 1, &texture);

	return true;
}

internal bool32 RenderTextureShader(FTextureShader* textureShader, ID3D11DeviceContext* deviceContext, i32 indexCount,
	DirectX::XMMATRIX world, DirectX::XMMATRIX view, DirectX::XMMATRIX projection, ID3D11ShaderResourceView* texture)
{
	// Set the shader parameters that it will use for rendering.
	if (!SetTextureShaderParameters(textureShader, deviceContext, world, view, projection, texture))
	{
		return false;
	}

	// Set the vertex input layout.
	deviceContext->IASetInputLayout(textureShader->layout);

	// Set the vertex and pixel shaders that will be used to render this triangle.
	deviceContext->VSSetShader(textureShader->vertexShader, NULL, 0);
	deviceContext->PSSetShader(textureShader->pixelShader, NULL, 0);

	// Set the sampler state in the pixel shader.
	deviceContext->PSSetSamplers(0, 1, &textureShader->sampleState);

	// Render.
	deviceContext->DrawIndexed(indexCount, 0, 0);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// FLightShaderD3D
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeLightShader(FTextureLightShader* lightShader, ID3D11Device* device, HWND window)
{
	bool32 result;
	wchar hlslFileName[128];
	i32 error;

	// > TODO: Write a good relative path loader or sth.
	// Set the filename of the hlsl shader.
	error = wcscpy_s(hlslFileName, 128, L"src\\shaders\\light.hlsl");
	if (error != 0)
	{
		return false;
	}

	if (GetFileAttributes(hlslFileName) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBox(NULL, L"File not found!", hlslFileName, MB_OK);
	}

	// Initialize the vertex and pixel (hlsl) shaders.
	ID3D10Blob* errorMessage;
	ID3D10Blob* vertexShaderBuffer;
	ID3D10Blob* pixelShaderBuffer;
	D3D11_INPUT_ELEMENT_DESC polygonLayout[3];
	u32 numElements;
	D3D11_BUFFER_DESC matrixBufferDesc;

	// Compile the vertex shader code.
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_vsEntryFuncName, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&vertexShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBox(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Compile the pixel shader code.
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, k_psEntryFuncName, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&pixelShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBox(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Create the vertex shader from the buffer.
	result = device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &lightShader->vertexShader);
	if (FAILED(result))
	{
		return false;
	}

	// Create the pixel shader from the buffer.
	result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &lightShader->pixelShader);
	if (FAILED(result))
	{
		return false;
	}

	// Create the vertex input layout description.
	// This setup needs to match the VertexType stucture in the ModelClass and in the shader.
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
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	polygonLayout[2].SemanticName = "NORMAL";
	polygonLayout[2].SemanticIndex = 0;
	polygonLayout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[2].InputSlot = 0;
	polygonLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[2].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	// Create the vertex input layout.
	result = device->CreateInputLayout(polygonLayout, numElements, vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &lightShader->layout);
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
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(FMatrixBuffer);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = device->CreateBuffer(&matrixBufferDesc, NULL, &lightShader->matrixBuffer);
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
	result = device->CreateSamplerState(&samplerDesc, &lightShader->sampleState);
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
	result = device->CreateBuffer(&lightBufferDesc, NULL, &lightShader->lightBuffer);
	if (FAILED(result))
	{
		return false;
	}

	return true;
}

internal bool32 SetLightShaderParameters(FTextureLightShader* lightShader, ID3D11DeviceContext* deviceContext,
	DirectX::XMMATRIX worldMatrix, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix,
	ID3D11ShaderResourceView* texture, DirectX::XMFLOAT3 lightDirection, DirectX::XMFLOAT4 diffuseColor)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	FMatrixBuffer* dataPtr;
	FLightBuffer* lightDataPtr;
	u32 bufferNumber;

	// Transpose the matrices to prepare them for the shader.
	worldMatrix = XMMatrixTranspose(worldMatrix);
	viewMatrix = XMMatrixTranspose(viewMatrix);
	projectionMatrix = XMMatrixTranspose(projectionMatrix);

	// Lock the constant buffer so it can be written to.
	result = deviceContext->Map(lightShader->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		return false;
	}

	// Get a pointer to the data in the constant buffer.
	dataPtr = (FMatrixBuffer*)mappedResource.pData;

	// Copy the matrices into the constant buffer.
	dataPtr->world = worldMatrix;
	dataPtr->view = viewMatrix;
	dataPtr->projection = projectionMatrix;

	// Unlock the constant buffer.
	deviceContext->Unmap(lightShader->matrixBuffer, 0);

	// Set the position of the constant buffer in the vertex shader.
	bufferNumber = 0;

	// Finanly set the constant buffer in the vertex shader with the updated values.
	deviceContext->VSSetConstantBuffers(bufferNumber, 1, &lightShader->matrixBuffer);

	// Set shader texture resource in the pixel shader.
	deviceContext->PSSetShaderResources(0, 1, &texture);

	// Lock the light constant buffer so it can be written to.
	result = deviceContext->Map(lightShader->lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		return false;
	}

	// Get a pointer to the data in the constant buffer.
	lightDataPtr = (FLightBuffer*)mappedResource.pData;

	// Copy the lighting variables into the constant buffer.
	lightDataPtr->diffuseColor = diffuseColor;
	lightDataPtr->lightDirection = lightDirection;
	lightDataPtr->padding = 0.0f;

	// Unlock the constant buffer.
	deviceContext->Unmap(lightShader->lightBuffer, 0);

	// Set the position of the light constant buffer in the pixel shader.
	bufferNumber = 0;

	// Finally set the light constant buffer in the pixel shader with the updated values.
	deviceContext->PSSetConstantBuffers(bufferNumber, 1, &lightShader->lightBuffer);

	return true;
}

internal bool32 RenderLightShader(FTextureLightShader* lightShader, ID3D11DeviceContext* deviceContext, i32 indexCount,
	DirectX::XMMATRIX world, DirectX::XMMATRIX view, DirectX::XMMATRIX projection,
	ID3D11ShaderResourceView* texture, DirectX::XMFLOAT3 lightDirection, DirectX::XMFLOAT4 diffuseColor)
{
	// Set the shader parameters that it will use for rendering.
	if (!SetLightShaderParameters(lightShader, deviceContext, world, view, projection, texture, lightDirection, diffuseColor))
	{
		return false;
	}

	// Set the vertex input layout.
	deviceContext->IASetInputLayout(lightShader->layout);

	// Set the vertex and pixel shaders that will be used to render this triangle.
	deviceContext->VSSetShader(lightShader->vertexShader, NULL, 0);
	deviceContext->PSSetShader(lightShader->pixelShader, NULL, 0);

	// Set the sampler state in the pixel shader.
	deviceContext->PSSetSamplers(0, 1, &lightShader->sampleState);

	// Render.
	deviceContext->DrawIndexed(indexCount, 0, 0);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// FTexture
////////////////////////////////////////////////////////////////////////////////

bool32 LoadTarga32BitIntoTexture(const char* filename, FTexture* tex)
{
	i32 error, bpp, imageSize;
	FILE* filePtr;
	u32 count;
	FTargaHeader targaFileHeader;
	u8* targaImage;

	// Open the targa file for reading in binary.
	error = fopen_s(&filePtr, filename, "rb");
	if (error != 0)
	{
		return false;
	}

	// Read in the file header.
	count = (u32)fread(&targaFileHeader, sizeof(FTargaHeader), 1, filePtr);
	if (count != 1)
	{
		return false;
	}

	// Get the important information from the header.
	tex->height = (i32)targaFileHeader.height;
	tex->width = (i32)targaFileHeader.width;
	bpp = (i32)targaFileHeader.bpp;

	// Allow both 24 and 32 bit. 24 will have a 255 value for alpha.
	if (bpp != 24 && bpp != 32)
	{
		return false;
	}

	// Calculate the size of the 32 bit image data.
	i32 bytesPerPixel = bpp / 8;;
	imageSize = tex->width * tex->height * bytesPerPixel;

	// Allocate memory for the targa image data.
	targaImage = new u8[imageSize];

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
internal void MakeTriangle(FTextureLightVertex* vertices, u32* indices)
{
	// Load the vertex array with data.
	vertices[0].position = DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f);  // Bottom left.
	vertices[0].texture = DirectX::XMFLOAT2(0.0f, 1.0f);
	vertices[0].normal = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);

	vertices[1].position = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);  // Top middle.
	vertices[1].texture = DirectX::XMFLOAT2(0.5f, 0.0f);
	vertices[1].normal = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);

	vertices[2].position = DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f);  // Bottom right.
	vertices[2].texture = DirectX::XMFLOAT2(1.0f, 1.0f);
	vertices[2].normal = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);

	// Load the index array with data.
	indices[0] = 0;  // Bottom left.
	indices[1] = 1;  // Top middle.
	indices[2] = 2;  // Bottom right.
}

internal void MakeQuad(FTextureLightVertex* vertices, u32* indices)
{
	// Fill both the vertex and index array with the three points of the triangle as well as the index to each of the points in clockwise order of drawing.
	vertices[0].position = DirectX::XMFLOAT3(-1.0f, 1.0f, 0.0f);  // Top left
	vertices[0].texture = DirectX::XMFLOAT2(0.0f, 0.0f);
	vertices[0].normal = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);

	vertices[1].position = DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f);  // Top right
	vertices[1].texture = DirectX::XMFLOAT2(1.0f, 0.0f);
	vertices[1].normal = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);

	vertices[2].position = DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f);  // Bottom left
	vertices[2].texture = DirectX::XMFLOAT2(0.0f, 1.0f);
	vertices[2].normal = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);

	vertices[3].position = DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f);  // Bottom right
	vertices[3].texture = DirectX::XMFLOAT2(1.0f, 1.0f);
	vertices[3].normal = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);

	indices[0] = 0; indices[1] = 1; indices[2] = 2;  // Triangle 1
	indices[3] = 2; indices[4] = 1; indices[5] = 3;  // Triangle 2
}

internal bool32 UploadMesh(FMeshBuffer *mesh, ID3D11Device* device, FTextureLightVertex* vertices, u32 vCount, u32* indices, u32 iCount)
{
	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;

	mesh->vertexCount = vCount;
	mesh->indexCount = iCount;

	// Set up the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(FTextureLightVertex) * vCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now create the vertex buffer.
	result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &mesh->vertexBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Set up the description of the static index buffer.
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(u32) * iCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	// Create the index buffer.
	result = device->CreateBuffer(&indexBufferDesc, &indexData, &mesh->indexBuffer);
	if (FAILED(result))
	{
		return false;
	}

	return true;
}

internal HMesh LoadMesh(FRenderWorld* world, ID3D11Device* device, FTextureLightVertex* verts, u32 vCount, u32* indices, u32 iCount)
{
	HMesh handle = world->meshCount++;
	UploadMesh(&world->meshes[handle], device, verts, vCount, indices, iCount);
	return handle;
}

internal HTexture LoadTexture(FRenderWorld* world, ID3D11Device* device, ID3D11DeviceContext* context, const char* fileName)
{
	HTexture handle = world->texturesCount++;
	InitializeTexture(&world->textures[handle], device, context, fileName);
	return handle;
}

internal void RenderMesh(FMeshBuffer* mesh, ID3D11DeviceContext* deviceContext)
{
	u32 stride;
	u32 offset;

	// Set vertex buffer stride and offset.
	stride = sizeof(FTextureLightVertex);
	offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetVertexBuffers(0, 1, &mesh->vertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetIndexBuffer(mesh->indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

internal u32 LoadGLBIntoWorld(FRenderWorld* world, const char* filename, HMesh* outHandles, u32 maxHandles)
{
	FGLBAsset asset = {};
	if (!GLB_Load(filename, &asset))
	{
		return 0;
	}

	u32 handleCount = 0;

	// Walk every mesh -> every primitive
	for (u32 mi = 0; mi < asset.meshCount; mi++)
	{
		FGLBMesh* mesh = &asset.meshes[mi];

		for (u32 pi = 0; pi < mesh->primitiveCount; pi++)
		{
			FGLBPrimitive* prim = &mesh->primitives[pi];
			if (!prim->vertices || !prim->indices || prim->vertexCount == 0)
			{
				continue;
			}

			if (handleCount >= maxHandles)
			{
				break;
			}

			// LoadMesh() is your existing function that calls UploadMesh()
			// and returns an HMesh handle (uint32 index into world->meshes[])
			//
			// NOTE: FGLBVertex and FTextureVertex need to match in layout.
			// FGLBVertex has position(xyz), normal(xyz), uv(uv).
			// If your FTextureVertex only has position+uv, either:
			//   a) cast FGLBVertex* and adjust stride  — simple but skips normals
			//   b) convert to FTextureVertex[] first   — shown below
			//   c) update FTextureVertex to include normals — recommended

			// Option (b): convert to FTextureVertex (pos + uv only, normals dropped)
			FTextureLightVertex* converted = (FTextureLightVertex*)malloc(prim->vertexCount * sizeof(FTextureLightVertex));
			if (!converted) continue;

			for (u32 v = 0; v < prim->vertexCount; v++)
			{
				converted[v].position = DirectX::XMFLOAT3(
					prim->vertices[v].px,
					prim->vertices[v].py,
					prim->vertices[v].pz);
				converted[v].texture = DirectX::XMFLOAT2(
					prim->vertices[v].u,
					prim->vertices[v].v);
			}

			HMesh handle = LoadMesh(world, world->d3d.device, converted, prim->vertexCount, prim->indices, prim->indexCount);
			free(converted);

			outHandles[handleCount++] = handle;
		}
	}

	GLB_Free(&asset);
	return handleCount;
}

////////////////////////////////////////////////////////////////////////////////
// FCameraD3D
////////////////////////////////////////////////////////////////////////////////
internal void RenderCamera(FCamera* camera)
{
	DirectX::XMFLOAT3 up, pos, lookAt;
	DirectX::XMVECTOR upVector, positionVector, lookAtVector;
	f32 yaw, pitch, roll;
	DirectX::XMMATRIX rotationMatrix;

	// Setup the vector that points upwards.
	up.x = 0.0f;
	up.y = 1.0f;
	up.z = 0.0f;

	// Load it into a XMVECTOR structure.
	upVector = XMLoadFloat3(&up);

	// Setup the position of the camera in the world.
	pos.x = camera->position.x;
	pos.y = camera->position.y;
	pos.z = camera->position.z;

	// Load it into a XMVECTOR structure.
	positionVector = XMLoadFloat3(&pos);

	// Setup where the camera is looking by default.
	lookAt.x = 0.0f;
	lookAt.y = 0.0f;
	lookAt.z = 1.0f;

	// Load it into a XMVECTOR structure.
	lookAtVector = XMLoadFloat3(&lookAt);

	// Set the yaw (Y axis), pitch (X axis), and roll (Z axis) rotations in radians.
	// Degrees to radians conversion: π/180 ≈ 0.0174532925.
	// DirectXMath rotation functions expect radians, so rotation values are multiplied by that constant.
	const f32 kDegreeToRadians = 0.0174532925f;
	pitch = camera->rotation.x * kDegreeToRadians;
	yaw = camera->rotation.y * kDegreeToRadians;
	roll = camera->rotation.z * kDegreeToRadians;

	// Create the rotation matrix from the yaw, pitch, and roll values.
	rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	// Transform the lookAt and up vector by the rotation matrix so the view is correctly rotated at the origin.
	lookAtVector = XMVector3TransformCoord(lookAtVector, rotationMatrix);
	upVector = XMVector3TransformCoord(upVector, rotationMatrix);

	// Translate the rotated camera position to the location of the viewer.
	lookAtVector = DirectX::XMVectorAdd(positionVector, lookAtVector);

	// Finally create the view matrix from the three updated vectors.
	camera->viewMatrix = DirectX::XMMatrixLookAtLH(positionVector, lookAtVector, upVector);
}

////////////////////////////////////
/// Global Functions
////////////////////////////////////
bool32 Initialize(FRenderWorld* world, i32 screenWidth, i32 screenHeight, bool32 vsync, HWND window, bool32 fullScreen, f32 screenDepth, f32 screenNear)
{
	bool32 result = true;

	FD3D* d3d = &world->d3d;
	result = InitializeFD3D(d3d, screenWidth, screenHeight, vsync, window, fullScreen, screenDepth, screenNear);
	if (!result)
	{
		MessageBox(window, L"Could not initialize Direct3D", L"Error", MB_OK);
		return result;
	}

	world->camera.position = { 0.0f, 0.0f, -5.0f };

	// > Note: Geometry stays here for now until we load from files.
#define DRAW_TRIANGLE 0
#if DRAW_TRIANGLE
#define VCOUNT 3
#define ICOUNT 3
#else
#define VCOUNT 4
#define ICOUNT 6
#endif

	FTextureLightVertex vertices[VCOUNT] = {};
	u32 indices[ICOUNT] = {};

#if DRAW_TRIANGLE
	MakeTriangle(vertices, indices);
#else
	MakeQuad(vertices, indices);
#endif

	// Load a GLB model — up to 64 primitives
	HMesh meshHandles[64] = {};
	u32 meshCount = LoadGLBIntoWorld(world, "src\\models\\cube.glb", meshHandles, 64);
	if (meshCount == 0)
	{
		MessageBox(window, L"Could not load suzanne.glb", L"Error", MB_OK);
		return false;
	}

	const char* textureFileName = "src\\textures\\mosaic_diffuseoriginal.tga";
	HTexture tex = LoadTexture(world, d3d->device, d3d->deviceContext, textureFileName);

	world->texLightShader.diffuseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	world->texLightShader.lightDirection = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
	result = InitializeLightShader(&world->texLightShader, d3d->device, window);
	if (!result)
	{
		MessageBox(window, L"Could not initialize the texture shader.", L"Error", MB_OK);
		return result;
	}

	return result;
}

bool32 Render(FRenderWorld* world)
{
	bool32 result = true;

	FD3D* d3d = &world->d3d;

	// Clear the buffers to begin the scene.
	BeginScene(d3d, color_rgba{ 0.0f, 0.0f, 0.0f, 3.0f });

	// Generate the view matrix based on the camera's position.
	RenderCamera(&world->camera);

	FMeshBuffer* mesh = &world->meshes[0];
	FTexture* tex = &world->textures[0];

	// Put the model vertex and index buffers on the graphics pipeline to prepare them for drawing.
	RenderMesh(mesh, d3d->deviceContext);

	local_presist f32 rot = 0.0f;
	rot -= 0.01f;
	d3d->worldMatrix = DirectX::XMMatrixRotationY(rot);

	// Render the model using the color shader.
	result = RenderLightShader(&world->texLightShader, d3d->deviceContext, mesh->indexCount,
		d3d->worldMatrix, world->camera.viewMatrix, d3d->projectionMatrix, tex->textureView,
		world->texLightShader.lightDirection, world->texLightShader.diffuseColor);

	if (!result)
	{
		return false;
	}

	// Present the rendered scene to the screen.
	EndScene(d3d);

	return result;
}