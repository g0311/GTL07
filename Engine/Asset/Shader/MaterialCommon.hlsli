// MaterialCommon.hlsli
// : Common texture and sampler declarations for material shaders.

#ifndef MATERIAL_COMMON_HLSLI
#define MATERIAL_COMMON_HLSLI

Texture2D DiffuseTexture : register(t0);    // map_Kd
Texture2D AmbientTexture : register(t1);    // map_Ka
Texture2D SpecularTexture : register(t2);   // map_Ks
Texture2D NormalTexture : register(t3);     // map_Ns
Texture2D AlphaTexture : register(t4);      // map_d
Texture2D BumpTexture : register(t5);       // map_bump

SamplerState SamplerWrap : register(s0);

#endif  // MATERIAL_COMMON_HLSLI