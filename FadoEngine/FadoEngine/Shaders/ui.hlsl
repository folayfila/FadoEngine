////////////////////////////////////////////////////////////////////////////////
// Filename: ui.hlsl
////////////////////////////////////////////////////////////////////////////////

/////////////
// GLOBALS //
/////////////
cbuffer UI_Constants : register(b0)
{
    float4x4 projection;
};

Texture2D ui_texture : register(t0);
SamplerState ui_sampler : register(s0);

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
    float4 tex = ui_texture.Sample(ui_sampler, input.uv);
    return tex * input.color;
}