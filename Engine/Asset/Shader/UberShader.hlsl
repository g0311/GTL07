// =============================================================================
// UberLit.hlsl
// A single "uber shader" supporting three classic lighting models and an Unlit view.

// View Modes:
//   VMI_Lit_Gouraud  : Per-Vertex lighting with Gouraud shading
//   VMI_Lit_Lambert  : Per-Pixel lighting with Lambertian reflectance
//   VMI_Lit_Phong    : Per-Pixel lighting with Blinn-Phong reflectance
//   VMI_Unlit        : No lighting calculations (outputs raw texture/color)

// Normal mapping supported via HAS_NORMAL_MAP (TBN)
// =============================================================================
#include "ShaderDefines.hlsli"
#include "LightingCommon.hlsli"
#include "CommonBuffers.hlsli"
#include "MaterialCommon.hlsli"

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

#ifdef HAS_NORMAL_MAP
    // Sample normal map (tangent space) - UV 파라미터 사용
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
#endif

    return WorldNormal;
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
    // Ka가 0이면 Kd 사용, Ks가 0이면 white(1,1,1) 사용
    float3 kA = all(Ka.rgb == 0.0) ? Kd.rgb : Ka.rgb;
    float3 kD = Kd.rgb;
    float3 kS = all(Ks.rgb == 0.0) ? float3(1, 1, 1) : Ks.rgb;

    // Gouraud: 모든 라이트를 순회 (Tiled Culling 사용 안 함)
    float3 LightColor = CalculateAllLightsGouraud(Output.WorldPosition, Normal, ViewDir, kA, kD, kS, Ns);

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

    // Normal 데이터 출력
    float3 Normal = CalculateWorldNormal(Input, Input.Tex);
    float3 EncodedNormal = Normal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);
    
    // 텍스처가 있다면 곱하기
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        float4 TexColor = DiffuseTexture.Sample(SamplerWrap, Input.Tex);
        Output.SceneColor.rgb *= TexColor.rgb;
        Output.SceneColor.a = TexColor.a;
    }

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

    // ===== Material Property Setup =====

    // Ambient: Use AmbientTexture if available, otherwise fallback to DiffuseTexture or Ka constant
    float3 kA;
    if (MaterialFlags & HAS_AMBIENT_MAP)
    {
        kA = AmbientTexture.Sample(SamplerWrap, UV).rgb;
    }
    else if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        kA = DiffuseTexture.Sample(SamplerWrap, UV).rgb;  // Fallback to diffuse texture
    }
    else
    {
        if (Ka.r == 0.0 && Ka.b == 0.0 && Ka.r == 0.0)
            kA = float3(1.0f, 1.0f, 1.0f);
        else
            kA = Ka.rgb;
    }

    // Diffuse: Use DiffuseTexture if available, otherwise Kd constant
    float3 kD;
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        kD = DiffuseTexture.Sample(SamplerWrap, UV).rgb;
    }
    else
    {
        if (Kd.r == 0.0 && Kd.b == 0.0 && Kd.r == 0.0)
            kD = float3(1.0f, 1.0f, 1.0f);
        else
            kD = Kd.rgb;
    }

    // Specular: Use SpecularTexture if available, otherwise fallback to DiffuseTexture or Ks constant
    float3 kS;
    if (MaterialFlags & HAS_SPECULAR_MAP)
    {
        kS = SpecularTexture.Sample(SamplerWrap, UV).rgb;
    }
    else if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        kS = DiffuseTexture.Sample(SamplerWrap, UV).rgb;  // Fallback to diffuse texture
    }
    else
    {
        if (Ks.r == 0.0 && Ks.b == 0.0 && Ks.r == 0.0)
            kS = float3(1.0f, 1.0f, 1.0f);
        else
            kS = Ks.rgb;
    }

    float Shininess = 1.0f;
    if (Ns != 0)
        Shininess = Ns;
    
    if (MaterialFlags & UNLIT)
    {
        Output.SceneColor = float4(kD, 1.0);
        Output.NormalData = float4(Normal * 0.5f + 0.5f, 1.0);
        return Output;
    }
    
    float3 TiledLightColor = CalculateTiledLighting(Input.Position, Input.WorldPosition, Normal, ViewDir, kA, kD, kS, Shininess, ViewportOffset, ViewportSize);
    
    float4 FinalColor;
    FinalColor.rgb = TiledLightColor;
    FinalColor.a = 1.0f;
     
    // Alpha handling
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
    
    return Output;
};