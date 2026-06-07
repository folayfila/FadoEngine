////////////////////////////////////////////////////////////////////////////////
// Filename: color.hlsl
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

// ColorBuffer is on register(b1) because MatrixBuffer occupies b0.
cbuffer ColorBuffer : register(b1)
{
    float4 tintColor;
};

/////////////
// TYPEDEFS //
/////////////
struct VertexInputType
{
    float4 position : POSITION;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType VertexShaderEntry(VertexInputType input)
{
    PixelInputType output;
    
    // Change the position vector to be 4 units for proper matrix calculations.
    // float4 = {x, y, z, w} : w is the pos vector
    input.position.w = 1.0f;
    
    // Calculate the position of the vertex against the world, view, and projection matrices.
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    return output;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 PixelShaderEntry(PixelInputType input) : SV_TARGET
{
    return tintColor;
}

/* Note to future me:
** To fix 1>FXC : error X3501: 'main': entrypoint not found
** Right click the shader file-> select properties-> Does not participate input builds in builds.
*/

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