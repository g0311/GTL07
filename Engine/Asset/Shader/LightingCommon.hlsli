// LightingCommon.hlsli
// : Common structures for lighting information in shaders.

#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "ShaderDefines.hlsli"
#include "CommonBuffers.hlsli"

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

// Light Culling시스템에서 사용하는 라이트 구조체
// LightCulling.hlsl과 동일한 구조체 사용
struct Light
{
    float4 position; // xyz: 월드 위치, w: 영향 반경
    float4 color; // xyz: 색상, w: 강도
    float4 direction; // xyz: 방향 (스포트라이트용), w: 광원 타입
    float4 angles; // x: 내부 원뿔 각도(cos), y: 외부 원뿔 각도(cos), z: falloff extent/falloff, w: InvRange2 (스포트전용)
};

// Viewport Info for Tiled Lighting
cbuffer TiledLightingParams : register(b3)
{
    uint2 ViewportOffset; // viewport offset
    uint2 ViewportSize; // viewport size
    uint NumLights; // total number of lights in the scene (for Gouraud)
    uint _padding; // Padding to align to 16 bytes
};

// Light Culling시스템에서 사용하는 Structured Buffer
StructuredBuffer<Light> AllLights : register(t13); // 라이트 데이터 버퍼
StructuredBuffer<uint> LightIndexBuffer : register(t14); // 타일별 라이트 인덱스
StructuredBuffer<uint2> TileLightInfo : register(t15); // 타일 라이트 정보 (offset, count)

#endif // LIGHTING_COMMON_HLSLI