////////////////////////////////////////////////////////////////////////////////
// Filename: texture.hlsl
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

// t0, s0 are GPU binding slots. They connect shader variables to actual GPU resources.
// The engine must bind them before drawing. Shader and CPU don’t talk by name, only by slots (indices).
// a texture to slot t0
// a sampler to slot s0

Texture2D ShaderTexture : register(t0);
SamplerState SampleType : register(s0);

/////////////
// TYPEDEFS //
/////////////
struct VertexInputType
{
    float4 position : POSITION;
    float2 tex : TEXCOORD0;
};

struct PixelInputType
{
    float4 position : SV_POSITION; // SV = System Value
    float2 tex : TEXCOORD0;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType TextureVertexShader(VertexInputType input)
{
    PixelInputType output;
    
    // Change the position vector to be 4 units for proper matrix calculations.
    // float4 = {x, y, z, w} : w is the pos vector
    input.position.w = 1.0f;

    // Calculate the position of the vertex against the world, view, and projection matrices.
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    // Store the input uv (texture coordinates) for the pixel shader to use.
    output.tex = input.tex;
    
    return output;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 TexturePixelShader(PixelInputType input) : SV_TARGET
{
    float4 textureColor;
    
    // Sample the pixel color from the texture using the sampler at this texture coordinate location.
    textureColor = ShaderTexture.Sample(SampleType, input.tex);
    
    return textureColor;
}