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
    float3 Ambient : LIGHT0; /* Gouraud */
    float3 Diffuse : LIGHT1; /* Gouraud */
    float3 Specular : LIGHT2; /* Gouraud */
};

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

// #ifdef HAS_NORMAL_MAP
//==================================================//
//==================== POM Functions ===============//
//==================================================//

// World space vector를 Tangent space로 변환
float3 WorldToTangentSpace(
    float3 WorldVec,
    float3 WorldNormal,
    float3 WorldTangent,
    float3 WorldBitangent)
{
    float3 T = normalize(WorldTangent);
    float3 B = normalize(WorldBitangent);
    float3 N = normalize(WorldNormal);

    // World -> Tangent 변환 행렬 (전치 행렬)
    float3x3 TBN = transpose(float3x3(T, B, N));
    return mul(WorldVec, TBN);
}

// Parallax Occlusion Mapping with Texture Size Normalization
float2 ParallaxOcclusionMapping(
    float2 UV,
    float3 ViewDirTangent,
    float HeightScale)
{
    // Safety check: ViewDirTangent이 너무 작으면 POM 비활성화
    if (length(ViewDirTangent) < 0.001f)
        return UV;

    // ViewDirTangent.z가 0에 가까우면 (거의 수평) POM 비활성화
    if (abs(ViewDirTangent.z) < 0.1f)
        return UV;

    // 레이어 수 동적 조절 (각도에 따라)
    const int MinLayers = 32;
    const int MaxLayers = 128;
    float NumLayers = lerp(MaxLayers, MinLayers, abs(dot(float3(0, 0, 1), ViewDirTangent)));

    float LayerDepth = 1.0f / NumLayers;
    float CurrentLayerDepth = 0.0f;

    // ViewDirTangent.z로 나눠서 올바른 parallax 비율 계산
    float2 ParallaxDir = ViewDirTangent.xy / ViewDirTangent.z;
    ParallaxDir.x = -ParallaxDir.x;  // X축 반전

    // 텍스처 해상도 정규화 제거 (문제 발생)
    // 대신 HeightScale만 사용
    float2 DeltaUV = ParallaxDir * HeightScale / NumLayers;

    // Ray Marching: 표면과 교차점 찾기
    float2 CurrentUV = UV;
    float CurrentHeight = BumpTexture.Sample(SamplerWrap, CurrentUV).r;

    [unroll(64)]
    for(int i = 0; i < NumLayers; i++)
    {
        // // 현재 레이어 깊이가 높이맵보다 깊으면 교차점 발견
        // float FlipHeight = 1.0f - CurrentHeight; // 높이맵은 흰색=높음, 검정색=낮음
        // if(CurrentLayerDepth >= FlipHeight)
        // {
        //     break;
        // }

        if(CurrentLayerDepth >= CurrentHeight)
        {
             break;
        }
        
        // UV 이동 및 다음 높이 샘플링
        // CurrentUV += DeltaUV;
        CurrentUV -= DeltaUV;
        CurrentHeight = BumpTexture.Sample(SamplerWrap, CurrentUV).r;
        CurrentLayerDepth += LayerDepth;
    }

    // 선형 보간 (Relief Mapping) - 교차점 정밀화
    float2 PrevUV = CurrentUV + DeltaUV;

    float AfterDepth = CurrentHeight - CurrentLayerDepth;
    float BeforeDepth = BumpTexture.Sample(SamplerWrap, PrevUV).r - (CurrentLayerDepth - LayerDepth);

    // Division by zero 방지
    float Denominator = AfterDepth - BeforeDepth;
    float Weight = 0.5f; // 기본값
    if (abs(Denominator) > 0.0001f)
    {
        Weight = saturate(AfterDepth / Denominator);
    }

    float2 FinalUV = lerp(CurrentUV, PrevUV, Weight);

    // UV가 너무 많이 이동했다면 원본 UV 반환
    float2 UVDelta = abs(FinalUV - UV);
    if (UVDelta.x > 0.5f || UVDelta.y > 0.5f)
    {
        return UV;
    }

    return FinalUV;
}



