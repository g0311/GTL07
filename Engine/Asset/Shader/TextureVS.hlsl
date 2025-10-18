cbuffer Model : register(b0)
{
	row_major float4x4 World;
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

	// Transform normal to world space
	float3 WorldNormal = mul(Input.Normal, (float3x3)World);
	float NormalLength = length(WorldNormal);
	Output.WorldNormal = (NormalLength > 0.0001f) ? (WorldNormal / NormalLength) : float3(0.0f, 0.0f, 1.0f);

	// Transform tangent to world space
	float3 WorldTangent = mul(Input.Tangent, (float3x3)World);
	float TangentLength = length(WorldTangent);
	Output.WorldTangent = (TangentLength > 0.0001f) ? (WorldTangent / TangentLength) : float3(1.0f, 0.0f, 0.0f);

	// Transform bitangent to world space
	float3 WorldBitangent = mul(Input.Bitangent, (float3x3)World);
	float BitangentLength = length(WorldBitangent);
	Output.WorldBitangent = (BitangentLength > 0.0001f) ? (WorldBitangent / BitangentLength) : float3(0.0f, 1.0f, 0.0f);

	Output.Tex = Input.Tex;

	return Output;
}
