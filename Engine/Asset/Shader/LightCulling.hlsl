/**
 * @file LightCulling.hlsl
 * @brief Tile-based light culling compute shader.
 */

#define TILE_SIZE 32

// Light types
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT 1

// Universal light struct for both point and spot lights
struct Light
{
    float4 position;    // xyz: world position, w: radius
    float4 color;       // xyz: color, w: intensity
    float4 direction;   // xyz: direction (for spot), w: light type (0=point, 1=spot)
    float4 angles;      // x: inner cone angle (cos), y: outer cone angle (cos), z,w: unused
};

StructuredBuffer<Light> AllLights;

cbuffer CullingParams : register(b0)
{
    matrix View;                 // 64 bytes (4x4 float matrix)
    matrix Projection;           // 64 bytes (4x4 float matrix)
    uint2 ScreenDimensions;      // 8 bytes
    uint2 ViewportOffset;        // 8 bytes  
    uint2 ViewportSize;          // 8 bytes
    uint NumLights;              // 4 bytes
    uint Padding;                // 4 bytes
};

// Output UAVs
RWStructuredBuffer<uint> LightIndexBuffer; // Global list of visible light indices for all tiles
RWStructuredBuffer<uint2> TileLightInfo;   // For each tile: x = offset in LightIndexBuffer, y = number of lights