/* POM Diff Version : Test If you want */
// // --- Helpers ---
// float2 ClampToRect(float2 uv, float2 rmin, float2 rmax)
// {
//     return float2(clamp(uv.x, rmin.x, rmax.x),
//                   clamp(uv.y, rmin.y, rmax.y));
// }
//
// // 원래 UV(Base) 기준으로 "반대편으로 넘어가지 않게" 보정
// float ClampOneSidedFromBase(float baseV, float valueV)
// {
//     float shift = valueV - baseV;
//     // +방향으로 밀렸다면 base보다 작아지는 것(반대편) 금지
//     // -방향으로 밀렸다면 base보다 커지는 것(반대편) 금지
//     return (shift > 0.0) ? max(valueV, baseV)
//                          : min(valueV, baseV);
// }
//
// float2 ParallaxOcclusionMapping(
//     float2 UV,
//     float3 ViewDirTangent,
//     float  HeightScale)
// {
//     // cbuffer 미바인딩 시 기본값 사용
//     float2 AtlasMin         = float2(0,0);
//     float2 AtlasMax         = float2(1,1);
//     int    MinLayers        = 128;
//     int    MaxLayers        = 256;
//     float  GrazingZMin      = 0.1f;
//     float  UvShiftClamp     = 0.5f;
//     int    BinarySearchStep = 5;
//     float  FlipX            = -1.0f; // DirectX 좌표계 X 반전
//
//     // 1) 안전 체크
//     if (length(ViewDirTangent) < 1e-3 || abs(HeightScale) < 1e-5)
//         return UV;
//
//     // 2) 정규화 + 그래이징 하한
//     float3 V     = normalize(ViewDirTangent);
//     float  AbsVz = max(abs(V.z), GrazingZMin);
//
//     // 3) 각도 기반 레이어 수
//     float  t          = saturate(abs(V.z));
//     float  numLayersF = lerp((float)MaxLayers, (float)MinLayers, t);
//     int    NumLayers  = (int)clamp(round(numLayersF), (float)MinLayers, (float)MaxLayers);
//     float  LayerDepth = rcp((float)NumLayers);
//
//     // 4) Parallax 방향 (V.xy / |V.z|) + 규약에 따른 X 반전
//     float2 ParallaxDir = V.xy * rcp(AbsVz);
//     ParallaxDir.x *= FlipX;
//
//     // 5) 레이어당 UV 이동량
//     float2 DeltaUV = ParallaxDir * (HeightScale * LayerDepth);
//
//     // 6) 레이 마칭(교차 전/후 두 점 확보)
//     float2 CurrentUV         = UV;
//     float  CurrentLayerDepth = 0.0;
//     // LOD 안정화를 위해 화면 미분 사용 (가능하면 SampleGrad)
//     float2 ddxUV = ddx(UV);
//     float2 ddyUV = ddy(UV);
//     float  CurrentH = BumpTexture.SampleGrad(SamplerWrap, CurrentUV, ddxUV, ddyUV).r;
//
//     [loop]
//     for (int i = 0; i < NumLayers; ++i)
//     {
//         float FlipH = 1.0f - CurrentH; // 높이맵은 흰색=높음, 검정색=낮음
//         if (CurrentLayerDepth >= FlipH)
//         {
//             break;
//         }
//         
//         // if (CurrentLayerDepth >= CurrentH)
//         // {
//         //     break;
//         // }
//         
//         CurrentUV += DeltaUV;
//         // CurrentUV -= DeltaUV;
//         CurrentH   = BumpTexture.SampleGrad(SamplerWrap, CurrentUV, ddxUV, ddyUV).r;
//         CurrentLayerDepth += LayerDepth;
//     }
//
//     // 7) 이분 탐색(정밀화)
//     float2 PrevUV = CurrentUV + DeltaUV;     // 직전 지점
//     float  dA = CurrentLayerDepth - LayerDepth;
//     float  dB = CurrentLayerDepth;
//
//     float2 A = PrevUV;
//     float2 B = CurrentUV;
//
//     [unroll]
//     for (int k = 0; k < BinarySearchStep; ++k)
//     {
//         float2 M  = 0.5 * (A + B);
//         float  dM = 0.5 * (dA + dB);
//         float  hM = BumpTexture.SampleGrad(SamplerWrap, M, ddxUV, ddyUV).r;
//
//         // h > d → 표면 위(더 깊이) → A=M
//         // h <= d → 이미 지난 쪽 → B=M
//         if (hM > dM) { A = M; dA = dM; }
//         else         { B = M; dB = dM; }
//     }
//
//     float2 FinalUV = 0.5 * (A + B);
//
//     // 8) 아틀라스/타일 경계 클램프
//     FinalUV = ClampToRect(FinalUV, AtlasMin, AtlasMax);
//
//     // 9) 원래 UV 기준 "반대편으로 넘어가지 않게" 보정
//     FinalUV.x = ClampOneSidedFromBase(UV.x, FinalUV.x);
//     FinalUV.y = ClampOneSidedFromBase(UV.y, FinalUV.y);
//
//     // 10) 과도 이동 가드(안정장치)
//     float2 dUV = abs(FinalUV - UV);
//     if (dUV.x > UvShiftClamp || dUV.y > UvShiftClamp)
//         FinalUV = UV;
//
//     return FinalUV;
// }

// #endif

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

//==================================================//
//============Light Segment Functions ===============//
//==================================================//
float3 GetMaterialProperty(uint TextureFlag,Texture2D Texture, float2 UV, float3 DefaultValue)
{
    if (MaterialFlags & TextureFlag)
    {
        return Texture.Sample(SamplerWrap, UV).rgb;
    }
    else if (TextureFlag!=HAS_SPECULAR_MAP && MaterialFlags & HAS_DIFFUSE_MAP)
    {
        return DiffuseTexture.Sample(SamplerWrap, UV).rgb;
    }
    else if (DefaultValue.r == 0.0 && DefaultValue.b == 0.0 && DefaultValue.r == 0.0)
    {
        return float3(1.0f, 1.0f, 1.0f);
    }
    else
    {
        return DefaultValue.rgb;
    }
}

