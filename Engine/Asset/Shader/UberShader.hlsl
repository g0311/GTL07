#if !defined(UNLIT) && !defined(LIGHTING_MODEL_GOURAUD) && !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG) && !defined(LIGHTING_MODEL_BLINNPHONG)
#define LIGHTING_MODEL_PHONG 1
#endif

#if (defined(UNLIT) + defined(LIGHTING_MODEL_GOURAUD) + defined(LIGHTING_MODEL_LAMBERT) + defined(LIGHTING_MODEL_PHONG)) + defined(LIGHTING_MODEL_BLINNPHONG)!= 1
#error "Exactly one of LIGHTING_MODEL_* must be defined."
#endif

// Light Culling에서 공유하는 헤더 파일 내용
// LightCulling.hlsl의 유틸리티 함수들을 포함
#define TILE_SIZE 32

// 스크린 픽셀 좌표에서 타일 인덱스 계산 (전역 화면 기준)
uint2 GetTileIndexFromPixel(float2 screenPos)
{
    return uint2(floor(screenPos.x / TILE_SIZE), floor(screenPos.y / TILE_SIZE));
}

// 스크린 픽셀 좌표에서 타일 인덱스 계산 (뷰포트 기준)
uint2 GetTileIndexFromPixelViewport(float2 screenPos, uint2 viewportOffset)
{
    float2 viewportRelativePos = screenPos - viewportOffset;
    return uint2(floor(viewportRelativePos.x / TILE_SIZE), floor(viewportRelativePos.y / TILE_SIZE));
}

// SV_Position에서 뷰포트 기준 타일 인덱스 추출 (픽셀 셰이더용)
uint2 GetCurrentTileIndex(float4 svPosition, uint2 viewportOffset)
{
    return GetTileIndexFromPixelViewport(svPosition.xy, viewportOffset);
}

// SV_Position에서 뷰포트 기준 타일 배열 인덱스 추출
uint GetCurrentTileArrayIndex(float4 svPosition, uint2 viewportOffset, uint2 viewportSize)
{
    uint2 viewportTileIndex = GetCurrentTileIndex(svPosition, viewportOffset);
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

// 광원 타입 정의 (LightCulling.hlsl과 동일)
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_AMBIENT 1
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
};

cbuffer Lighting : register(b3)
{
    int NumAmbientLights;
    float3 _pad0;
    FAmbientLightInfo AmbientLights[NUM_AMBIENT_LIGHT];
    
    int HasDirectionalLight;
    float3 _pad1;
    FDirectionalLightInfo DirectionalLight;
    
    int NumPointLights;
    float3 _pad2;
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    
    int NumSpotLights;
    float3 _pad3;
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT];
};

// Light Culling시스템에서 사용하는 Structured Buffer
StructuredBuffer<Light> AllLights : register(t13);             // 라이트 데이터 버퍼
StructuredBuffer<uint> LightIndexBuffer : register(t14);       // 타일별 라이트 인덱스
StructuredBuffer<uint2> TileLightInfo : register(t15);         // 타일 라이트 정보 (offset, count)
// 레지스터 번호 뒤에서 시작 => 기존 셰이더에서 인클루드 해도 문제 X

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

