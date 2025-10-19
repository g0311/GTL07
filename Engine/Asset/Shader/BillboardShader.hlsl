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
    float2 Tex : TEXCOORD0;
};

Texture2D DiffuseTexture : register(t0); // map_Kd
SamplerState SamplerWrap : register(s0);

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    
    Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
    Output.Tex = Input.Tex;
    
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float4 DiffuseColor = DiffuseTexture.Sample(SamplerWrap, Input.Tex);
    return DiffuseColor;
}