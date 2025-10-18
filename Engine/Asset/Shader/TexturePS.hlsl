#include "TextureVS.hlsl"
#include "Editor/Public/SplitterWindow.h"
#include "Global/Enum.h"
#include "Physics/Public/BoundingVolume.h"

struct FAmbientLightInfo
{
    float4 AmbientColor;
    float Intensity;
};

struct FSpotLightInfo
{
    float4 Color;
    float Intensity;

    float3 Position;
    float InvRange2;// 1/(Range*Range) : Spotlight Range
    
    float3 Direction;
    float CosOuter;   // Spotlight의 바깥 Cone

    float CosInner; // Spotlight의 안쪽 Cone (OuterAngle > InnerAngle)
    float Falloff;  // Inner Cone부터 Outer Cone까지 범위의 감쇠율
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
    
    int SpotlightCount;
    FSpotLightInfo Spotlight[8];
};

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

float4 CalculateAmbientFactor(float2 UV)
{
    float4 AmbientColor;
    // Material Ambient(Albedo)
    /*TODO : Apply DiffuseTexture for now, due to Binding AmbientTexture feature don't exist yet*/
    // if (MaterialFlags & HAS_AMBIENT_MAP)
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        // AmbientColor = AmbientTexture.Sample(SamplerWrap, UV);
        AmbientColor = DiffuseTexture.Sample(SamplerWrap, UV);
    }
    else
    {
        AmbientColor = Ka;
    }
    // Light Ambient
    float4 AccumulatedAmbientColor = 0;
    for (int i = 0; i < AmbientCount; i++)
    {
        // Light * Intensity
        AccumulatedAmbientColor+= Ambient[i].AmbientColor*Ambient[i].Intensity;
    }
    AmbientColor *= AccumulatedAmbientColor;
    return AmbientColor;
}
float3 CalculateSpotlightFactors(float3 Position, float3 Norm, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 AccumulatedSpotlightColor = 0;
    for (int i = 0; i < SpotlightCount; i++)
    {
        float3 LightDir = normalize(Spotlight[i].Position - Position);

        // Attenuation : Range & Cone(Cos)
        float RangeAttenuation = saturate(1.0 - length(LightDir) * Spotlight[i].InvRange2);
        
        float SpotLightCos = dot(LightDir, normalize(Spotlight[i].Direction));
        float ConeWidth = max(Spotlight[i].CosInner - Spotlight[i].CosOuter, 1e-5);
        float SpotRatio = saturate((SpotLightCos - Spotlight[i].CosOuter) / ConeWidth);
        SpotRatio = pow(SpotRatio, max(Spotlight[i].Falloff, 0.0));

        /* TODO : Blinn Phong for Now */
        // Light * Intensity
        float3 SpotlightColor = Spotlight[i].Color.rgb * Spotlight[i].Intensity * RangeAttenuation * SpotRatio;

        // Diffuse
        float NdotL = saturate(dot(Norm, LightDir));
        float3 Diffuse = Kd * SpotlightColor * NdotL;
        
        // Specular
        float3 H = normalize(LightDir + ViewDir);
        float  NdotH = saturate(dot(Norm, H));
        float3 Specular = Ks * SpotlightColor * pow(NdotH, Shininess);

        AccumulatedSpotlightColor += Diffuse * Specular;
    }

    return AccumulatedSpotlightColor;
}

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;
    
    float2 UV = Input.Tex;
    float3 Norm  = normalize(Input.WorldNormal);
    float3 ViewDir  = normalize(ViewWorldLocation - Input.WorldPosition);

    float3 kD = (MaterialFlags & HAS_DIFFUSE_MAP) ? DiffuseTexture.Sample(SamplerWrap, UV).rgb : Kd;
    float3 kS = (MaterialFlags & HAS_SPECULAR_MAP) ? SpecularTexture.Sample(SamplerWrap, UV).rgb : Ks;
    
    // Ambient contribution
    float3 AmbientColor = CalculateAmbientFactor(UV).rgb;
    //
    // // Spotlight contribution
    // float3 SpotlightColor = CalculateSpotlightFactors(Input.WorldPosition, Norm, ViewDir, kD, kS, Ns);
    //
    // float4 FinalColor = float4(0.f, 0.f, 0.f, 1.f);
    // FinalColor.rgb = AmbientColor + SpotlightColor;
    //
    // // Alpha handling
    // /*TODO : Apply DiffuseTexture for now, due to Binding AlphaTexture feature don't exist yet*/
    // if (MaterialFlags & HAS_DIFFUSE_MAP)
    // {
    //     float alpha = DiffuseTexture.Sample(SamplerWrap, UV).w;
    //     FinalColor.a = D * alpha;
    // }

    // Output.SceneColor = FinalColor;
    Output.SceneColor.rgb = AmbientColor;
    Output.SceneColor.a = 1.0;
    float3 EncodedNormal = normalize(Norm) * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);
	
    return Output;
}
