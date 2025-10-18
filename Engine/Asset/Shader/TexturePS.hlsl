#include "TextureVS.hlsl"

struct FAmbientLightInfo
{
    float4 AmbientColor;
    float Intensity;
};

struct FDirectionalLightInfo
{
    float3 Direction;
    float _Padding;
    float3 Color;
    float Intensity;
};

cbuffer MaterialConstants : register(b2) // b0, b1 is in VS
{
    float4 Ka;		// Ambient color
    float4 Kd;		// Diffuse color
    float4 Ks;		// Specular color
    float Ns;		// Specular exponent
    float Ni;		// Index of refraction
    float D;		// Dissolve factor
    uint MaterialFlags;	// Which textures are available (bitfield)
    float Time;
};

cbuffer Lighting : register(b3)
{
    int AmbientCount;
    FAmbientLightInfo Ambient[8];
};

cbuffer DirectionalLighting : register(b4)
{
    FDirectionalLightInfo Directional;
    int HasDirectionalLight;
}

Texture2D DiffuseTexture : register(t0);	// map_Kd
Texture2D AmbientTexture : register(t1);	// map_Ka
Texture2D SpecularTexture : register(t2);	// map_Ks
Texture2D NormalTexture : register(t3);		// map_Ns
Texture2D AlphaTexture : register(t4);		// map_d
Texture2D BumpTexture : register(t5);		// map_bump

SamplerState SamplerWrap : register(s0);

// Material flags
#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_AMBIENT_MAP	 (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_NORMAL_MAP	 (1 << 3)
#define HAS_ALPHA_MAP	 (1 << 4)
#define HAS_BUMP_MAP	 (1 << 5)

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;
    float2 UV = Input.Tex;
    
    // Base diffuse color
    float4 DiffuseColor = Kd;
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        DiffuseColor *= DiffuseTexture.Sample(SamplerWrap, UV);
    }

    // Ambient contribution
    float4 AmbientColor = Ka;
    if (MaterialFlags & HAS_AMBIENT_MAP)
    {
        AmbientColor *= AmbientTexture.Sample(SamplerWrap, UV);
    }
    // Add Ambient Light from cbuffer
    float4 AccumulatedAmbientColor = 0;
    for (int i = 0; i < AmbientCount; i++)
    {
        AccumulatedAmbientColor += Ambient[i].AmbientColor * Ambient[i].Intensity;
    }
    AmbientColor *= AccumulatedAmbientColor;

    // Directional Light
    float3 DirectLighting = float3(0.f, 0.f, 0.f);
    if (HasDirectionalLight > 0)
    {
        float3 normal = normalize(Input.WorldNormal);
        float3 lightDir = normalize(-Directional.Direction);
        float NdotL = saturate(dot(normal, lightDir));
        DirectLighting += Directional.Color * Directional.Intensity * NdotL;
    }
    
    // Final Color 
    float4 FinalColor;
    FinalColor.rgb = DiffuseColor.rgb * (DirectLighting + AmbientColor.rgb);
    FinalColor.a = DiffuseColor.a;
    
    // Alpha handling
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
        FinalColor.a = D;
        FinalColor.a *= alpha;
    }

    Output.SceneColor = FinalColor;

    // Calculate World Normal for Normal Buffer
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

    if (MaterialFlags & HAS_NORMAL_MAP)
    {
        // Sample normal map (tangent space)
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

    // Encode world normal to [0,1] range for storage
    float3 EncodedNormal = WorldNormal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
};
