// LightingCommon.hlsli
// : Common structures for lighting information in shaders.

#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "ShaderDefines.hlsli"

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

#endif // LIGHTING_COMMON_HLSLI