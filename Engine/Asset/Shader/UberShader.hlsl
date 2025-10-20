// Light Culling에서 공유하는 헤더 파일 내용
// LightCulling.hlsl의 유틸리티 함수들을 포함
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
    float4 position;    // xyz: 월드 위치, w: 영향 반경
    float4 color;       // xyz: 색상, w: 강도
    float4 direction;   // xyz: 방향 (스포트라이트용), w: 광원 타입
    float4 angles;      // x: 내부 원뿔 각도(cos), y: 외부 원뿔 각도(cos), z: falloff extent/falloff, w: InvRange2 (스포트전용)
};

// 광원 타입 정의 (C++ ELightType enum과 동일한 순서)
#define LIGHT_TYPE_AMBIENT 0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_POINT 2
#define LIGHT_TYPE_SPOT 3

// 기존 라이트 구조체들 (호환성을 위해 유지)
#define NUM_POINT_LIGHT 8
#define NUM_SPOT_LIGHT 8
#define NUM_AMBIENT_LIGHT 8

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

cbuffer Model : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 WorldInverseTranspose;
}

cbuffer Camera : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 ViewWorldLocation;    
    float NearClip;
    float FarClip;
};

cbuffer MaterialConstants : register(b2) // b0, b1 is in VS
{
    float4 Ka;		// Ambient color
    float4 Kd;		// Diffuse color
    float4 Ks;		// Specular color
    float Ns;		// Specular exponent (Shineness)
    float Ni;		// Index of refraction
    float D;		// Dissolve factor
    uint MaterialFlags;
    float Time;
    float HeightScale; // POM depth scale
    float2 HeightTextureSize; // Height map resolution for normalization
};

// Tiled Lighting을 위한 뷰포트 정보
cbuffer TiledLightingParams : register(b3)
{
    uint2 ViewportOffset;   // 뷰포트 오프셋
    uint2 ViewportSize;     // 뷰포트 크기
    uint NumLights;         // 씬의 전체 라이트 개수 (Gouraud용)
    uint _padding;          // 16바이트 정렬
};
// Light Culling시스템에서 사용하는 Structured Buffer
StructuredBuffer<Light> AllLights : register(t13);             // 라이트 데이터 버퍼
StructuredBuffer<uint> LightIndexBuffer : register(t14);       // 타일별 라이트 인덱스
StructuredBuffer<uint2> TileLightInfo : register(t15);         // 타일 라이트 정보 (offset, count)

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
#define UNLIT			 (1 << 6)

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition: TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 Tex : TEXCOORD2;
    float3 WorldTangent : TEXCOORD3;
    float3 WorldBitangent : TEXCOORD4;
    float4 Color : COLOR;
};

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
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
    float3 H = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(WorldNormal, H));
    return Ks * LightColor * pow(NdotH, Shininess);
#endif
}

