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

cbuffer Material : register(b2) // b0, b1 is in VS
{
    float4 Ka;      // Ambient color
    float4 Kd;      // Diffuse color
    float4 Ks;      // Specular color
    float Ns;       // Specular exponent (Shineness)
    float Ni;       // Index of refraction
    float D;        // Dissolve factor
    uint MaterialFlags;
    float Time;
};

#endif  // COMMON_BUFFERS_HLSLI