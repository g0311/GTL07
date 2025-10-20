// CommonBuffers.hlsli 
// : Common constant buffer structures for shaders.

#ifndef COMMON_BUFFERS_HLSLI
#define COMMON_BUFFERS_HLSLI

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

#endif  // COMMON_BUFFERS_HLSLI