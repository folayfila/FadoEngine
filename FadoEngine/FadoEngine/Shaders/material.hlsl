////////////////////////////////////////////////////////////////////////////////
// Filename: material.hlsl
////////////////////////////////////////////////////////////////////////////////

/////////////
// GLOBALS //
/////////////

// Constant buffers are bound to "registers" (slots) from C++.
//
// Registers are essentially booked GPU slots for stuff we want to set from the CPU (c++).
// The registers are PER SHADER STAGE, meaning:
//
// Vertex Shader (VS)
//   b0, b1, b2, ...
//
// Pixel Shader (PS)
//   b0, b1, b2, ...
//
// are completely independent.
//
// Example:
//
// C++
//   VSSetConstantBuffers(0, ...) -> VS b0
//   PSSetConstantBuffers(0, ...) -> PS b0
//
// These do NOT conflict with each other.

 // Vertex Shader constant buffer slot 0
cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

// Pixel Shader constant buffer slot 0
cbuffer LightBuffer : register(b0)
{
    float4 ambientColor;
    float4 diffuseColor;
    float3 lightDirection;
    float padding;
};

// Pixel Shader constant buffer slot 1
cbuffer MaterialBuffer : register(b1)
{
    float4 color;
    int hasTexture;
    int isLit;
    float2 _pad;
};

// Resource registers:
//
// b# = Constant Buffers (cbuffer)
// t# = Textures (Shader Resource Views)
// s# = Samplers
// u# = UAVs (RW textures/buffers)
//
// These are also per shader stage.
//
// C++
//   PSSetShaderResources(0, ...) -> t0
//   PSSetSamplers(0, ...)        -> s0

Texture2D ShaderTexture : register(t0);
SamplerState SampleType : register(s0);

/////////////
// TYPEDEFS //
/////////////
struct VertexInput
{
    float4 position : POSITION;
    float3 normal :   NORMAL;
    float2 tex :      TEXCOORD0;
};

struct PixelInput
{
    float4 position : SV_POSITION; // SV = System Value
    float3 normal :   NORMAL;
    float2 tex :      TEXCOORD0;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInput VertexShaderEntry(VertexInput input)
{
    PixelInput output;
    
    input.position.w = 1.0f;
    
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    output.normal = normalize(mul(input.normal, (float3x3) worldMatrix));
    output.tex = input.tex;
    
    return output;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 PixelShaderEntry(PixelInput input) : SV_TARGET
{
    float4 baseColor = color;

    if (hasTexture)
    {
        baseColor *= ShaderTexture.Sample(SampleType, input.tex);
    }

    if (isLit)
    {
        float3 lightDir = -lightDirection;
        float lightIntensity = saturate(dot(input.normal, lightDir));
        
        float4 litColor = ambientColor + (diffuseColor * lightIntensity);
        
        baseColor *= saturate(litColor);
    }

    return baseColor;
}