float4 CalculateAmbientLight(float2 UV)
{
    float4 AmbientColor;
    // Material Ambient(Albedo)
    /*TODO : Apply DiffuseTexture for now, due to Binding AmbientTexture feature don't exist yet*/
    if (MaterialFlags & HAS_AMBIENT_MAP)
    {
        AmbientColor = AmbientTexture.Sample(SamplerWrap, UV);
    }
    else if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        AmbientColor = DiffuseTexture.Sample(SamplerWrap, UV);
    }
    else
    {
        AmbientColor = Ka;
    }
    // Light Ambient
    float4 AccumulatedAmbientColor = float4(0, 0, 0, 0);
    for (int i = 0; i < NumAmbientLights; i++)
    {
        // Light * Intensity
        AccumulatedAmbientColor += AmbientLights[i].Color * AmbientLights[i].Intensity;
    }
    return AmbientColor * AccumulatedAmbientColor;
}
float3 CalculateDirectionalLight(float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    if (HasDirectionalLight <= 0)
        return float3(0, 0, 0);

    float3 LightDir = normalize(-DirectionalLight.Direction);
    float3 LightColor = DirectionalLight.Color * DirectionalLight.Intensity;
    
    // Diffuse
    float NdotL = saturate(dot(WorldNormal, LightDir));
    float3 Diffuse = Kd * LightColor * NdotL;
    
    // Specular
    float3 Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, Ks, LightColor, Shininess);
    
    return Diffuse + Specular;
}
float3 CalculatePointLights(float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 AccumulatedPointlightColor = 0;
    for (int i = 0; i < NumPointLights; i++)
    {
        float3 LightVec = PointLights[i].Position - WorldPos;
        float Distance = length(LightVec);

        if (Distance > PointLights[i].Radius)
            continue;
        
        float3 LightDir = LightVec / Distance;

        float RangeAttenuation = saturate(1.0 - Distance/PointLights[i].Radius);
        RangeAttenuation = pow(RangeAttenuation, max(PointLights[i].FalloffExtent, 0.0));
        
        // Light * Intensity
        float3 PointlightColor = PointLights[i].Color.rgb * PointLights[i].Intensity * RangeAttenuation;

        // Diffuse
        float NdotL = saturate(dot(normalize(WorldNormal), LightDir));
        float3 Diffuse = Kd * PointlightColor * NdotL; // Kd is Albedo
        
        // Specular
        float3 Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, Ks, PointlightColor, Shininess);
        AccumulatedPointlightColor += Diffuse + Specular;
    }
    return AccumulatedPointlightColor;
}
float3 CalculateSpotLights(float3 WorldPos, float3 WorldNormal, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 AccumulatedSpotlightColor = 0;
    for (int i = 0; i < NumSpotLights; i++)
    {
        float3 LightVec = SpotLights[i].Position - WorldPos;
        float Distance = length(LightVec);
        float3 LightDir = LightVec / Distance;

        // Attenuation : Range & Cone(Cos)
        float RangeAttenuation = saturate(1.0 - Distance * Distance * SpotLights[i].InvRange2);

        // SpotLight Cone Attenuation
        // SpotLightCos: angle between light direction and pixel direction
        float SpotLightCos = dot(-LightDir, normalize(SpotLights[i].Direction));

        // ConeWidth: CosInner > CosOuter (because InnerAngle < OuterAngle, cos is decreasing)
        float ConeWidth = max(SpotLights[i].CosInner - SpotLights[i].CosOuter, 1e-5);

        // SpotRatio: 0 when outside CosOuter, 1 when inside CosInner
        float SpotRatio = saturate((SpotLightCos - SpotLights[i].CosOuter) / ConeWidth);
        if (SpotRatio!=0)
        {
            SpotRatio = pow(SpotRatio, max(SpotLights[i].Falloff, 0.0)); // Falloff increase, light between outer~inner spread out 
        }
        
        // Light * Intensity
        float3 SpotlightColor = SpotLights[i].Color.rgb * SpotLights[i].Intensity * RangeAttenuation * SpotRatio;

        // Diffuse
        float NdotL = saturate(dot(WorldNormal, LightDir));
        float3 Diffuse = Kd * SpotlightColor * NdotL; // Kd is Albedo

        // Specular
        float3 Specular = CalculateSpecular(LightDir, WorldNormal, ViewDir, Ks, SpotlightColor, Shininess);
        AccumulatedSpotlightColor += Diffuse + Specular;
    }

    return AccumulatedSpotlightColor;
}
float3 CalculateWorldNormal(PS_INPUT Input)
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
    
    // Sample normal map (tangent space)
    if (MaterialFlags & HAS_NORMAL_MAP)
    {
        float3 TangentNormal = NormalTexture.Sample(SamplerWrap, Input.Tex).rgb;
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
#if LIGHTING_MODEL_GOURAUD
    float3 Normal = normalize(Output.WorldNormal);
    float3 ViewDir = normalize(ViewWorldLocation - Output.WorldPosition);

    // Vertex Shader에서는 텍스처 샘플링 불가 - Material 상수만 사용
    float3 kD = Kd.rgb;
    float3 kS = Ks.rgb;

    // Ambient Light 계산
    float3 MaterialAmbient = Ka.rgb;
    float3 AccumulatedAmbientColor = float3(0, 0, 0);
    for (int i = 0; i < NumAmbientLights; i++)
    {
        AccumulatedAmbientColor += AmbientLights[i].Color.rgb * AmbientLights[i].Intensity;
    }
    float3 AmbientColor = MaterialAmbient * AccumulatedAmbientColor;

    float3 DirectionalColor = CalculateDirectionalLight(Normal, ViewDir, kD, kS, Ns);
    float3 PointLightColor = CalculatePointLights(Output.WorldPosition, Normal, ViewDir, kD, kS, Ns);
    float3 SpotLightColor = CalculateSpotLights(Output.WorldPosition, Normal, ViewDir, kD, kS, Ns);

    Output.Color.rgb = AmbientColor + DirectionalColor + PointLightColor + SpotLightColor;
    Output.Color.a = 1.0f;
#endif
    return Output;
}

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;
#if LIGHTING_MODEL_GOURAUD
    Output.SceneColor = Input.Color;
    // 텍스처가 있다면 곱하기
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        
        float4 TexColor = DiffuseTexture.Sample(SamplerWrap, Input.Tex);
        Output.SceneColor.rgb *= TexColor.rgb;
        Output.SceneColor.a = TexColor.a;
    };
#else
    float2 UV = Input.Tex;
    float3 Normal  = normalize(Input.WorldNormal);
    float3 ViewDir  = normalize(ViewWorldLocation - Input.WorldPosition);
     
    float3 kD = (MaterialFlags & HAS_DIFFUSE_MAP) ? DiffuseTexture.Sample(SamplerWrap, UV).rgb : Kd;
    float3 kS = (MaterialFlags & HAS_SPECULAR_MAP) ? SpecularTexture.Sample(SamplerWrap, UV).rgb : Ks;
     
    // Unlit 처리
    if (MaterialFlags & UNLIT)
    {
        Output.SceneColor = float4(kD, 1.0);
        Output.NormalData = float4(Normal * 0.5f + 0.5f, 1.0);
        return Output;
    }
     
    // Light
    float3 AmbientColor = CalculateAmbientLight(UV).rgb;
    float3 DirectionalColor = CalculateDirectionalLight(Normal, ViewDir, kD, kS, Ns);
    float3 PointLightColor = CalculatePointLights(Input.WorldPosition, Normal, ViewDir, kD, kS, Ns);
    float3 SpotLightColor = CalculateSpotLights(Input.WorldPosition, Normal, ViewDir, kD, kS, Ns);
         
    float4 FinalColor;
    FinalColor.rgb = AmbientColor + DirectionalColor + PointLightColor + SpotLightColor;
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
    
    // Normal
    // Calculate World Normal for Normal Buffer
    float3 WorldNormal = CalculateWorldNormal(Input);
    
    // Encode world normal to [0,1] range for storage
    float3 EncodedNormal = WorldNormal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
};