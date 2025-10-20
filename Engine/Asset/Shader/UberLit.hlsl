// =============================================================================
// UberLit.hlsl
// A single "uber shader" supporting three classic lighting models and an Unlit view.

// View Modes:
//   VMI_Lit_Gouraud  : Per-Vertex lighting with Gouraud shading
//   VMI_Lit_Lambert  : Per-Pixel lighting with Lambertian reflectance
//   VMI_Lit_Phong    : Per-Pixel lighting with Blinn-Phong reflectance
//   VMI_Unlit        : No lighting calculations (outputs raw texture/color)

// Normal mapping supported via HAS_NORMAL_MAP (TBN)
// =============================================================================
#include "ShaderDefines.hlsli"
#include "LightingCommon.hlsli"
#include "CommonBuffers.hlsli"
#include "MaterialCommon.hlsli"

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
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 Tex : TEXCOORD2;
    float3 WorldTangent : TEXCOORD3;
    float3 WorldBitangent : TEXCOORD4;
};

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

float3 CalculateWorldNormal(PS_INPUT Input, float2 UV)
{
    float3 WorldNormal = Input.WorldNormal;
    
    // Safety check: if normal is zero, use a default up vector
    if (length(WorldNormal) < 0.001f)
    {
        WorldNormal = float3(0.0f, 0.0f, 1.0f);
    }
    else
    {
        WorldNormal = normalize(WorldNormal);
    }
    
    // Sample normal map (tangent space)
    if (MaterialFlags & HAS_NORMAL_MAP)
    {
        float3 TangentNormal = NormalTexture.Sample(SamplerWrap, UV).rgb;
        // Decode from [0,1] to [-1,1]
        TangentNormal = TangentNormal * 2.0f - 1.0f;

        // Construct TBN matrix (Tangent, Bitangent, Normal)
        float3 N = WorldNormal;
        float3 T = Input.WorldTangent;
        float3 B = Input.WorldBitangent;

        // Safety check for tangent and bitangent
        if (length(T) > 0.001f && length(B) > 0.001f)
        {
            T = normalize(T);
            B = normalize(B);
            // Transform tangent space normal to world space
            WorldNormal = normalize(TangentNormal.x * T + TangentNormal.y * B + TangentNormal.z * N);
        }
    }
    
    return WorldNormal;
}

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
    Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);

    // Transform normal to world space using inverse transpose for non-uniform scale
    // Do NOT normalize here - let GPU interpolate, then normalize in PS
    Output.WorldNormal = mul(Input.Normal, (float3x3) WorldInverseTranspose);

    // Transform tangent to world space using inverse transpose
    Output.WorldTangent = mul(Input.Tangent, (float3x3) WorldInverseTranspose);

    // Transform bitangent to world space using inverse transpose
    Output.WorldBitangent = mul(Input.Bitangent, (float3x3) WorldInverseTranspose);

    Output.Tex = Input.Tex;

#if LIGHTING_MODEL_GOURAUD
          
#endif
    return Output;
}

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;
    float2 UV = Input.Tex;
    float3 Normal = normalize(Input.WorldNormal);
    float3 ViewDir = normalize(ViewWorldLocation - Input.WorldPosition);

    float3 kD = (MaterialFlags & HAS_DIFFUSE_MAP) ? DiffuseTexture.Sample(SamplerWrap, UV).rgb : Kd;
    float3 kS = (MaterialFlags & HAS_SPECULAR_MAP) ? SpecularTexture.Sample(SamplerWrap, UV).rgb : Ks;

    // Unlit Ã³¸®
    if (MaterialFlags & UNLIT)
    {
        Output.SceneColor = float4(kD, 1.0);
        Output.NormalData = float4(Normal * 0.5f + 0.5f, 1.0);
        return Output;
    }

    // Light
    float4 Albedo;
    // Material Ambient(Albedo)
    /*TODO : Apply DiffuseTexture for now, due to Binding AmbientTexture feature don't exist yet*/
    // if (MaterialFlags & HAS_AMBIENT_MAP)
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        // AmbientColor = AmbientTexture.Sample(SamplerWrap, UV);
        Albedo = DiffuseTexture.Sample(SamplerWrap, UV);
    }
    else
    {
        Albedo = Ka;
    }
    float3 AmbientColor = CalculateAmbientLight(Albedo).rgb;
    float3 DirectionalColor = CalculateDirectionalLight(Normal, ViewDir, kD, kS, Ns);
    float3 PointLightColor = CalculatePointLights(Input.WorldPosition, Normal, ViewDir, kD, kS, Ns);
    float3 SpotLightColor = CalculateSpotLights(Input.WorldPosition, Normal, ViewDir, kD, kS, Ns);
    
    float4 FinalColor;
    FinalColor.rgb = AmbientColor + DirectionalColor + PointLightColor + SpotLightColor;
    FinalColor.a = 1.0f;

    // Alpha handling
    /*TODO : Apply DiffuseTexture for now, due to Binding AlphaTexture feature don't exist yet*/
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        float alpha = DiffuseTexture.Sample(SamplerWrap, UV).w;
        FinalColor.a = D * alpha;
    }
    Output.SceneColor = FinalColor;

    // Normal
    // Calculate World Normal for Normal Buffer
    float3 WorldNormal = CalculateWorldNormal(Input, UV);
    
    // Encode world normal to [0,1] range for storage
    float3 EncodedNormal = WorldNormal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
};