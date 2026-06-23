////////////////////////////////////////////////////////////////////////////////
// Filename: ui.hlsl
////////////////////////////////////////////////////////////////////////////////

/////////////
// GLOBALS //
/////////////
cbuffer MatrixBuffer : register(b0)
{
    float4x4 projection;
};

Texture2D uiTexture : register(t0);
SamplerState uiSampler : register(s0);

/////////////
// TYPEDEFS //
/////////////
struct VertexInputType
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct PixelInputType
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType VertexShaderEntry(VertexInputType input)
{
    PixelInputType output;
    output.pos = mul(projection, float4(input.pos, 0, 1));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 PixelShaderEntry(PixelInputType input) : SV_TARGET
{
    float4 tex = uiTexture.Sample(uiSampler, input.uv);
    return tex * input.color;
}