//==================================================//
//============main Functions =======================//
//==================================================//
PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
    Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
    
    // Do NOT normalize here - let GPU interpolate, then normalize in PS
    Output.WorldNormal = mul(Input.Normal, (float3x3)WorldInverseTranspose);

    // Transform tangent to world space using inverse transpose
    Output.WorldTangent = mul(Input.Tangent, (float3x3)WorldInverseTranspose);

    // Transform bitangent to world space using inverse transpose
    Output.WorldBitangent = mul(Input.Bitangent, (float3x3)WorldInverseTranspose);

    Output.Tex = Input.Tex;
    Output.Ambient = float3(1.0f, 1.0f, 1.0f);
    Output.Diffuse = float3(1.0f, 1.0f, 1.0f);
    Output.Specular = float3(1.0f, 1.0f, 1.0f);

#if LIGHTING_MODEL_GOURAUD
    // Gouraud: 모든 라이트를 순회 (Tiled Culling 사용 안 함)
    float3 ViewDir = normalize(ViewWorldLocation - Output.WorldPosition);
    float Shininess = (Ns == 0)? 1.0f : Ns;
    
    FLightSegment LightColor = CalculateAllLights(Output.WorldPosition, Output.WorldNormal, ViewDir, Shininess);

    Output.Ambient = LightColor.Ambient;
    Output.Diffuse = LightColor.Diffuse;
    Output.Specular = LightColor.Specular;    
#endif
    
    return Output;
}

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;
    
    float3 ViewDir = normalize(ViewWorldLocation - Input.WorldPosition);

    // ===== UV Setup (POM: Parallax Occlusion Mapping 적용) =====
    float2 UV = Input.Tex;

#ifdef HAS_NORMAL_MAP
    // POM: Parallax Occlusion Mapping 적용
    if (MaterialFlags & HAS_BUMP_MAP)
    {
        float3 ViewDirTangent = WorldToTangentSpace(
            ViewDir,
            Input.WorldNormal,
            Input.WorldTangent,
            Input.WorldBitangent
        );

        UV = ParallaxOcclusionMapping(UV, ViewDirTangent, HeightScale);
    }
#endif
    
    // ===== Normal Setup =====
    float3 Normal = CalculateWorldNormal(Input, UV);
    float3 EncodedNormal = Normal * 0.5f + 0.5f; /* Encode world normal to [0,1] range for storage */
    Output.NormalData = float4(EncodedNormal, 1.0f);

    // ===== Material Property Setup =====
    float3 MaterialAmbient = GetMaterialProperty(HAS_AMBIENT_MAP, AmbientTexture, UV, Ka);
    float3 MaterialDiffuse = GetMaterialProperty(HAS_DIFFUSE_MAP, DiffuseTexture, UV, Kd);
    float3 MaterialSpecular = GetMaterialProperty(HAS_SPECULAR_MAP, SpecularTexture, UV, Ks);
    
    float Shininess = (Ns == 0)? 1.0f : Ns;
    
    // ===== Alpha Setup =====
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float Alpha = AlphaTexture.Sample(SamplerWrap, UV).w;
        Output.SceneColor.a = D * Alpha;
    }
    else if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        float Alpha = DiffuseTexture.Sample(SamplerWrap, UV).w;
        Output.SceneColor.a = D * Alpha;
    }

#if LIGHTING_MODEL_GOURAUD
    float3 AccumulatedLightWithMaterial = float3(0, 0, 0);
    AccumulatedLightWithMaterial += Input.Ambient * MaterialAmbient;
    AccumulatedLightWithMaterial += Input.Diffuse * MaterialDiffuse;
    AccumulatedLightWithMaterial += Input.Specular * MaterialSpecular;
    Output.SceneColor.rgb = AccumulatedLightWithMaterial;
#elif LIGHTING_MODEL_UNLIT
    /* TODO : 원래는 Albedo로 해야하지만, 현재는 DiffuseTexture = Albedo임 */
    Output.SceneColor= float4(0.5, 0.5, 0.5, 1);
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        float4 TexColor = DiffuseTexture.Sample(SamplerWrap, UV);
        Output.SceneColor= TexColor;
    };
#else
    FLightSegment LightColor;
    
    LightColor = CalculateTiledLighting(Input.Position, Input.WorldPosition, Normal, ViewDir, Shininess,
        ViewportOffset, ViewportSize);
    
    float3 AccumulatedLightWithMaterial = float3(0, 0, 0);
    AccumulatedLightWithMaterial += LightColor.Ambient * MaterialAmbient;
    AccumulatedLightWithMaterial += LightColor.Diffuse * MaterialDiffuse;
    AccumulatedLightWithMaterial += LightColor.Specular * MaterialSpecular;
    AccumulatedLightWithMaterial = saturate(AccumulatedLightWithMaterial);
    Output.SceneColor.rgb = AccumulatedLightWithMaterial;
#endif
    
    return Output;
};
