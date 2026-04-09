////////////////////////////////////////////////////////////////////////////////
// Filename: color.vs
////////////////////////////////////////////////////////////////////////////////

/////////////
// GLOBALS //
/////////////
cbuffer MatrixBuffer
{
	matrix worldMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
};

/////////////
// TYPEDEFS //
/////////////
struct VertexInputType
{
	float4 position : POSITION;
    float4 color : COLOR;
};

struct PixelInputType
{
    float4 position : SV_POSITION;	// SV = System Value
    float4 color : COLOR;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType ColorVertexShader(VertexInputType input)
{
    PixelInputType output;
    
    // Change the position vector to be 4 units for proper matrix calculations.
    // float4 = {x, y, z, w} : w is the pos vector
    input.position.w = 1.0f;
    
    // Calculate the position of the vertex against the world, view, and projection matrices.
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    // Store the input color for the pixel shader to use.
    output.color = input.color;
    
    return output;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 ColorPixelShader(PixelInputType input) : SV_TARGET
{
    float4 color = input.color;
    return color;
}

/*
* To have them both input one file instead of .vs and .ps:
* fxc /T vs_5_0 /E ColorVertexShader ColorShader.hlsl /Fo ColorVS.cso
* fxc /T ps_5_0 /E ColorPixelShader ColorShader.hlsl /Fo ColorPS.cso
*
*** /T vs_5_0 -> target vertex shader model5.0
*** /T ps_5_0 -> target pixel shader model5.0
*** /E -> entrypoint function name
*** Fo -> output compiled shader object file
*
*/