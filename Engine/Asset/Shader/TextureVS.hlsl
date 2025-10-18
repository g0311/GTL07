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
};


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

	return Output;
}
