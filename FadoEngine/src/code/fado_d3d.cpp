#include "fado_d3d.h"

////////////////////////////////////////////////////////////////////////////////
// FD3D
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeFD3D(FD3D* fdirect3D, int32 screenWidth, int32 screenHeight, bool32 vsync, HWND Window, bool32 fullScreen, float screenDepth, float screenNear)
{
	HRESULT result;
	IDXGIFactory* factory;
	IDXGIAdapter* adapter;
	IDXGIOutput* adapterOutput;
	uint32 numModes, numerator, denominator;
	uint64 stringLength;
	DXGI_MODE_DESC* displayModeList;
	DXGI_ADAPTER_DESC adapterDesc;
	int32 error;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	D3D_FEATURE_LEVEL featureLevel;
	ID3D11Texture2D* backBufferPtr;
	D3D11_TEXTURE2D_DESC depthBufferDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	D3D11_RASTERIZER_DESC rasterDesc;
	float fieldOfView, screenAspect;

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
	for (uint32 i = 0; i < numModes; i++)
	{
		if (displayModeList[i].Width == (uint32)screenWidth)
		{
			if (displayModeList[i].Height == (uint32)screenHeight)
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
	fdirect3D->videoCardMemory = (int32)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

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
	float colorArr[4];

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
internal bool32 InitializeColorShader(FColorShaderD3D *colorShader, ID3D11Device* device, HWND window)
{
	bool32 result;
	wchar_t hlslFileName[128];
	int32 error;

	// > TODO: Write a good relative path loader or sth.
	// Set the filename of the hlsl shader.
	error = wcscpy_s(hlslFileName, 128, L"D:\\Dev\\Git\\FadoEngine\\FadoEngine\\src\\shaders\\color.hlsl");
	if (error != 0)
	{
		return false;
	}

	if (GetFileAttributes(hlslFileName) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBox(NULL, L"File not found!", hlslFileName, MB_OK);
	}

	const char* vsEntryFuncName = "ColorVertexShader";
	const char* psEntryFuncName = "ColorPixelShader";

	// Initialize the vertex and pixel (hlsl) shaders.
	ID3D10Blob* errorMessage;
	ID3D10Blob* vertexShaderBuffer;
	ID3D10Blob* pixelShaderBuffer;
	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
	uint32 numElements;
	D3D11_BUFFER_DESC matrixBufferDesc;

	// Compile the vertex shader code.
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, vsEntryFuncName, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&vertexShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		MessageBox(window, hlslFileName, L"Missing Shader File", MB_OK);
		return false;
	}

	// Compile the pixel shader code.
	result = D3DCompileFromFile(hlslFileName, NULL, NULL, psEntryFuncName, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
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

internal bool SetShaderParameters(FColorShaderD3D* colorShader, ID3D11DeviceContext* deviceContext, DirectX::XMMATRIX worldMatrix, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	FMatrixBuffer* dataPtr;
	uint32 bufferNumber;

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

internal bool32 RenderColorShader(FColorShaderD3D* colorShader, ID3D11DeviceContext* deviceContext, int32 indexCount, DirectX::XMMATRIX world, DirectX::XMMATRIX view, DirectX::XMMATRIX projection)
{
	// Set the shader parameters that it will use for rendering.
	if (!SetShaderParameters(colorShader, deviceContext, world, view, projection))
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
// FModelD3D
////////////////////////////////////////////////////////////////////////////////
internal bool32 InitializeModel(FModelD3D *model, ID3D11Device* device)
{
	FVertex* vertices;
	uint32* indices;
	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;

	// First create two temporary arrays to hold the vertex and index data that we will use later to populate the final buffers with.

	// Set the number of vertices in the vertex array and create it.
	model->vertexCount = 3;
	vertices = new FVertex[model->vertexCount];
	if (!vertices)
	{
		return false;
	}

	// Set the number of indices in the index array and create it.
	model->indexCount = 3;
	indices = new uint32[model->indexCount];
	if (!indices)
	{
		return false;
	}

	// Fill both the vertex and index array with the three points of the triangle as well as the index to each of the points in clockwise order of drawing.
	// >> Drawing an RGB Triangle:
	vertices[0].position = DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f);   // Bottom left.
	vertices[0].color = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);	// Blue

	vertices[1].position = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);		// Top middle.
	vertices[1].color = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);	// Red

	vertices[2].position = DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f);	// Bottom right.
	vertices[2].color = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);	// Green

	// Load the index array with data.
	indices[0] = 0;  // Bottom left.
	indices[1] = 1;  // Top middle.
	indices[2] = 2;  // Bottom right.

	// Set up the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(FVertex) * model->vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now create the vertex buffer.
	result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &model->vertexBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Set up the description of the static index buffer.
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(uint32) * model->indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	// Create the index buffer.
	result = device->CreateBuffer(&indexBufferDesc, &indexData, &model->indexBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Release the arrays now that the vertex and index buffers have been created and loaded.
	delete[] vertices;
	vertices = 0;

	delete[] indices;
	indices = 0;

	return true;
}

internal void RenderModel(FModelD3D* model, ID3D11DeviceContext* deviceContext)
{
	uint32 stride;
	uint32 offset;

	// Set vertex buffer stride and offset.
	stride = sizeof(FVertex);
	offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetVertexBuffers(0, 1, &model->vertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetIndexBuffer(model->indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

////////////////////////////////////////////////////////////////////////////////
// FCameraD3D
////////////////////////////////////////////////////////////////////////////////
internal void RenderCamera(FCameraD3D* camera)
{
	DirectX::XMFLOAT3 up, pos, lookAt;
	DirectX::XMVECTOR upVector, positionVector, lookAtVector;
	float yaw, pitch, roll;
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
	const float kDegreeToRadians = 0.0174532925f;
	pitch = camera->rotation.x * kDegreeToRadians;
	yaw = camera->rotation.y * kDegreeToRadians;
	roll = camera->rotation.z++ * kDegreeToRadians;

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
bool32 Render(FApplication* application)
{
	bool32 result = true;

	FD3D* fdirect3D = &application->direct3D;

	// Clear the buffers to begin the scene.
	BeginScene(fdirect3D, color_rgba{ 0.0f, 0.0f, 0.0f, 1.0f });

	// Generate the view matrix based on the camera's position.
	RenderCamera(&application->camera);

	// Get the world, view, and projection matrices from the camera and d3d objects.
	DirectX::XMMATRIX worldMatrix, viewMatrix, projectionMatrix;
	worldMatrix = fdirect3D->worldMatrix;
	viewMatrix = application->camera.viewMatrix;
	projectionMatrix = fdirect3D->projectionMatrix;

	// Put the model vertex and index buffers on the graphics pipeline to prepare them for drawing.
	RenderModel(&application->model, fdirect3D->deviceContext);

	// Render the model using the color shader.
	result = RenderColorShader(&application->colorShader, fdirect3D->deviceContext, application->model.indexCount, worldMatrix, viewMatrix, projectionMatrix);
	if (!result)
	{
		return false;
	}

	// Present the rendered scene to the screen.
	EndScene(fdirect3D);

	return result;
}

bool32 Initialize(FApplication* application, int32 screenWidth, int32 screenHeight, bool32 vsync, HWND window, bool32 fullScreen, float screenDepth, float screenNear)
{
	bool32 result = true;

	FD3D* fdirect3D = &application->direct3D;
	result = InitializeFD3D(fdirect3D, screenWidth, screenHeight, vsync, window, fullScreen, screenDepth, screenNear);
	if (!result)
	{
		MessageBox(window, L"Could not initialize Direct3D", L"Error", MB_OK);
		return result;
	}

	application->camera.position = { 0.0f, 0.0f, -5.0f };

	result = InitializeModel(&application->model, fdirect3D->device);
	if (!result)
	{
		MessageBox(window, L"Could not initialize the model object.", L"Error", MB_OK);
		return result;
	}

	result = InitializeColorShader(&application->colorShader, fdirect3D->device, window);
	if (!result)
	{
		MessageBox(window, L"Could not initialize the color shader object.", L"Error", MB_OK);
		return result;
	}

	return result;
}