// Thread group shared memory to store indices for one tile
groupshared uint visibleLightIndices[1024];
groupshared uint numVisibleLights;

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 Tid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    // 뷰포트 영역을 벗어난 타일은 처리하지 않음
    uint2 tilePos = Gid.xy;
    uint2 tileMinBound = tilePos * TILE_SIZE;
    uint2 tileMaxBound = tileMinBound + TILE_SIZE;
    
    // 뷰포트 영역을 벗어난 타일인지 확인
    if (tileMinBound.x >= ViewportOffset.x + ViewportSize.x || 
        tileMinBound.y >= ViewportOffset.y + ViewportSize.y ||
        tileMaxBound.x <= ViewportOffset.x ||
        tileMaxBound.y <= ViewportOffset.y)
    {
        return; // 이 타일은 뷰포트 영역에 포함되지 않음
    }

    // Reset shared memory counter
    if (GI == 0)
    {
        numVisibleLights = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // Step 1: 현재 타일 프러스텀 계산
    // 뷰포트 오프셋을 고려한 스크린 좌표 계산
    uint2 actualTileMin = max(tileMinBound, ViewportOffset);
    uint2 actualTileMax = min(tileMaxBound, ViewportOffset + ViewportSize);
    
    // NDC 좌표로 변환 (-1 to 1)
    float2 tileMinNDC = float2(
        (2.0f * actualTileMin.x) / ScreenDimensions.x - 1.0f,
        1.0f - (2.0f * actualTileMin.y) / ScreenDimensions.y
    );
    float2 tileMaxNDC = float2(
        (2.0f * actualTileMax.x) / ScreenDimensions.x - 1.0f,
        1.0f - (2.0f * actualTileMax.y) / ScreenDimensions.y
    );
    
    // 프러스텀 평면 계산 (뷰 스페이스에서)
    matrix invProj = transpose(Projection); // 역행렬 근사치 (단순화를 위해)
    
    // 타일의 4개 모서리 좌표를 뷰 스페이스로 변환
    float3 frustumCorners[4];
    frustumCorners[0] = float3(tileMinNDC.x, tileMinNDC.y, 1.0f); // Near top-left
    frustumCorners[1] = float3(tileMaxNDC.x, tileMinNDC.y, 1.0f); // Near top-right
    frustumCorners[2] = float3(tileMinNDC.x, tileMaxNDC.y, 1.0f); // Near bottom-left
    frustumCorners[3] = float3(tileMaxNDC.x, tileMaxNDC.y, 1.0f); // Near bottom-right
    
    // 프러스텀 평면들 계산 (Left, Right, Top, Bottom)
    float4 frustumPlanes[4];
    
    // Left plane: 왼쪽 모서리에서
    float3 leftNormal = cross(frustumCorners[0], frustumCorners[2]);
    leftNormal = normalize(leftNormal);
    frustumPlanes[0] = float4(leftNormal, 0);
    
    // Right plane: 오른쪽 모서리에서
    float3 rightNormal = cross(frustumCorners[3], frustumCorners[1]);
    rightNormal = normalize(rightNormal);
    frustumPlanes[1] = float4(rightNormal, 0);
    
    // Top plane: 위쪽 모서리에서
    float3 topNormal = cross(frustumCorners[1], frustumCorners[0]);
    topNormal = normalize(topNormal);
    frustumPlanes[2] = float4(topNormal, 0);
    
    // Bottom plane: 아래쪽 모서리에서
    float3 bottomNormal = cross(frustumCorners[2], frustumCorners[3]);
    bottomNormal = normalize(bottomNormal);
    frustumPlanes[3] = float4(bottomNormal, 0);
    
    // Step 2: 프러스텀과 라이트 교차 여부 검사
    const uint threadsPerGroup = TILE_SIZE * TILE_SIZE;
    const uint lightsPerThread = (NumLights + threadsPerGroup - 1) / threadsPerGroup;
    const uint startLightIndex = GI * lightsPerThread;
    const uint endLightIndex = min(startLightIndex + lightsPerThread, NumLights);
    
    // 각 스레드가 담당할 라이트들을 검사
    for (uint lightIndex = startLightIndex; lightIndex < endLightIndex; ++lightIndex)
    {
        Light light = AllLights[lightIndex];
        
        // 라이트를 뷰 스페이스로 변환
        float4 lightPosView = mul(View, float4(light.position.xyz, 1.0));
        float lightRadius = light.position.w;
        uint lightType = (uint)light.direction.w;
        
        bool isVisible = true;
        
        if (lightType == LIGHT_TYPE_POINT)
        {
            // Point Light: Sphere-Frustum intersection test
            [unroll]
            for (uint planeIndex = 0; planeIndex < 4; ++planeIndex)
            {
                float distance = dot(frustumPlanes[planeIndex].xyz, lightPosView.xyz) + frustumPlanes[planeIndex].w;
                if (distance < -lightRadius)
                {
                    isVisible = false;
                    break;
                }
            }
        }
        else if (lightType == LIGHT_TYPE_SPOT)
        {
            // SpotLight: Cone-Frustum intersection test
            float3 lightDirView = normalize(mul(View, float4(light.direction.xyz, 0.0)).xyz);
            float cosOuterAngle = light.angles.y;
            
            // 스포몇 라이트 컨과 타일 프러스텀 교차 검사
            // 단순화된 방법: 컨의 바운딩 스피어로 근사
            float coneHeight = lightRadius / cosOuterAngle;
            float3 coneEnd = lightPosView.xyz + lightDirView * coneHeight;
            
            // 컨의 바운딩 스피어로 근사하여 검사
            float3 sphereCenter = (lightPosView.xyz + coneEnd) * 0.5;
            float sphereRadius = length(lightPosView.xyz - coneEnd) * 0.5 + lightRadius * 0.5;
            
            [unroll]
            for (uint planeIndex = 0; planeIndex < 4; ++planeIndex)
            {
                float distance = dot(frustumPlanes[planeIndex].xyz, sphereCenter) + frustumPlanes[planeIndex].w;
                if (distance < -sphereRadius)
                {
                    isVisible = false;
                    break;
                }
            }
            
            // 추가 검사: 컨 방향과 타일 중심 간의 각도 검사
            if (isVisible)
            {
                // 타일 중심에서 라이트로의 벡터
                float2 tileCenter = (actualTileMin + actualTileMax) * 0.5;
                float2 tileCenterNDC = float2(
                    (2.0f * tileCenter.x) / ScreenDimensions.x - 1.0f,
                    1.0f - (2.0f * tileCenter.y) / ScreenDimensions.y
                );
                float3 tileCenterView = float3(tileCenterNDC.x, tileCenterNDC.y, 1.0f); // 근사치
                
                float3 lightToTile = normalize(tileCenterView - lightPosView.xyz);
                float dotProduct = dot(lightDirView, lightToTile);
                
                // 컨 범위를 벗어난 경우
                if (dotProduct < cosOuterAngle)
                {
                    isVisible = false;
                }
            }
        }
        
        // Z 범위 검사 (기본적인 near/far plane 처리)
        if (isVisible)
        {
            if (lightPosView.z + lightRadius < 0.1f || lightPosView.z - lightRadius > 1000.0f)
            {
                isVisible = false;
            }
        }
        
        // 보이는 라이트를 shared memory에 추가
        if (isVisible)
        {
            uint index;
            InterlockedAdd(numVisibleLights, 1, index);
            if (index < 1024)
            {
                visibleLightIndices[index] = lightIndex;
            }
        }
    }
    
    // 모든 스레드가 라이트 검사를 마칠 때까지 대기
    GroupMemoryBarrierWithGroupSync();
    
    // Step 3: UAV에 결과 저장
    if (GI == 0 && numVisibleLights > 0)
    {
        // 전역 버퍼에 공간 예약
        uint globalOffset;
        InterlockedAdd(LightIndexBuffer[0], numVisibleLights, globalOffset);
        globalOffset += 1; // 첫 번째 요소는 카운터용
        
        // 타일 정보 저장 (오프셋과 라이트 개수)
        uint tileIndex = Gid.y * ((ScreenDimensions.x + TILE_SIZE - 1) / TILE_SIZE) + Gid.x;
        TileLightInfo[tileIndex] = uint2(globalOffset, min(numVisibleLights, 1024u));
    }
    
    // 모든 스레드가 보이는 라이트 인덱스를 전역 버퍼에 쓰기
    if (numVisibleLights > 0)
    {
        uint globalOffset = TileLightInfo[Gid.y * ((ScreenDimensions.x + TILE_SIZE - 1) / TILE_SIZE) + Gid.x].x;
        uint actualCount = min(numVisibleLights, 1024u);
        
        for (uint i = GI; i < actualCount; i += threadsPerGroup)
        {
            LightIndexBuffer[globalOffset + i] = visibleLightIndices[i];
        }
    }
}
