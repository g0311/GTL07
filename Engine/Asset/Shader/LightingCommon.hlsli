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
    float Falloff;      // Inner Cone���� Outer Cone���� ������ ������
    float CosOuter;     // cos(OuterAngle)
    float CosInner;     // cos(InnerAngle) - (InnerAngle < OuterAngle)
    float _padding;
};

// Light Culling�ý��ۿ��� ����ϴ� ����Ʈ ����ü
// LightCulling.hlsl�� ������ ����ü ���
struct Light
{
    float4 position; // xyz: ���� ��ġ, w: ���� �ݰ�
    float4 color; // xyz: ����, w: ����
    float4 direction; // xyz: ���� (����Ʈ����Ʈ��), w: ���� Ÿ��
    float4 angles; // x: ���� ���� ����(cos), y: �ܺ� ���� ����(cos), z: falloff extent/falloff, w: InvRange2 (����Ʈ����)
};

// Viewport Info for Tiled Lighting
cbuffer TiledLightingParams : register(b3)
{
    uint2 ViewportOffset; // viewport offset
    uint2 ViewportSize; // viewport size
    uint NumLights; // total number of lights in the scene (for Gouraud)
    uint _padding; // Padding to align to 16 bytes
};

// Light Culling�ý��ۿ��� ����ϴ� Structured Buffer
StructuredBuffer<Light> AllLights : register(t13); // ����Ʈ ������ ����
StructuredBuffer<uint> LightIndexBuffer : register(t14); // Ÿ�Ϻ� ����Ʈ �ε���
StructuredBuffer<uint2> TileLightInfo : register(t15); // Ÿ�� ����Ʈ ���� (offset, count)

#endif // LIGHTING_COMMON_HLSLI