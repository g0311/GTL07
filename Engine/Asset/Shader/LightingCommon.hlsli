// LightingCommon.hlsli
// : Common structures for lighting information in shaders.

#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "ShaderDefines.hlsli"
#include "CommonBuffers.hlsli"
#include "MaterialCommon.hlsli"

struct FAmbientLightInfo
{
    float4 Color;
    float Intensity;
    float3 _padding;
};

struct FDirectionalLightInfo
{
    float3 Direction;
    float _padding;
    float3 Color;
    float Intensity;
};

struct FPointLightInfo
{
    float3 Position;
    float Radius;
    float3 Color;
    float Intensity;
    float FalloffExtent;
    float3 _padding;
};

struct FSpotLightInfo
{
    float4 Color;
    float Intensity;
    float3 Position;
    float3 Direction;
    float InvRange2;    // 1/(Range*Range) : Spotlight Range
    float Falloff;      // Inner Cone부터 Outer Cone까지 범위의 감쇠율
    float CosOuter;     // cos(OuterAngle)
    float CosInner;     // cos(InnerAngle) - (InnerAngle < OuterAngle)
    float _padding;
};

cbuffer Lighting : register(b3)
{
    int NumAmbientLights;
    float3 _pad0;
    FAmbientLightInfo AmbientLights[NUM_AMBIENT_LIGHT];
    
    int HasDirectionalLight;
    float3 _pad1;
    FDirectionalLightInfo DirectionalLight;
    
    int NumPointLights;
    float3 _pad2;
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    
    int NumSpotLights;
    float3 _pad3;
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT];
};

float3 CalculateSpecular(float3 LightDir, float3 WorldNormal, float3 ViewDir, float3 Ks, float3 LightColor, float Shininess)
{
#if LIGHTING_MODEL_LAMBERT
    return float3(0, 0, 0);
#elif LIGHTING_MODEL_PHONG
    float3 R = reflect(-LightDir, WorldNormal);
    float VdotR = saturate(dot(R, ViewDir));
    return Ks * LightColor * pow(VdotR, Shininess);
#elif LIGHTING_MODEL_BLINNPHONG
    float3 H = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(WorldNormal, H));
    return Ks * LightColor * pow(NdotH, Shininess);
#else
    return float3(0, 0, 0);
#endif
}

float4 CalculateAmbientLight(float2 UV)
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
    float4 AccumulatedAmbientColor = float4(0, 0, 0, 0);
    for (int i = 0; i < NumAmbientLights; i++)
    {
        // Light * Intensity
        AccumulatedAmbientColor += AmbientLights[i].Color * AmbientLights[i].Intensity;
    }
    return AmbientColor * AccumulatedAmbientColor;
}

float3 CalculateDirectionalLight(float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    if (HasDirectionalLight <= 0)
        return float3(0, 0, 0);

    float3 LightDir = normalize(-DirectionalLight.Direction);
    float3 LightColor = DirectionalLight.Color * DirectionalLight.Intensity;
    
    // Diffuse
    float NdotL = saturate(dot(WorldNormal, LightDir));
    float3 Diffuse = Kd * LightColor * NdotL;
    
    // Specular
    float3 Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, Ks, LightColor, Shininess);
    
    return Diffuse + Specular;
}

float3 CalculatePointLights(float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 AccumulatedPointlightColor;
    for (int i = 0; i < NumPointLights; i++)
    {
        float3 LightVec = PointLights[i].Position - WorldPos;
        float Distance = length(LightVec);

        if (Distance > PointLights[i].Radius)
            continue;
        
        float3 LightDir = LightVec / Distance;

        float RangeAttenuation = saturate(1.0 - Distance / PointLights[i].Radius);
        RangeAttenuation = pow(RangeAttenuation, max(PointLights[i].FalloffExtent, 0.0));
        
        // Light * Intensity
        float3 PointlightColor = PointLights[i].Color.rgb * PointLights[i].Intensity * RangeAttenuation;

        // Diffuse
        float NdotL = saturate(dot(normalize(WorldNormal), LightDir));
        float3 Diffuse = Kd * PointlightColor * NdotL; // Kd is Albedo
        
        // Specular
        float3 Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, Ks, PointlightColor, Shininess);
        AccumulatedPointlightColor += Diffuse + Specular;
    }
    return AccumulatedPointlightColor;
}

float3 CalculateSpotLights(float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 AccumulatedSpotlightColor = 0;
    for (int i = 0; i < NumSpotLights; i++)
    {
        float3 LightVec = SpotLights[i].Position - WorldPos;
        float Distance = length(LightVec);
        float3 LightDir = LightVec / Distance;

        // Attenuation : Range & Cone(Cos)
        float RangeAttenuation = saturate(1.0 - Distance * Distance * SpotLights[i].InvRange2);

        // SpotLight Cone Attenuation
        // SpotLightCos: angle between light direction and pixel direction
        float SpotLightCos = dot(-LightDir, normalize(SpotLights[i].Direction));

        // ConeWidth: CosInner > CosOuter (because InnerAngle < OuterAngle, cos is decreasing)
        float ConeWidth = max(SpotLights[i].CosInner - SpotLights[i].CosOuter, 1e-5);

        // SpotRatio: 0 when outside CosOuter, 1 when inside CosInner
        float SpotRatio = saturate((SpotLightCos - SpotLights[i].CosOuter) / ConeWidth);
        if (SpotRatio != 0)
        {
            SpotRatio = pow(SpotRatio, max(SpotLights[i].Falloff, 0.0)); // Falloff increase, light between outer~inner spread out 
        }
        
        // Light * Intensity
        float3 SpotlightColor = SpotLights[i].Color.rgb * SpotLights[i].Intensity * RangeAttenuation * SpotRatio;

        // Diffuse
        float NdotL = saturate(dot(WorldNormal, LightDir));
        float3 Diffuse = Kd * SpotlightColor * NdotL; // Kd is Albedo

        // Specular
        float3 Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, Ks, SpotlightColor, Shininess);
        AccumulatedSpotlightColor += Diffuse + Specular;
    }

    return AccumulatedSpotlightColor;
}

#endif // LIGHTING_COMMON_HLSLI