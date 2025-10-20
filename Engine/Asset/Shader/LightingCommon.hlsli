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

#define TILE_SIZE 32

// 스크린 픽셀 좌표에서 타일 인덱스 계산 (뷰포트 기준)
uint2 GetTileIndexFromPixelViewport(float2 screenPos, uint2 viewportOffset)
{
    float2 viewportRelativePos = screenPos - viewportOffset;
    return uint2(floor(viewportRelativePos.x / TILE_SIZE), floor(viewportRelativePos.y / TILE_SIZE));
}

// SV_Position에서 뷰포트 기준 타일 배열 인덱스 추출
uint GetCurrentTileArrayIndex(float4 svPosition, uint2 viewportOffset, uint2 viewportSize)
{
    uint2 viewportTileIndex = GetTileIndexFromPixelViewport(svPosition, viewportOffset);
    uint tilesPerRow = (viewportSize.x + TILE_SIZE - 1) / TILE_SIZE;
    return viewportTileIndex.y * tilesPerRow + viewportTileIndex.x;
}

// Light Culling시스템에서 사용하는 라이트 구조체
// LightCulling.hlsl과 동일한 구조체 사용
struct Light
{
    float4 position; // xyz: 월드 위치, w: 영향 반경
    float4 color; // xyz: 색상, w: 강도
    float4 direction; // xyz: 방향 (스포트라이트용), w: 광원 타입
    float4 angles; // x: 내부 원뿔 각도(cos), y: 외부 원뿔 각도(cos), z: falloff extent/falloff, w: InvRange2 (스포트전용)
};

// 광원 타입 정의 (C++ ELightType enum과 동일한 순서)
#define LIGHT_TYPE_AMBIENT 0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_POINT 2
#define LIGHT_TYPE_SPOT 3

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

FAmbientLightInfo ConvertToAmbientLight(Light light)
{
    FAmbientLightInfo ambientLight;
    ambientLight.Color = float4(light.color.rgb, 1.0); // Only RGB, no intensity in color
    ambientLight.Intensity = light.color.w; // Intensity stored separately
    return ambientLight;
}

FDirectionalLightInfo ConvertToDirectionalLight(Light light)
{
    FDirectionalLightInfo directionalLight;
    directionalLight.Direction = light.direction.xyz;
    directionalLight.Color = light.color.rgb;
    directionalLight.Intensity = light.color.w;
    return directionalLight;
}

FPointLightInfo ConvertToPointLight(Light light)
{
    FPointLightInfo pointLight;
    pointLight.Position = light.position.xyz;
    pointLight.Radius = light.position.w;
    pointLight.Color = light.color.rgb;
    pointLight.Intensity = light.color.w;
    pointLight.FalloffExtent = light.angles.z;
    return pointLight;
}

FSpotLightInfo ConvertToSpotLight(Light light)
{
    FSpotLightInfo spotLight;
    spotLight.Position = light.position.xyz;
    spotLight.Direction = light.direction.xyz;
    spotLight.Color = light.color;
    spotLight.Intensity = light.color.w;
    spotLight.CosInner = light.angles.x;
    spotLight.CosOuter = light.angles.y;
    spotLight.Falloff = light.angles.z;
    spotLight.InvRange2 = light.angles.w;
    return spotLight;
}

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
    float3 H = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(WorldNormal, H));
    return Ks * LightColor * pow(NdotH, Shininess);
#endif
}

float3 CalculateSingleAmbientLight(FAmbientLightInfo ambientLight, float3 Kd)
{
    return Kd * ambientLight.Color.rgb * ambientLight.Intensity;
}

float3 CalculateSingleDirectionalLight(FDirectionalLightInfo directionalLight, float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 LightDir = normalize(-directionalLight.Direction);
    float3 LightColor = directionalLight.Color * directionalLight.Intensity;
    
    // Diffuse
    float NdotL = saturate(dot(WorldNormal, LightDir));
    float3 Diffuse = Kd * LightColor * NdotL;
    
    // Specular
    float3 Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, Ks, LightColor, Shininess);
    
    return Diffuse + Specular;
}

