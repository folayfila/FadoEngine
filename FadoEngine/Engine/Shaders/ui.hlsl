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

cbuffer SquiggleBuffer : register(b1)
{
    float time;
    float2 noiseScale;
    float squiggleStrength;
    float squiggleFPS;
    float3 pad; // keep 16-byte alignment, adjust as needed
};

Texture2D uiTexture : register(t0);
SamplerState uiSampler : register(s0);

Texture2D NoiseTexture : register(t1);
SamplerState WrapSampler : register(s1);

static const float PI = 3.14159265f;
static const float2 OFFSET_MULT = float2(3.14159265f, 2.71828183f); // pi, e

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
    float2 screenPos : TEXCOORD1; // new
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
    output.screenPos = input.pos; // pre-projection screen-space pos
    return output;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 PixelShaderEntry(PixelInputType input) : SV_TARGET
{
    float2 noiseUV = input.screenPos / noiseScale;
    float2 noiseOffset = floor(time * squiggleFPS) * OFFSET_MULT;
    float noiseSample = NoiseTexture.Sample(WrapSampler, noiseUV + noiseOffset).r * 4.0f * PI;

    float2 direction = float2(cos(noiseSample), sin(noiseSample));
    float2 squiggleUV = input.uv + direction * squiggleStrength * 0.005f;

    float4 tex = uiTexture.Sample(uiSampler, squiggleUV);
    return tex * input.color;
}