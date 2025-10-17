/**
 * @file LightCulling.hlsl
 * @brief Tile-based light culling compute shader.
 */

#define TILE_SIZE 16

// Matches the PointLight struct in the engine
struct PointLight
{
    float4 position; // w for radius
    float4 color;    // w for intensity
};

// Input buffers
StructuredBuffer<PointLight> AllLights;

cbuffer CullingParams : register(b0)
{
    matrix View;
    matrix Projection;
    uint2 ScreenDimensions;    // 전체 화면 크기
    uint2 ViewportOffset;      // 뷰포트 시작 위치 (x, y)
    uint2 ViewportSize;        // 뷰포트 크기 (width, height)
    uint NumLights;
    uint3 Padding;
};

// Output UAVs
RWStructuredBuffer<uint> LightIndexBuffer; // Global list of visible light indices for all tiles
RWStructuredBuffer<uint2> TileLightInfo;   // For each tile: x = offset in LightIndexBuffer, y = number of lights

// Thread group shared memory to store indices for one tile
groupshared uint visibleLightIndices[1024];
groupshared uint numVisibleLights;

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex)
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

    // Reset shared memory counter (only one thread in the group needs to do this)
    if (GI == 0)
    {
        numVisibleLights = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // TODO: Step 1: Construct frustum for this tile (Gid.xy) using Projection matrix and screen space coordinates.
    // 뷰포트 오프셋을 고려한 좌표 계산 필요
    // Since we are not using a depth buffer, the frustum will be from near to far plane.

    // TODO: Step 2: Loop through lights and perform sphere-frustum intersection tests.
    // Each thread in the group can check a subset of lights.
    // If a light is visible, add its index to `visibleLightIndices` using InterlockedAdd on `numVisibleLights`.


    // TODO: Step 3: Write the results to the global UAVs.
    // One thread should atomically reserve space in `LightIndexBuffer` and then all threads can cooperate
    // to write the `visibleLightIndices` to the reserved space.
    // The tile's offset and count are written to `TileLightInfo`.
}