float3 CalculateSinglePointLight(FPointLightInfo pointLight, float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 LightVec = pointLight.Position - WorldPos;
    float Distance = length(LightVec);

    if (Distance > pointLight.Radius)
        return float3(0, 0, 0);
        
    float3 LightDir = LightVec / Distance;

    float RangeAttenuation = saturate(1.0 - Distance / pointLight.Radius);
    RangeAttenuation = pow(RangeAttenuation, max(pointLight.FalloffExtent, 0.0));
    
    // Light * Intensity
    float3 PointlightColor = pointLight.Color.rgb * pointLight.Intensity * RangeAttenuation;

    // Diffuse
    float NdotL = saturate(dot(normalize(WorldNormal), LightDir));
    float3 Diffuse = Kd * PointlightColor * NdotL;
    
    // Specular
    float3 Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, Ks, PointlightColor, Shininess);
    
    return Diffuse + Specular;
}

float3 CalculateSingleSpotLight(FSpotLightInfo spotLight, float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 LightVec = spotLight.Position - WorldPos;
    float Distance = length(LightVec);
    float3 LightDir = LightVec / Distance;

    // Attenuation : Range & Cone(Cos)
    float RangeAttenuation = saturate(1.0 - Distance * Distance * spotLight.InvRange2);

    // SpotLight Cone Attenuation
    float SpotLightCos = dot(-LightDir, normalize(spotLight.Direction));
    float ConeWidth = max(spotLight.CosInner - spotLight.CosOuter, 1e-5);
    float SpotRatio = saturate((SpotLightCos - spotLight.CosOuter) / ConeWidth);
    
    if (SpotRatio != 0)
    {
        SpotRatio = pow(SpotRatio, max(spotLight.Falloff, 0.0));
    }
    
    // Light * Intensity
    float3 SpotlightColor = spotLight.Color.rgb * spotLight.Intensity * RangeAttenuation * SpotRatio;

    // Diffuse
    float NdotL = saturate(dot(WorldNormal, LightDir));
    float3 Diffuse = Kd * SpotlightColor * NdotL;

    // Specular
    float3 Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, Ks, SpotlightColor, Shininess);
    
    return Diffuse + Specular;
}

float3 CalculateTiledLighting(float4 svPosition, float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess, uint2 viewportOffset, uint2 viewportSize)
{
    float3 AccumulatedColor = float3(0, 0, 0);

    // 현재 픽셀이 속한 타일의 인덱스를 계산
    uint tileArrayIndex = GetCurrentTileArrayIndex(svPosition, viewportOffset, viewportSize);

    // 타일의 라이트 정보 가져오기
    uint2 tileLightInfo = TileLightInfo[tileArrayIndex];
    uint lightIndexOffset = tileLightInfo.x;
    uint lightCount = tileLightInfo.y;

    // 타일에 속한 모든 라이트를 순회하면서 계산
    for (uint i = 0; i < lightCount; ++i)
    {
        uint lightIndex = LightIndexBuffer[lightIndexOffset + i];
        Light light = AllLights[lightIndex];

        uint lightType = (uint) light.direction.w;

        if (lightType == LIGHT_TYPE_AMBIENT)
        {
            FAmbientLightInfo ambientLight = ConvertToAmbientLight(light);
            AccumulatedColor += CalculateSingleAmbientLight(ambientLight, Kd);
        }
        else if (lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            FDirectionalLightInfo directionalLight = ConvertToDirectionalLight(light);
            AccumulatedColor += CalculateSingleDirectionalLight(directionalLight, WorldNormal, ViewDir, Kd, Ks, Shininess);
        }
        else if (lightType == LIGHT_TYPE_POINT)
        {
            FPointLightInfo pointLight = ConvertToPointLight(light);
            AccumulatedColor += CalculateSinglePointLight(pointLight, WorldPos, WorldNormal, ViewDir, Kd, Ks, Shininess);
        }
        else if (lightType == LIGHT_TYPE_SPOT)
        {
            FSpotLightInfo spotLight = ConvertToSpotLight(light);
            AccumulatedColor += CalculateSingleSpotLight(spotLight, WorldPos, WorldNormal, ViewDir, Kd, Ks, Shininess);
        }
    }

    return AccumulatedColor;
}

#endif // LIGHTING_COMMON_HLSLI