// LightingCommon.hlsli
// : Common structures for lighting information in shaders.

#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "ShaderDefines.hlsli"
#include "CommonBuffers.hlsli"

struct FLightSegment
{
    float3 Ambient;
    float3 Diffuse;
    float3 Specular;
};

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

// Light Culling
struct FLight
{
    float4 Position;    // xyz: 월드 위치, w: 영향 반경
    float4 Color;       // xyz: 색상, w: 강도
    float4 Direction;   // xyz: 방향 (스포트라이트용), w: 광원 타입
    float4 Angles;      // x: 내부 원뿔 각도(cos), y: 외부 원뿔 각도(cos), z: falloff extent/falloff, w: InvRange2 (스포트전용)
};

uint2 GetTileIndexFromPixelViewport(float2 screenPos, uint2 viewportOffset)
{
    float2 viewportRelativePos = screenPos - viewportOffset;
    return uint2(floor(viewportRelativePos.x / TILE_SIZE), floor(viewportRelativePos.y / TILE_SIZE));
}

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
    uint2 ViewportSize;   // viewport size
    uint NumLights;       // total number of lights in the scene (for Gouraud)
    uint EnableCulling;   // Light Culling 활성화 여부 (1=활성화, 0=모든라이트렌더)
};

// Light Culling용 Structured Buffer
StructuredBuffer<FLight> AllLights : register(t13);
StructuredBuffer<uint> LightIndexBuffer : register(t14);
StructuredBuffer<uint2> TileLightInfos : register(t15); // (offset, count)

FAmbientLightInfo ConvertToAmbientLight(FLight light)
{
    FAmbientLightInfo ambientLight;
    ambientLight.Color = float4(light.Color.rgb, 1.0); // Only RGB, no intensity in color
    ambientLight.Intensity = light.Color.w; // Intensity stored separately
    return ambientLight;
}

FDirectionalLightInfo ConvertToDirectionalLight(FLight light)
{
    FDirectionalLightInfo directionalLight;
    directionalLight.Direction = light.Direction.xyz;
    directionalLight.Color = light.Color.rgb;
    directionalLight.Intensity = light.Color.w;
    return directionalLight;
}

FPointLightInfo ConvertToPointLight(FLight light)
{
    FPointLightInfo pointLight;
    pointLight.Position = light.Position.xyz;
    pointLight.Radius = light.Position.w;
    pointLight.Color = light.Color.rgb;
    pointLight.Intensity = light.Color.w;
    pointLight.FalloffExtent = light.Angles.z;
    return pointLight;
}

FSpotLightInfo ConvertToSpotLight(FLight light)
{
    FSpotLightInfo spotLight;
    spotLight.Position = light.Position.xyz;
    spotLight.Direction = light.Direction.xyz;
    spotLight.Color = light.Color;
    spotLight.Intensity = light.Color.w;
    spotLight.CosInner = light.Angles.x;
    spotLight.CosOuter = light.Angles.y;
    spotLight.Falloff = light.Angles.z;
    spotLight.InvRange2 = light.Angles.w;
    return spotLight;
}

float3 CalculateDiffuse(float3 LightColor, float3 WorldNormal, float3 LightDir)
{
    WorldNormal = normalize(WorldNormal); /* Defensive Normalize*/
    LightDir = normalize(LightDir);       /* Defensive Normalize*/
    float NdotL = saturate(dot(WorldNormal, LightDir));
    return LightColor * NdotL;
}

float3 CalculateSpecular(float3 LightColor, float3 WorldNormal, float3 LightDir, float3 ViewDir, float Shininess)
{
    float NdotL = dot(WorldNormal, LightDir);
    float NdotV = dot(WorldNormal, ViewDir);

    if (NdotL <= 0 || NdotV <= 0) return 0;

    WorldNormal = normalize(WorldNormal); /* Defensive Normalize*/
    LightDir = normalize(LightDir);       /* Defensive Normalize*/
    ViewDir = normalize(ViewDir);         /* Defensive Normalize*/
#if LIGHTING_MODEL_LAMBERT
    return float3(0, 0, 0);
#elif LIGHTING_MODEL_PHONG
    float3 R = reflect(-LightDir, WorldNormal);
    float VdotR = saturate(dot(R, ViewDir));
    const float EPS = 1e-4;
    if (VdotR <= EPS )
        return 0;
    return LightColor * pow(VdotR, Shininess);
    
#elif LIGHTING_MODEL_BLINNPHONG
    float3 H = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(WorldNormal, H));
    return LightColor * pow(NdotH, Shininess);
#else
    float3 H = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(WorldNormal, H));
    return LightColor * pow(NdotH, Shininess);
#endif
}

float3 CalculateAmbientLight(FAmbientLightInfo ambientLight)
{
    return ambientLight.Color.rgb * ambientLight.Intensity;
}

float3 CalculateDirectionalLight(FDirectionalLightInfo DirectionalLight)
{
    return DirectionalLight.Color * DirectionalLight.Intensity;
}

float3 CalculatePointLight(FPointLightInfo PointLight, float3 WorldPos)
{
    float3 LightVec = PointLight.Position - WorldPos;
    float Distance = length(LightVec);

    if (Distance > PointLight.Radius)
        return float3(0, 0, 0);
    
    float RangeAttenuation = saturate(1.0 - (Distance * Distance) / (PointLight.Radius * PointLight.Radius));
    RangeAttenuation = pow(RangeAttenuation, max(PointLight.FalloffExtent, 0.0));

    return PointLight.Color.rgb * PointLight.Intensity * RangeAttenuation;
}

