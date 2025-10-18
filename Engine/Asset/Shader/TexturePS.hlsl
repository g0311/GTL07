#include "TextureVS.hlsl"

cbuffer MaterialConstants : register(b2)
{
    float4 Ka;		    // Ambient color
    float4 Kd;		    // Diffuse color
    float4 Ks;		    // Specular color
    float Ns;		    // Specular exponent
    float Ni;		    // Index of refraction
    float D;		    // Dissolve factor
    uint MaterialFlags;	// Which textures are available (bitfield)
    float Time;         // Time in seconds
    float HeightScale;  // Parallax height scale
    float Padding[3];   // 16-byte alignment
};

Texture2D DiffuseTexture : register(t0);	// map_Kd
Texture2D AmbientTexture : register(t1);	// map_Ka
Texture2D SpecularTexture : register(t2);	// map_Ks

#if defined(HAS_NORMAL_MAP)
Texture2D NormalTexture : register(t3);		// map_Ns
#endif

Texture2D AlphaTexture : register(t4);		// map_d

#if defined(HAS_HEIGHT_MAP)
Texture2D HeightTexture : register(t5);		// Height Map (for Parallax)
#endif

SamplerState SamplerWrap : register(s0);

// Material flags (bit values must match CoreTypes.h)
#define DIFFUSE_MAP_FLAG  (1 << 0)
#define AMBIENT_MAP_FLAG  (1 << 1)
#define SPECULAR_MAP_FLAG (1 << 2)
#define NORMAL_MAP_FLAG   (1 << 3)
#define ALPHA_MAP_FLAG    (1 << 4)
#define HEIGHT_MAP_FLAG   (1 << 5)

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

#if defined(HAS_HEIGHT_MAP)
// World space vector를 Tangent space로 변환
float3 WorldToTangentSpace(float3 worldVec, float3 worldNormal, float3 worldTangent, float3 worldBitangent)
{
    // TBN 행렬 구성 (Tangent, Bitangent, Normal)
    float3 T = normalize(worldTangent);
    float3 B = normalize(worldBitangent);
    float3 N = normalize(worldNormal);

    // World space -> Tangent space 변환
    float3x3 TBN = float3x3(T, B, N);
    return mul(worldVec, TBN);
}

// Simple Parallax Mapping (1단계 오프셋)
float2 SimpleParallaxMapping(float2 UV, float3 ViewDirTangent, float heightScale)
{
    // Height 샘플링
    float height = HeightTexture.Sample(SamplerWrap, UV).r;

    // UV 오프셋 계산
    // ViewDir.xy = tangent space에서의 수평 방향
    float2 offset = ViewDirTangent.xy * (height * heightScale);

    // UV 수정
    return UV - offset;
}

// Parallax Occlusion Mapping (Ray Marching with interpolation)
float2 ParallaxOcclusionMapping(float2 UV, float3 ViewDirTangent, float heightScale)
{
    // 레이어 수 동적 조절 (각도에 따라)
    const int minLayers = 8;
    const int maxLayers = 32;
    float numLayers = lerp(float(maxLayers), float(minLayers), abs(dot(float3(0, 0, 1), ViewDirTangent)));

    // 초기화
    float layerDepth = 1.0f / numLayers;
    float currentLayerDepth = 0.0f;
    float2 deltaUV = ViewDirTangent.xy * heightScale / numLayers;
    float2 currentUV = UV;

    float currentHeight = HeightTexture.Sample(SamplerWrap, currentUV).r;

    // Ray Marching: 표면과 교차점 찾기
    [unroll(32)]
    for(int i = 0; i < 32 && currentLayerDepth < currentHeight; i++)
    {
        // 다음 레이어로 이동
        currentUV -= deltaUV;
        currentHeight = HeightTexture.Sample(SamplerWrap, currentUV).r;
        currentLayerDepth += layerDepth;
    }

    // Relief Mapping: 선형 보간으로 부드럽게
    float2 prevUV = currentUV + deltaUV;
    float afterDepth = currentHeight - currentLayerDepth;
    float beforeDepth = HeightTexture.Sample(SamplerWrap, prevUV).r - currentLayerDepth + layerDepth;

    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalUV = lerp(currentUV, prevUV, weight);

    return finalUV;
}
#endif

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;

    float4 FinalColor = float4(0.f, 0.f, 0.f, 1.f);
    float2 UV = Input.Tex;

    // Parallax UV 계산 (Height Map이 있을 때만)
    #if defined(HAS_HEIGHT_MAP)
    if (MaterialFlags & HEIGHT_MAP_FLAG)
    {
        // View Direction 계산 (World space)
        float3 ViewDirWorld = normalize(ViewWorldLocation - Input.WorldPosition);

        // Tangent space로 변환
        float3 ViewDirTangent = WorldToTangentSpace(
            ViewDirWorld,
            Input.WorldNormal,
            Input.WorldTangent,
            Input.WorldBitangent
        );

        // Parallax Occlusion Mapping 적용
        UV = ParallaxOcclusionMapping(UV, ViewDirTangent, HeightScale);
    }
    #endif

    // Base diffuse color
    float4 DiffuseColor = Kd;
    if (MaterialFlags & DIFFUSE_MAP_FLAG)
    {
        DiffuseColor *= DiffuseTexture.Sample(SamplerWrap, UV);
        FinalColor.a = DiffuseColor.a;
    }

    // Ambient contribution
    float4 AmbientColor = Ka;
    if (MaterialFlags & AMBIENT_MAP_FLAG)
    {
        AmbientColor *= AmbientTexture.Sample(SamplerWrap, UV);
    }

    FinalColor.rgb = DiffuseColor.rgb + AmbientColor.rgb;

    // Alpha handling
    if (MaterialFlags & ALPHA_MAP_FLAG)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
        FinalColor.a = D;
        FinalColor.a *= alpha;
    }

    Output.SceneColor = FinalColor;

    // Calculate World Normal for Normal Buffer
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

    #if defined(HAS_NORMAL_MAP)
    if (MaterialFlags & NORMAL_MAP_FLAG)
    {
        // Sample normal map (tangent space)
        float3 TangentNormal = NormalTexture.Sample(SamplerWrap, UV).rgb;

        // Decode from [0,1] to [-1,1]
        TangentNormal = TangentNormal * 2.0f - 1.0f;

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
    #endif

    // Encode world normal to [0,1] range for storage
    float3 EncodedNormal = WorldNormal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
}
