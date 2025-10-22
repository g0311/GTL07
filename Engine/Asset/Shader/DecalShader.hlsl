#include "LightingCommon.hlsli"

cbuffer DecalConstants : register(b2)
{
	row_major float4x4 DecalWorld;
    row_major float4x4 DecalViewProjection;
	float FadeProgress;
};

Texture2D DecalTexture : register(t0);
SamplerState DecalSampler : register(s0);

Texture2D FadeTexture : register(t1);
SamplerState FadeSampler : register(s1);

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Color : COLOR;
	float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float4 WorldPos : TEXCOORD0;
	float4 Normal : TEXCOORD1;
	float2 Tex : TEXCOORD2;
};

PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;

	float4 Pos = mul(float4(Input.Position, 1.0f), World);
	Output.Position = mul(mul(Pos, View), Projection);
	Output.WorldPos = Pos;
	Output.Normal = normalize(mul(float4(Input.Normal, 0.0f), WorldInverseTranspose));
	Output.Tex = Input.Tex;
	return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
	// +-+-+ Decal Projection & Texture Sampling +-+-+
    float2 DecalUV;
	
	// Decal Local Transition
    float4 DecalLocalPos = mul(Input.WorldPos, DecalViewProjection);
    DecalLocalPos /= DecalLocalPos.w;

	float EPS = 1e-5f;
    if (DecalLocalPos.x < 0.f - EPS ||
    	DecalLocalPos.x > 1.f + EPS ||
    	abs(DecalLocalPos.y) > 1.f + EPS ||
    	abs(DecalLocalPos.z) > 1.f + EPS)
    {
        discard;
    }
	
	//UV Transition
	// ([-0.5~0.5], [-0.5~0.5]) -> ([0~1.0], [1.0~0])
    DecalUV = ((DecalLocalPos.yz) * float2(0.5f, -0.5f) + 0.5f);
    
	float4 DecalColor = DecalTexture.Sample(DecalSampler, DecalUV);

	// +-+-+ Alpha & Fade Handling +-+-+
	float FadeValue = FadeTexture.Sample(FadeSampler, DecalUV).r;
	DecalColor.a *= 1.0f - saturate(FadeProgress / (FadeValue + 1e-6));
	
	if (DecalColor.a < 0.001f) { discard; }
    //return DecalColor;
	
	// +-+-+ Lighting Calculation +-+-+
    float3 N = normalize(Input.Normal.xyz);
    float3 V = normalize(ViewWorldLocation - Input.WorldPos.xyz);
	
    float3 kD = DecalColor.rgb; // Diffuse = decal base color
    float3 kS = float3(0.1f, 0.1f, 0.1f); // Specular
	float3 kA = float3(0.1f, 0.1f, 0.1f); // Ambient
    float Ns = 16.0f; // Roughness
	
    float3 TiledLightColor = CalculateTiledLighting(Input.Position, Input.WorldPos.xyz, N, V, kA, kD, kS, Ns, ViewportOffset, ViewportSize);
	
    float3 finalColor = DecalColor.rgb * TiledLightColor;
    return float4(finalColor, DecalColor.a);
}