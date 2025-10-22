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

// NDC 깊이(0~1)를 뷰 공간 깊이로 변환
// Projection 행렬을 사용하여 역변환
float NDCDepthToViewZ(float ndcDepth, matrix proj)
{
    // DirectX perspective projection:
    // proj[2][2] = Far / (Far - Near)
    // proj[3][2] = -Near * Far / (Far - Near)
    // z_ndc = (z_view * proj[2][2] + proj[3][2]) / z_view
    // 
    // 역변환:
    // z_ndc * z_view = z_view * proj[2][2] + proj[3][2]
    // z_view * (z_ndc - proj[2][2]) = proj[3][2]
    // z_view = proj[3][2] / (z_ndc - proj[2][2])
    
    return proj[3][2] / (ndcDepth - proj[2][2]);
}

// 클러스터 인덱스 계산 (픽셀 SV_Position과 뷰 공간 Z 기반)
// 선형 분포를 사용
uint3 GetClusterIndexFromSVPos(float4 svPosition, float viewZ, uint2 viewportOffset, uint2 viewportSize, float nearClip, float farClip)
{
    // 화면 좌표 -> 뷰포트 상대 좌표
    float2 viewportRelativePos = svPosition.xy - viewportOffset;
    uint clusterX = (uint)floor(viewportRelativePos.x / CLUSTER_SIZE_X);
    uint clusterY = (uint)floor(viewportRelativePos.y / CLUSTER_SIZE_Y);

    // 뷰 공간 Z를 선형 분포로 클러스터 Z 인덱스 계산
    float t = (viewZ - nearClip) / (farClip - nearClip);
    uint clusterZ = (uint)min((uint)(t * CLUSTER_SIZE_Z), CLUSTER_SIZE_Z - 1);
    
    return uint3(clusterX, clusterY, clusterZ);
}

// 현재 픽셀의 클러스터 1D 배열 인덱스 계산
uint GetCurrentClusterArrayIndex(float4 svPosition, float viewZ, uint2 viewportOffset, uint2 viewportSize)
{
    uint2 clustersXY = uint2((viewportSize.x + CLUSTER_SIZE_X - 1) / CLUSTER_SIZE_X,
                             (viewportSize.y + CLUSTER_SIZE_Y - 1) / CLUSTER_SIZE_Y);
    uint3 c = GetClusterIndexFromSVPos(svPosition, viewZ, viewportOffset, viewportSize, NearClip, FarClip);
    return c.z * (clustersXY.x * clustersXY.y) + c.y * clustersXY.x + c.x;
}

// Viewport Info for Tiled Lighting
cbuffer TiledLightingParams : register(b3)
{
    uint2 ViewportOffset; // viewport offset
    uint2 ViewportSize;   // viewport size
    uint NumLights;       // total number of lights in the scene (for Gouraud)
    uint EnableCulling;   // Light Culling 활성화 여부 (1=활성화, 0=모든라이트렌더)
    uint EnableCullingDebug; // Light Culling Debug 모드 활성화 여부 (1=활성화, 0=비활성화)
    uint Padding;
};

// Light Culling용 Structured Buffer
StructuredBuffer<FLight> AllLights : register(t13);
StructuredBuffer<uint> LightIndexBuffer : register(t14);
StructuredBuffer<uint2> ClusterLightInfo : register(t15); // (offset, count)

FAmbientLightInfo ConvertToAmbientLight(FLight Light)
{
    FAmbientLightInfo AmbientLight;
    AmbientLight.Color = float4(Light.Color.rgb, 1.0); // Only RGB, no intensity in color
    AmbientLight.Intensity = Light.Color.w; // Intensity stored separately
    return AmbientLight;
}

FDirectionalLightInfo ConvertToDirectionalLight(FLight Light)
{
    FDirectionalLightInfo DirectionalLight;
    DirectionalLight.Direction = Light.Direction.xyz;
    DirectionalLight.Color = Light.Color.rgb;
    DirectionalLight.Intensity = Light.Color.w;
    return DirectionalLight;
}

FPointLightInfo ConvertToPointLight(FLight Light)
{
    FPointLightInfo PointLight;
    PointLight.Position = Light.Position.xyz;
    PointLight.Radius = Light.Position.w;
    PointLight.Color = Light.Color.rgb;
    PointLight.Intensity = Light.Color.w;
    PointLight.FalloffExtent = Light.Angles.z;
    return PointLight;
}

