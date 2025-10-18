#include "TextureVS.hlsl"

#define NUM_POINT_LIGHT 8
#define NUM_SPOT_LIGHT 8

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

cbuffer MaterialConstants : register(b2) // b0, b1 is in VS
{
    float4 Ka;		// Ambient color
    float4 Kd;		// Diffuse color
    float4 Ks;		// Specular color
    float Ns;		// Specular exponent
    float Ni;		// Index of refraction
    float D;		// Dissolve factor
    uint MaterialFlags;
    float Time;
};

cbuffer Lighting : register(b3)
{
    int NumAmbientLights;
    float3 _pad0;
    FAmbientLightInfo AmbientLights[8];
    
    int HasDirectionalLight;
    float3 _pad2;
    FDirectionalLightInfo DirectionalLight;
    
    int NumPointLights;
    float3 _pad3;
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    
    int NumSpotLights;
    float3 _pad1;
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT];
};

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

float4 CalculateAmbientFactor(float2 UV)
{
    float4 AmbientColor;
    // Material Ambient(Albedo)
    /*TODO : Apply DiffuseTexture for now, due to Binding AmbientTexture feature don't exist yet*/
    // if (MaterialFlags & HAS_AMBIENT_MAP)
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        // AmbientColor = AmbientTexture.Sample(SamplerWrap, UV);
        AmbientColor = DiffuseTexture.Sample(SamplerWrap, UV);
    }
    else
    {
        AmbientColor = Ka;
    }
    // Light Ambient
    float4 AccumulatedAmbientColor = 0;
    for (int i = 0; i < NumAmbientLights; i++)
    {
        // Light * Intensity
        AccumulatedAmbientColor += AmbientLights[i].Color * AmbientLights[i].Intensity;
    }
    AmbientColor *= AccumulatedAmbientColor;
    return AmbientColor;
}
float3 CalculateSpotlightFactors(float3 Position, float3 Norm, float3 ViewDir, float3 Kd, float3 Ks, float Shininess)
{
    float3 AccumulatedSpotlightColor = 0;
    for (int i = 0; i < NumSpotLights; i++)
    {
        float3 LightVec = SpotLights[i].Position - Position;
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
        SpotRatio = pow(SpotRatio, max(SpotLights[i].Falloff, 0.0));

        /* TODO : Blinn Phong for Now */
        // Light * Intensity
        float3 SpotlightColor = SpotLights[i].Color.rgb * SpotLights[i].Intensity * RangeAttenuation * SpotRatio;

        // Diffuse
        float NdotL = saturate(dot(Norm, LightDir));
        float3 Diffuse = Kd * SpotlightColor * NdotL;
        
        // Specular
        float3 H = normalize(LightDir + ViewDir);
        float  NdotH = saturate(dot(Norm, H));
        float3 Specular = Ks * SpotlightColor * pow(NdotH, Shininess);

        AccumulatedSpotlightColor += Diffuse + Specular;
    }

    return AccumulatedSpotlightColor;
}

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;
    float2 UV = Input.Tex;
    float3 Norm  = normalize(Input.WorldNormal);
    float3 ViewDir  = normalize(ViewWorldLocation - Input.WorldPosition);

    float3 kD = (MaterialFlags & HAS_DIFFUSE_MAP) ? DiffuseTexture.Sample(SamplerWrap, UV).rgb : Kd;
    float3 kS = (MaterialFlags & HAS_SPECULAR_MAP) ? SpecularTexture.Sample(SamplerWrap, UV).rgb : Ks;

    float3 Diffuse;
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        // AmbientColor = AmbientTexture.Sample(SamplerWrap, UV);
        Diffuse = DiffuseTexture.Sample(SamplerWrap, UV);
    }
    
    // Ambient contribution
    float3 AmbientColor = CalculateAmbientFactor(UV).rgb;
    
    // Spotlight contribution
    float3 SpotlightColor = CalculateSpotlightFactors(Input.WorldPosition, Norm, ViewDir, kD, kS, Ns);
    
    // Directional Light
    float3 DirectLighting = float3(0.f, 0.f, 0.f);
    if (HasDirectionalLight > 0)
    {
        float3 normal = normalize(Input.WorldNormal);
        float3 lightDir = normalize(-DirectionalLight.Direction);
        float NdotL = saturate(dot(normal, lightDir));
        DirectLighting += DirectionalLight.Color * DirectionalLight.Intensity * NdotL;
    }
    
    // Point Lights
    float3 PointLighting = float3(0.f, 0.f, 0.f);
    for (int i = 0; i < NumPointLights; i++)
    {
        float3 toLight = PointLights[i].Position - Input.WorldPosition;
        float dist = length(toLight);
        
        if (dist < PointLights[i].Radius)
        {
            float3 lightDir = toLight / dist;
            float3 normal = normalize(Input.WorldNormal);
            float NdotL = saturate(dot(normal, lightDir));
            float auttenuation = pow(1.0f - (dist / PointLights[i].Radius), PointLights[i].FalloffExtent);
            
            PointLighting += PointLights[i].Color * PointLights[i].Intensity * NdotL * auttenuation;
        }
    }
    // Final Color
    float4 FinalColor;
    FinalColor.rgb = AmbientColor + Diffuse * (SpotlightColor + DirectLighting + PointLighting);
    FinalColor.a = 1.0f;
    Output.SceneColor = FinalColor;
    
    // Alpha handling
    // /*TODO : Apply DiffuseTexture for now, due to Binding AlphaTexture feature don't exist yet*/
    // if (MaterialFlags & HAS_DIFFUSE_MAP)
    // {
    //     float alpha = DiffuseTexture.Sample(SamplerWrap, UV).w;
    //     FinalColor.a = D * alpha;
    // }


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

    if (MaterialFlags & HAS_NORMAL_MAP)
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

    // Encode world normal to [0,1] range for storage
    float3 EncodedNormal = WorldNormal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
};