float3 CalculateSpotLight(FSpotLightInfo spotLight, float3 WorldPos)
{
    float3 LightVec = spotLight.Position - WorldPos;
    float Distance = length(LightVec);

    // Attenuation : Range & Cone(Cos)
    float RangeAttenuation = saturate(1.0 - Distance * Distance * spotLight.InvRange2);

    // SpotLight Cone Attenuation
    float CurCos = dot(-normalize(LightVec), normalize(spotLight.Direction));
    float DirectionAttenuation = saturate((CurCos - spotLight.CosOuter) / max(spotLight.CosInner - spotLight.CosOuter, 1e-5));
    if (DirectionAttenuation != 0) 
    {
        DirectionAttenuation = pow(DirectionAttenuation, max(spotLight.Falloff, 0.0));
    }

    // Light * Intensity
    return spotLight.Color.rgb * spotLight.Intensity * RangeAttenuation * DirectionAttenuation;
}

float3 CalculateLight(FLight Light, uint LightType, float3 WorldPos)
{
    if (LightType == LIGHT_TYPE_AMBIENT)
    {
        FAmbientLightInfo ambientLight = ConvertToAmbientLight(Light);
        return CalculateAmbientLight(ambientLight);
    }
    else if (LightType == LIGHT_TYPE_DIRECTIONAL)
    {
        FDirectionalLightInfo DirectionalLight = ConvertToDirectionalLight(Light);
        return CalculateDirectionalLight(DirectionalLight);
    }
    else if (LightType == LIGHT_TYPE_POINT)
    {
        FPointLightInfo PointLight = ConvertToPointLight(Light);
        return CalculatePointLight(PointLight, WorldPos);
    }
    else if (LightType == LIGHT_TYPE_SPOT)
    {
        FSpotLightInfo SpotLight = ConvertToSpotLight(Light);
        return CalculateSpotLight(SpotLight, WorldPos);
    }
    return float3(0, 0, 0);
}

// no Tilled Culling (ex Gouraud)
FLightSegment CalculateAllLights(float3 WorldPos, float3 WorldNormal, float3 ViewDir, float Shininess)
{
    FLightSegment AccumulatedColor;
    AccumulatedColor.Ambient = float3(0, 0, 0);
    AccumulatedColor.Diffuse = float3(0, 0, 0);
    AccumulatedColor.Specular = float3(0, 0, 0); 

    for (uint i = 0; i < NumLights; ++i)
    {
        float3 LightColor = float3(0, 0, 0);
        FLight Light = AllLights[i];
        uint LightType = (uint) Light.Direction.w;
        LightColor = CalculateLight(Light, LightType, WorldPos);

        if (LightType == LIGHT_TYPE_AMBIENT)
        {
            AccumulatedColor.Ambient += LightColor;
        }
        else
        {
            float3 LightDir;
            if (LightType == LIGHT_TYPE_DIRECTIONAL)
            {
                LightDir = normalize(-Light.Direction);
            }
            else
            {
                LightDir = Light.Position.xyz - WorldPos;
            }
            
            AccumulatedColor.Diffuse += CalculateDiffuse(LightColor, WorldNormal, LightDir);
            AccumulatedColor.Specular += CalculateSpecular(LightColor, WorldNormal, LightDir, ViewDir, Shininess);   
        }
    }

    return AccumulatedColor;
}

// Tiled Lighting (PS optimized) : Culling 활성화: 타일 기반 렌더링
FLightSegment CalculateTiledLighting(float4 svPosition, float3 WorldPos, float3 WorldNormal, float3 ViewDir, float Shininess,
    uint2 viewportOffset, uint2 viewportSize)
{
    // Light Culling 비활성화 : 모든 라이트를 렌더링
    if (EnableCulling == 0)
    {
        return CalculateAllLights(WorldPos, WorldNormal, ViewDir, Shininess);    
    }
    
    // ===== Tile Setup =====
    // Current pixel's tile index
    uint TileArrayIndex = GetCurrentTileArrayIndex(svPosition, viewportOffset, viewportSize);

    // Fetch tile's light information
    uint2 TileLightInfo = TileLightInfos[TileArrayIndex];
    uint LightIndexOffset = TileLightInfo.x;
    uint LightCount = TileLightInfo.y;

    // ===== Light Setup =====
    FLightSegment LightColor;
    LightColor.Ambient = float3(0, 0, 0);
    LightColor.Diffuse = float3(0, 0, 0);
    LightColor.Specular = float3(0, 0, 0);
    
    // Iterate through all lights in the tile
    for (uint i = 0; i < LightCount; ++i)
    {
        uint LightIndex = LightIndexBuffer[LightIndexOffset + i];
        FLight Light = AllLights[LightIndex];

        uint LightType = (uint) Light.Direction.w;
        float3 AccumulatedColor = CalculateLight(Light, LightType, WorldPos);

        // ===== Light Segment(A, D, S) Setup =====
        if (LightType == LIGHT_TYPE_AMBIENT)
        {
            LightColor.Ambient += AccumulatedColor;
        }
        else
        {
            float3 LightDir = Light.Position.xyz - WorldPos;
            LightColor.Diffuse += CalculateDiffuse(AccumulatedColor, WorldNormal, LightDir);
            LightColor.Specular += CalculateSpecular(AccumulatedColor, WorldNormal, LightDir, ViewDir, Shininess);   
        }
    }
    
    return LightColor;
}

#endif // LIGHTING_COMMON_HLSLI