FSpotLightInfo ConvertToSpotLight(FLight Light)
{
    FSpotLightInfo SpotLight;
    SpotLight.Position = Light.Position.xyz;
    SpotLight.Direction = Light.Direction.xyz;
    SpotLight.Color = Light.Color;
    SpotLight.Intensity = Light.Color.w;
    SpotLight.CosInner = Light.Angles.x;
    SpotLight.CosOuter = Light.Angles.y;
    SpotLight.Falloff = Light.Angles.z;
    SpotLight.InvRange2 = Light.Angles.w;
    return SpotLight;
}

#ifndef PI
#define PI 3.14159265358979323846
#endif
static const float INV_PI = 1.0 / PI;

float3 CalculateDiffuse(float3 LightColor, float3 WorldNormal, float3 LightDir)
{
    WorldNormal = normalize(WorldNormal); /* Defensive Normalize*/
    LightDir = normalize(LightDir);       /* Defensive Normalize*/
    float NdotL = dot(WorldNormal, LightDir);
    return LightColor * NdotL * INV_PI;;
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

float3 CalculateAmbientLight(FAmbientLightInfo AmbientLight)
{
    return AmbientLight.Color.rgb * AmbientLight.Intensity;
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

float3 CalculateSpotLight(FSpotLightInfo SpotLight, float3 WorldPos)
{
    float3 LightVec = SpotLight.Position - WorldPos;
    float Distance = length(LightVec);

    // Attenuation : Range & Cone(Cos)
    float RangeAttenuation = saturate(1.0 - Distance * Distance * SpotLight.InvRange2);
    if (RangeAttenuation != 0)
    {
        RangeAttenuation = pow(RangeAttenuation, max(SpotLight.Falloff, 0.0));
    }

    // SpotLight Cone Attenuation
    float CurCos = dot(-normalize(LightVec), normalize(SpotLight.Direction));
    float DirectionAttenuation = saturate((CurCos - SpotLight.CosOuter) / max(SpotLight.CosInner - SpotLight.CosOuter, 1e-5));
    if (DirectionAttenuation != 0) 
    {
        DirectionAttenuation = pow(DirectionAttenuation, max(SpotLight.Falloff, 0.0));
    }
    
    // Light * Intensity
    return SpotLight.Color.rgb * SpotLight.Intensity * RangeAttenuation * DirectionAttenuation;
}

float3 CalculateLight(FLight Light, uint LightType, float3 WorldPos)
{
    if (LightType == LIGHT_TYPE_AMBIENT)
    {
        FAmbientLightInfo AmbientLight = ConvertToAmbientLight(Light);
        return CalculateAmbientLight(AmbientLight);
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
                LightDir = normalize(Light.Position.xyz - WorldPos);
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
    float viewZ = svPosition.w;
    uint clusterArrayIndex = GetCurrentClusterArrayIndex(svPosition, viewZ, viewportOffset, viewportSize);

    // Fetch cluster's light information
    uint2 clusterLight = ClusterLightInfo[clusterArrayIndex];
    uint LightIndexOffset = clusterLight.x;
    uint LightCount = clusterLight.y;

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
            float3 LightDir = normalize(Light.Position.xyz - WorldPos);
            LightColor.Diffuse += CalculateDiffuse(AccumulatedColor, WorldNormal, LightDir);
            LightColor.Specular += CalculateSpecular(AccumulatedColor, WorldNormal, LightDir, ViewDir, Shininess);   
        }
    }
    
    // 컬링 디버그 모드 활성화 시 라이트 개수에 따라 색깔 설정
    if (EnableCullingDebug == 1)
    {
        float t = saturate((float)LightCount / 20.0); // 20개 라이트 기준으로 정규화
        
        // Heat map 색상: 파란색(0) -> 초록색 -> 노란색 -> 빨간색(많음)
        float3 debugColor;
        if (t < 0.25)
        {
            // 파란색 -> 하늘색
            debugColor = lerp(float3(0.0, 0.0, 1.0), float3(0.0, 1.0, 1.0), t * 4.0);
        }
        else if (t < 0.5)
        {
            // 하늘색 -> 초록색
            debugColor = lerp(float3(0.0, 1.0, 1.0), float3(0.0, 1.0, 0.0), (t - 0.25) * 4.0);
        }
        else if (t < 0.75)
        {
            // 초록색 -> 노란색
            debugColor = lerp(float3(0.0, 1.0, 0.0), float3(1.0, 1.0, 0.0), (t - 0.5) * 4.0);
        }
        else
        {
            // 노란색 -> 빨간색
            debugColor = lerp(float3(1.0, 1.0, 0.0), float3(1.0, 0.0, 0.0), (t - 0.75) * 4.0);
        }
        
        // 기존 라이팅 결과를 디버그 색상으로 덮어쓰기
        LightColor.Diffuse = debugColor;
    }
    
    return LightColor;
}

#endif // LIGHTING_COMMON_HLSLI