// ------------------------------------- Used ------------------------------------------//
// Light 구조체를 기존 구조체로 변환하는 헬퍼 함수들
FAmbientLightInfo ConvertToAmbientLight(Light light)
{
    FAmbientLightInfo ambientLight;
    ambientLight.Color = float4(light.color.rgb, 1.0);  // Only RGB, no intensity in color
    ambientLight.Intensity = light.color.w;  // Intensity stored separately
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

//==================================================//
//==================== POM Functions ===============//
//==================================================//

// World space vector를 Tangent space로 변환
float3 WorldToTangentSpace(float3 worldVec, float3 worldNormal, float3 worldTangent, float3 worldBitangent)
{
    float3 T = normalize(worldTangent);
    float3 B = normalize(worldBitangent);
    float3 N = normalize(worldNormal);

    // World -> Tangent 변환 행렬 (전치 행렬)
    float3x3 TBN = float3x3(T, B, N);
    return mul(TBN, worldVec);
}

// Parallax Occlusion Mapping with Texture Size Normalization
float2 ParallaxOcclusionMapping(
    float2 UV,
    float3 ViewDirTangent,
    float heightScale,
    float2 textureSize)
{
    // Safety check: ViewDirTangent이 너무 작으면 POM 비활성화
    if (length(ViewDirTangent) < 0.001f)
        return UV;

    // ViewDirTangent.z가 0에 가까우면 (거의 수평) POM 비활성화
    if (abs(ViewDirTangent.z) < 0.1f)
        return UV;

    // 레이어 수 동적 조절 (각도에 따라)
    const int minLayers = 8;
    const int maxLayers = 32;
    float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0, 0, 1), ViewDirTangent)));

    float layerDepth = 1.0f / numLayers;
    float currentLayerDepth = 0.0f;

    // ViewDirTangent.z로 나눠서 올바른 parallax 비율 계산
    float2 parallaxDir = ViewDirTangent.xy / ViewDirTangent.z;
    parallaxDir.x = -parallaxDir.x;  // X축 반전

    // 텍스처 해상도 정규화 제거 (문제 발생)
    // 대신 heightScale만 사용
    float2 deltaUV = parallaxDir * heightScale / numLayers;

    // Ray Marching: 표면과 교차점 찾기
    float2 currentUV = UV;
    float currentHeight = BumpTexture.Sample(SamplerWrap, currentUV).r;

    [unroll(32)]
    for(int i = 0; i < numLayers; i++)
    {
        // 현재 레이어 깊이가 높이맵보다 깊으면 교차점 발견
        if(currentLayerDepth >= currentHeight)
            break;

        // UV 이동 및 다음 높이 샘플링
        currentUV -= deltaUV;
        currentHeight = BumpTexture.Sample(SamplerWrap, currentUV).r;
        currentLayerDepth += layerDepth;
    }

    // 선형 보간 (Relief Mapping) - 교차점 정밀화
    float2 prevUV = currentUV + deltaUV;

    float afterDepth = currentHeight - currentLayerDepth;
    float beforeDepth = BumpTexture.Sample(SamplerWrap, prevUV).r - (currentLayerDepth - layerDepth);

    // Division by zero 방지
    float denominator = afterDepth - beforeDepth;
    float weight = 0.5f; // 기본값
    if (abs(denominator) > 0.0001f)
    {
        weight = saturate(afterDepth / denominator);
    }

    float2 finalUV = lerp(currentUV, prevUV, weight);

    // UV가 너무 많이 이동했다면 원본 UV 반환
    float2 uvDelta = abs(finalUV - UV);
    if (uvDelta.x > 0.5f || uvDelta.y > 0.5f)
    {
        return UV;
    }

    return finalUV;
}

float3 CalculateWorldNormal(PS_INPUT Input, float2 UV)
{
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
    
    // Sample normal map (tangent space) - UV 파라미터 사용
    if (MaterialFlags & HAS_NORMAL_MAP)
    {
        float3 TangentNormal = NormalTexture.Sample(SamplerWrap, UV).rgb;
        // Decode from [0,1] to [-1,1]
        TangentNormal = TangentNormal * 2.0f - 1.0f;
        // DirectX 텍스처 좌표계 보정: X축 반전
        TangentNormal.x = -TangentNormal.x;

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

    return WorldNormal;
}

//==================================================//
//====================Tile Light Calc====================//
//==================================================//
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

    float RangeAttenuation = saturate(1.0 - Distance/pointLight.Radius);
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

// Gouraud용: 모든 라이트를 순회하는 함수 (Tiled Culling 없이)
float3 CalculateAllLightsGouraud(float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 AccumulatedColor = float3(0, 0, 0);

    // 모든 라이트를 순회
    for (uint i = 0; i < NumLights; ++i)
    {
        Light light = AllLights[i];
        uint lightType = (uint)light.direction.w;

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

// Tiled Lighting 메인 계산 함수 (PS용 - 최적화됨)
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

        uint lightType = (uint)light.direction.w;

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

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
    Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);

    // Transform normal to world space using inverse transpose for non-uniform scale
    // Do NOT normalize here - let GPU interpolate, then normalize in PS
    Output.WorldNormal = mul(Input.Normal, (float3x3)WorldInverseTranspose);

    // Transform tangent to world space using inverse transpose
    Output.WorldTangent = mul(Input.Tangent, (float3x3)WorldInverseTranspose);

    // Transform bitangent to world space using inverse transpose
    Output.WorldBitangent = mul(Input.Bitangent, (float3x3)WorldInverseTranspose);

    Output.Tex = Input.Tex;
    Output.Color = Input.Color;
#if LIGHTING_MODEL_GOURAUD
    float3 Normal = normalize(Output.WorldNormal);
    float3 ViewDir = normalize(ViewWorldLocation - Output.WorldPosition);

    // Vertex Shader에서는 텍스처 샘플링 불가 - Material 상수만 사용
    float3 kD = Kd.rgb;
    float3 kS = Ks.rgb;

    // Gouraud: 모든 라이트를 순회 (Tiled Culling 사용 안 함)
    float3 LightColor = CalculateAllLightsGouraud(Output.WorldPosition, Normal, ViewDir, kD, kS, Ns);

    // 입력 버텍스 컬러와 라이팅 결과를 블렌드
    Output.Color.rgb = LightColor;
    Output.Color.a = Input.Color.a;
#endif
    return Output;
}

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;
#if LIGHTING_MODEL_GOURAUD
    // Gouraud: VS에서 계산된 라이팅을 그대로 사용
    Output.SceneColor = Input.Color;

    // 텍스처가 있다면 곱하기
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        float4 TexColor = DiffuseTexture.Sample(SamplerWrap, Input.Tex);
        Output.SceneColor.rgb *= TexColor.rgb;
        Output.SceneColor.a = TexColor.a;
    }

    // Normal 데이터 출력
    float3 Normal = CalculateWorldNormal(Input, Input.Tex);
    float3 EncodedNormal = Normal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);
