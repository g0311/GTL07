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
    float Falloff;      // Inner Cone ~ Outer Cone 사이 보간, 높을수록 급격히 떨어짐
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

// ��ũ�� �ȼ� ��ǥ���� Ÿ�� �ε��� ��� (����Ʈ ����)
uint2 GetTileIndexFromPixelViewport(float2 screenPos, uint2 viewportOffset)
{
    float2 viewportRelativePos = screenPos - viewportOffset;
    return uint2(floor(viewportRelativePos.x / TILE_SIZE), floor(viewportRelativePos.y / TILE_SIZE));
}

// SV_Position���� ����Ʈ ���� Ÿ�� �迭 �ε��� ����
uint GetCurrentTileArrayIndex(float4 svPosition, uint2 viewportOffset, uint2 viewportSize)
{
    uint2 viewportTileIndex = GetTileIndexFromPixelViewport(svPosition, viewportOffset);
    uint tilesPerRow = (viewportSize.x + TILE_SIZE - 1) / TILE_SIZE;
    return viewportTileIndex.y * tilesPerRow + viewportTileIndex.x;
}

// Viewport Info for Tiled Lighting
cbuffer TiledLightingParams : register(b3)
{
    uint2 ViewportOffset; // viewport offset
    uint2 ViewportSize; // viewport size
    uint NumLights; // total number of lights in the scene (for Gouraud)
    uint EnableCulling; // Light Culling 활성화 여부 (1=활성화, 0=모든라이트렌더)
};

// Light Culling용 Structured Buffer
StructuredBuffer<Light> AllLights : register(t13);
StructuredBuffer<uint> LightIndexBuffer : register(t14);
StructuredBuffer<uint2> TileLightInfo : register(t15); // (offset, count)

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

float3 CalculateSpecular(float3 LightDir, float3 WorldNormal, float3 ViewDir, float3 InKs, float3 LightColor, float Shininess)
{
#if LIGHTING_MODEL_LAMBERT
    return float3(0, 0, 0);
#elif LIGHTING_MODEL_PHONG
    float3 R = reflect(-LightDir, WorldNormal);
    float VdotR = saturate(dot(R, ViewDir));
    const float EPS = 1e-4;
    if (VdotR <= EPS )
        return 0;
    return InKs * LightColor * pow(VdotR, Shininess);
#elif LIGHTING_MODEL_BLINNPHONG
    float3 H = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(WorldNormal, H));
    return InKs * LightColor * pow(NdotH, Shininess);
#else
    float3 H = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(WorldNormal, H));
    return InKs * LightColor * pow(NdotH, Shininess);
#endif
}

// Ambient Light: Use ambient texture if available, otherwise use diffuse
float3 CalculateSingleAmbientLight(FAmbientLightInfo ambientLight, float3 InKa, float3 InKd)
{
    // If Ka is zero (0,0,0), fallback to Kd
    float3 AmbientMaterial = all(InKa == 0.0) ? InKd : InKa;
    return AmbientMaterial * ambientLight.Color.rgb * ambientLight.Intensity;
}

float3 CalculateSingleDirectionalLight(FDirectionalLightInfo directionalLight, float3 WorldNormal, float3 ViewDir, float3 InKd, float3 InKs, float Shininess)
{
    float3 LightDir = normalize(-directionalLight.Direction);
    float3 LightColor = directionalLight.Color * directionalLight.Intensity;
    
    float NdotL = saturate(dot(WorldNormal, LightDir));
    float3 Diffuse = InKd * LightColor * NdotL;

    float3 Specular = float3(0, 0, 0);
    if (NdotL > 0.0)
    {
        Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, InKs, LightColor, Shininess);
    }

    return Diffuse + Specular;
}

float3 CalculateSinglePointLight(FPointLightInfo pointLight, float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 InKd, float3 InKs, float Shininess)
{
    float3 LightVec = pointLight.Position - WorldPos;
    float Distance = length(LightVec);

    if (Distance > pointLight.Radius)
        return float3(0, 0, 0);

    float3 LightDir = LightVec / Distance;

    float RangeAttenuation = saturate(1.0 - ((Distance * Distance) / (pointLight.Radius * pointLight.Radius)));
    RangeAttenuation = pow(RangeAttenuation, max(pointLight.FalloffExtent, 0.0));

    // Light * Intensity
    float3 PointlightColor = pointLight.Color.rgb * pointLight.Intensity * RangeAttenuation;

    // Diffuse
    float NdotL = saturate(dot(normalize(WorldNormal), LightDir));
    float3 Diffuse = InKd * PointlightColor * NdotL;
    
    float3 Specular = float3(0, 0, 0);
    if (NdotL > 0.0)
    {
        Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, InKs, PointlightColor, Shininess);
    }

    return Diffuse + Specular;
}

