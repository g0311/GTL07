// ================================================================
// WorldNormalShader.hlsl
// - Renders world-space normals for the editor.
// - Input: Normal Texture (encoded in [0,1] range)
// - Output: RGB visualization of world normals
// ================================================================

// ------------------------------------------------
// Constant Buffers
// ------------------------------------------------
cbuffer PerFrameConstants : register(b1)
{
    float2 RenderTargetSize;
    float2 Padding;
};

// ------------------------------------------------
// Textures and Sampler
// ------------------------------------------------
Texture2D NormalTexture : register(t0);
SamplerState PointSampler : register(s0);

// ------------------------------------------------
// Vertex and Pixel Shader I/O
// ------------------------------------------------
struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

// ================================================================
// Vertex Shader
// - Generates a full-screen triangle without needing a vertex buffer.
// ================================================================
PS_INPUT mainVS(uint vertexID : SV_VertexID)
{
    PS_INPUT output;

    // SV_VertexID를 사용하여 화면을 덮는 큰 삼각형의 클립 공간 좌표를 생성
    // ID 0 -> (-1, 1), ID 1 -> (3, 1), ID 2 -> (-1, -3)
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    output.Position = float4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
    output.Position.y *= -1.0f;

    return output;
}

// ================================================================
// Pixel Shader
// - Samples encoded normals and visualizes them in RGB.
// - Decodes from [0,1] range to [-1,1] range.
// ================================================================
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float2 ScreenPosition = Input.Position.xy;
    float2 uv = ScreenPosition / RenderTargetSize;

    // Sample the encoded normal from the texture
    float4 encodedNormal = NormalTexture.Sample(PointSampler, uv);

    // Check if this pixel has valid geometry (alpha != 0)
    // If alpha is 0, it means no geometry was rendered here (background)
    if (encodedNormal.a < 0.01f)
    {
        // Return background color (black)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Decode from [0,1] to [-1,1]
    float3 worldNormal = encodedNormal.rgb * 2.0f - 1.0f;

    // Normalize to ensure unit length
    worldNormal = normalize(worldNormal);

    // Re-encode to [0,1] for visualization
    // In Unreal Engine style:
    // R channel = X axis (Red: +X, Cyan: -X)
    // G channel = Y axis (Green: +Y, Magenta: -Y)
    // B channel = Z axis (Blue: +Z, Yellow: -Z)
    float3 visualColor = worldNormal * 0.5f + 0.5f;

    return float4(visualColor, 1.0f);
}