#elif LIGHTING_MODEL_UNLIT
    // TODO : 원래는 Albedo로 해야하지만, 현재는 DiffuseTexture = Albedo임
    Output.SceneColor= float4(0.5, 0.5, 0.5, 1);
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        float4 TexColor = DiffuseTexture.Sample(SamplerWrap, Input.Tex);
        Output.SceneColor= TexColor;
    };
#else
    float2 UV = Input.Tex;
    float3 ViewDir  = normalize(ViewWorldLocation - Input.WorldPosition);

    // POM: Parallax Occlusion Mapping 적용
    if (MaterialFlags & HAS_BUMP_MAP)
    {
        float3 ViewDirTangent = WorldToTangentSpace(
            ViewDir,
            Input.WorldNormal,
            Input.WorldTangent,
            Input.WorldBitangent
        );

        UV = ParallaxOcclusionMapping(UV, ViewDirTangent, HeightScale, HeightTextureSize);
    }

    // Normal 계산 (POM으로 수정된 UV 사용)
    float3 Normal = CalculateWorldNormal(Input, UV);

    // Encode world normal to [0,1] range for storage
    float3 EncodedNormal = Normal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    // 기본 Albedo: map_Kd (DiffuseTexture)가 물체의 기본 색상 (POM UV 사용)
    float3 Albedo = (MaterialFlags & HAS_DIFFUSE_MAP) ? DiffuseTexture.Sample(SamplerWrap, UV).rgb : Kd.rgb;

    // Diffuse: 기본적으로 Albedo 사용
    float3 kD = Albedo;

    // Specular: 별도 SpecularTexture가 설정되었다면 사용, 아니면 Ks 상수
    float3 kS = (MaterialFlags & HAS_SPECULAR_MAP) ? SpecularTexture.Sample(SamplerWrap, UV).rgb : Ks.rgb;
     
    // Unlit 처리
    if (MaterialFlags & UNLIT)
    {
        Output.SceneColor = float4(kD, 1.0);
        Output.NormalData = float4(Normal * 0.5f + 0.5f, 1.0);
        return Output;
    }

    // Tiled Lighting 사용
    float3 TiledLightColor = CalculateTiledLighting(Input.Position, Input.WorldPosition, Normal, ViewDir, kD, kS, Ns, ViewportOffset, ViewportSize);
    
     
    // Light
    // float3 AmbientColor = CalculateAmbientLight(UV).rgb;
    // float3 DirectionalColor = CalculateDirectionalLight(Normal, ViewDir, kD, kS, Ns);
    // float3 PointLightColor = CalculatePointLights(Input.WorldPosition, Normal, ViewDir, kD, kS, Ns);
    // float3 SpotLightColor = CalculateSpotLights(Input.WorldPosition, Normal, ViewDir, kD, kS, Ns);
         
    float4 FinalColor;
    FinalColor.rgb = TiledLightColor;
    FinalColor.a = 1.0f;
     
    // Alpha handling
    /*TODO : Apply DiffuseTexture for now, due to Binding AlphaTexture feature don't exist yet*/
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).w;
        FinalColor.a = D * alpha;
    }
    else if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        float alpha = DiffuseTexture.Sample(SamplerWrap, UV).w;
        FinalColor.a = D * alpha;
    }
    Output.SceneColor = FinalColor;
#endif
    
    // // Normal
    // // Calculate World Normal for Normal Buffer
    // float3 WorldNormal = CalculateWorldNormal(Input);
    //
    // // Encode world normal to [0,1] range for storage
    // float3 EncodedNormal = WorldNormal * 0.5f + 0.5f;
    // Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
};