float3 CalculateSingleSpotLight(FSpotLightInfo spotLight, float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 InKd, float3 InKs, float Shininess)
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
    float3 Diffuse = InKd * SpotlightColor * NdotL;

    // Specular (only if surface faces the light)
    // If Ks is zero (0,0,0), fallback to white (1,1,1)
    float3 SpecularMaterial = all(InKs == 0.0) ? float3(1, 1, 1) : InKs;
    float3 Specular = float3(0, 0, 0);
    if (NdotL > 0.0)
    {
        Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, SpecularMaterial, SpotlightColor, Shininess);
    }

    return Diffuse + Specular;
}

// For Gouraud (no Tilled Culling)
float3 CalculateAllLightsGouraud(float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 InKa, float3 InKd, float3 InKs, float Shininess)
{
    float3 AccumulatedColor = float3(0, 0, 0);

    for (uint i = 0; i < NumLights; ++i)
    {
        Light light = AllLights[i];
        uint lightType = (uint) light.direction.w;

        if (lightType == LIGHT_TYPE_AMBIENT)
        {
            FAmbientLightInfo ambientLight = ConvertToAmbientLight(light);
            AccumulatedColor += CalculateSingleAmbientLight(ambientLight, InKa, InKd);
        }
        else if (lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            FDirectionalLightInfo directionalLight = ConvertToDirectionalLight(light);
            AccumulatedColor += CalculateSingleDirectionalLight(directionalLight, WorldNormal, ViewDir, InKd, InKs, Shininess);
        }
        else if (lightType == LIGHT_TYPE_POINT)
        {
            FPointLightInfo pointLight = ConvertToPointLight(light);
            AccumulatedColor += CalculateSinglePointLight(pointLight, WorldPos, WorldNormal, ViewDir, InKd, InKs, Shininess);
        }
        else if (lightType == LIGHT_TYPE_SPOT)
        {
            FSpotLightInfo spotLight = ConvertToSpotLight(light);
            AccumulatedColor += CalculateSingleSpotLight(spotLight, WorldPos, WorldNormal, ViewDir, InKd, InKs, Shininess);
        }
    }

    return AccumulatedColor;
}

// Tiled Lighting (PS optimized)
float3 CalculateTiledLighting(float4 svPosition, float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 InKa, float3 InKd, float3 InKs, float Shininess, uint2 viewportOffset, uint2 viewportSize)
{
    float3 AccumulatedColor = float3(0, 0, 0);

    // Light Culling이 비활성화된 경우 모든 라이트를 렌더링
    if (EnableCulling == 0)
    {
        return CalculateAllLightsGouraud(WorldPos, WorldNormal, ViewDir, InKa, InKd, InKs, Shininess);
    }

    // Light Culling 활성화: 타일 기반 렌더링
    // Current pixel's tile index
    uint tileArrayIndex = GetCurrentTileArrayIndex(svPosition, viewportOffset, viewportSize);

    // Fetch tile's light information
    uint2 tileLightInfo = TileLightInfo[tileArrayIndex];
    uint lightIndexOffset = tileLightInfo.x;
    uint lightCount = tileLightInfo.y;

    // Iterate through all lights in the tile
    for (uint i = 0; i < lightCount; ++i)
    {
        uint lightIndex = LightIndexBuffer[lightIndexOffset + i];
        Light light = AllLights[lightIndex];

        uint lightType = (uint) light.direction.w;

        if (lightType == LIGHT_TYPE_AMBIENT)
        {
            FAmbientLightInfo ambientLight = ConvertToAmbientLight(light);
            AccumulatedColor += CalculateSingleAmbientLight(ambientLight, InKa, InKd);
        }
        else if (lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            FDirectionalLightInfo directionalLight = ConvertToDirectionalLight(light);
            AccumulatedColor += CalculateSingleDirectionalLight(directionalLight, WorldNormal, ViewDir, InKd, InKs, Shininess);
        }
        else if (lightType == LIGHT_TYPE_POINT)
        {
            FPointLightInfo pointLight = ConvertToPointLight(light);
            AccumulatedColor += CalculateSinglePointLight(pointLight, WorldPos, WorldNormal, ViewDir, InKd, InKs, Shininess);
        }
        else if (lightType == LIGHT_TYPE_SPOT)
        {
            FSpotLightInfo spotLight = ConvertToSpotLight(light);
            AccumulatedColor += CalculateSingleSpotLight(spotLight, WorldPos, WorldNormal, ViewDir, InKd, InKs, Shininess);
        }
    }

    return AccumulatedColor;
}

#endif // LIGHTING_COMMON_HLSLI