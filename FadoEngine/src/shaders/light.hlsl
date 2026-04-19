////////////////////////////////////////////////////////////////////////////////
// Filename: light.hlsl
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

cbuffer LightBuffer
{
    float4 diffuseColor;
    float3 lightDirection;
    float padding;
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
    float3 normal : NORMAL;
};

struct PixelInputType
{
    float4 position : SV_POSITION; // SV = System Value
    float2 tex : TEXCOORD0;
    float3 normal : NORMAL;
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
    
    // Store the input uv (texture coordinates) for the pixel shader to use.
    output.tex = input.tex;
    
    // Calculate the normal vector against the world matrix only.
    output.normal = mul(input.normal, (float3x3) worldMatrix);
    
    // NOTE: Sometimes these normals need to be re-normalized inside the pixel shader due to the interpolation that occurs.
    // Normalize the normal vector.
    output.normal = normalize(output.normal);
    
    return output;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 PixelShaderEntry(PixelInputType input) : SV_TARGET
{
    float4 textureColor;
    float3 lightDir;
    float lightIntensity;
    float4 color;
    
    // Sample the pixel color from the texture using the sampler at this texture coordinate location.
    textureColor = ShaderTexture.Sample(SampleType, input.tex);
    
    // Invert the light direction for calculations.
    lightDir = -lightDirection;
    
    // Calculate the amount of light on this pixel.
    /*
      1 -> facing light (bright)
      0 -> perpendicular (dark)
    < 0 -> facing away
    */
    lightIntensity = saturate(dot(input.normal, lightDir));     // saturate = clamp(0-1)
    
    // Determine the final amount of diffuse color based on the diffuse color combined with the light intensity.
    color = saturate(diffuseColor * lightIntensity);
    
    // Multiply the texture pixel and the final diffuse color to get the final pixel color result.
    color *= textureColor;
    
    return color;
}