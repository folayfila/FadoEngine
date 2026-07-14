////////////////////////////////////////////////////////////////////////////////
// Filename: particle.hlsl
////////////////////////////////////////////////////////////////////////////////

/////////////
// GLOBALS //
/////////////

cbuffer MatrixBuffer : register(b0)
{
    matrix viewMatrix;
    matrix projectionMatrix;
};

/////////////
// TYPEDEFS //
/////////////

struct VertexInput
{
    float3 position : POSITION; // quad corner, e.g. -0.5..0.5
    float2 tex : TEXCOORD;
    float3 instPos : INST_POS;
    float instSize : INST_SIZE;
    float4 instColor : INST_COLOR;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 color : COLOR0;
};

Texture2D ParticleTexture : register(t0);
SamplerState SampleType : register(s0);

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInput VertexShaderEntry(VertexInput input)
{
    PixelInput output;

    // Camera-facing billboard: extract right/up from the view matrix's rows,
    // scale the quad corner by instSize, offset by instPos.
    float3 camRight = float3(viewMatrix._11, viewMatrix._21, viewMatrix._31);
    float3 camUp = float3(viewMatrix._12, viewMatrix._22, viewMatrix._32);

    float3 worldPos = input.instPos
        + camRight * input.position.x * input.instSize
        + camUp * input.position.y * input.instSize;

    float4 viewPos = mul(float4(worldPos, 1.0f), viewMatrix);
    output.position = mul(viewPos, projectionMatrix);

    output.tex = input.tex;
    output.color = input.instColor;
    return output;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 PixelShaderEntry(PixelInput input) : SV_TARGET
{
    float4 texColor = ParticleTexture.Sample(SampleType, input.tex);
    texColor *= input.color;
    return texColor;
}