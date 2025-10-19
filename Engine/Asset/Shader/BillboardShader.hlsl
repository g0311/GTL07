// ============================================
// Billboard Shader.hlsl
// ============================================

cbuffer Model : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 WorldInverseTranspose;
}

cbuffer Camera : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 ViewWorldLocation;
    float NearClip;
    float FarClip;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

Texture2D DiffuseTexture : register(t0); // map_Kd
SamplerState SamplerWrap : register(s0);

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    
    Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
    Output.Tex = Input.Tex;
    
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float4 textureColor = DiffuseTexture.Sample(SamplerWrap, Input.Tex);

    // Alpha testing: Discard the pixel if its alpha is below a certain threshold (e.g., 0.1)
    // This prevents transparent parts from writing to the depth buffer.
    clip(textureColor.a - 0.1f);
    
    // +-+-+-+ WORKAROUND for Billboard Sorting Issue +-+-+-+
    //
    // [Problem]
    // Billboards, which are transparent, have sorting issues with editor helpers like the grid.
    // 1. If Depth Write is ON: The billboard's transparent background incorrectly occludes the grid behind it.
    // 2. If Depth Write is OFF: The grid, drawn after the billboard, incorrectly occludes the billboard's opaque icon.
    //
    // [Root Cause]
    // The engine's rendering pipeline renders all game passes (including FBillboardPass) BEFORE editor passes (like the grid).
    // This render order makes it impossible to correctly sort alpha-blended billboards with the grid.
    //
    // [This Workaround: Alpha Testing]
    // This clip() function fakes per-pixel depth write. It discards fully transparent pixels, so they don't write depth.
    // At the same time, we enable depth-writing for the whole pass (DefaultDepthStencilState). This allows the non-discarded,
    // opaque pixels to write their depth, ensuring they appear in front of the grid. This is a quick fix that avoids a major pipeline refactor.
    //
    // [Long-Term Solution: Pipeline Refactoring]
    // The fundamental solution is to re-order the rendering pipeline to explicitly control the layering.
    // The ideal pipeline order would be:
    //   1. Opaque Pass (e.g., Static Meshes) - Populates the depth buffer. (Depth Test: ON, Depth Write: ON)
    //   2. Editor Grid Pass - Renders the grid, correctly occluded by opaque objects. (Depth Test: ON, Depth Write: OFF)
    //   3. Transparent Pass (e.g., In-Game Billboards) - Renders game-world transparents using alpha-blending and depth-testing (read-only).
    //   4. Editor Overlay Pass (e.g., Visualization Billboards) - Renders editor icons after the grid, also using alpha-blending and depth-testing (read-only).
    // This requires creating a separate EditorBillboardPass and changing the main render loop in URenderer::Update().

    return textureColor;
}