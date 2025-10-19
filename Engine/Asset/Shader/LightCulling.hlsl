/**
 * @file LightCulling.hlsl
 * @brief 타일 기반 광원 컬링 컴퓨트 셰이더
 */

// 타일 한 변의 크기 정의. TILE_SIZE x TILE_SIZE 스레드가 하나의 타일을 처리
#define TILE_SIZE 32

// =================================================================
// 픽셀 셰이더용 타일 유틸리티 함수들
// =================================================================

// 스크린 픽셀 좌표에서 타일 인덱스 계산 (전역 화면 기준)
uint2 GetTileIndexFromPixel(float2 screenPos)
{
    return uint2(floor(screenPos.x / TILE_SIZE), floor(screenPos.y / TILE_SIZE));
}

// 스크린 픽셀 좌표에서 타일 인덱스 계산 (뷰포트 기준)
uint2 GetTileIndexFromPixelViewport(float2 screenPos, uint2 viewportOffset)
{
    float2 viewportRelativePos = screenPos - viewportOffset;
    return uint2(floor(viewportRelativePos.x / TILE_SIZE), floor(viewportRelativePos.y / TILE_SIZE));
}

// 타일 인덱스를 1D 배열 인덱스로 변환 (전역 화면 기준)
uint GetTileArrayIndex(uint2 tileIndex, uint2 screenDimensions)
{
    uint tilesPerRow = (screenDimensions.x + TILE_SIZE - 1) / TILE_SIZE;
    return tileIndex.y * tilesPerRow + tileIndex.x;
}

// SV_Position에서 뷰포트 기준 타일 인덱스 추출 (픽셀 셰이더용)
uint2 GetCurrentTileIndex(float4 svPosition, uint2 viewportOffset)
{
    return GetTileIndexFromPixelViewport(svPosition.xy, viewportOffset);
}

// SV_Position에서 뷰포트 기준 타일 배열 인덱스 추출
uint GetCurrentTileArrayIndex(float4 svPosition, uint2 viewportOffset, uint2 viewportSize)
{
    uint2 viewportTileIndex = GetCurrentTileIndex(svPosition, viewportOffset);
    uint tilesPerRow = (viewportSize.x + TILE_SIZE - 1) / TILE_SIZE;
    return viewportTileIndex.y * tilesPerRow + viewportTileIndex.x;
}

// 타일 내에서의 상대 위치 계산 (0~TILE_SIZE-1)
uint2 GetPixelInTile(float2 screenPos)
{
    return uint2(screenPos.x % TILE_SIZE, screenPos.y % TILE_SIZE);
}

// 타일의 시작 픽셀 좌표 계산
float2 GetTileStartPixel(uint2 tileIndex)
{
    return float2(tileIndex.x * TILE_SIZE, tileIndex.y * TILE_SIZE);
}

// 타일의 끝 픽셀 좌표 계산
float2 GetTileEndPixel(uint2 tileIndex)
{
    return float2((tileIndex.x + 1) * TILE_SIZE - 1, (tileIndex.y + 1) * TILE_SIZE - 1);
}

// 버텍스 셰이더용: 월드 좌표에서 타일 인덱스 계산
uint2 GetTileIndexFromWorldPos(float3 worldPos, matrix viewMatrix, matrix projMatrix, uint2 viewportOffset, uint2 viewportSize)
{
    // 월드 좌표를 뷰 공간으로 변환
    float4 viewPos = mul(viewMatrix, float4(worldPos, 1.0));
    
    // 뷰 공간을 클립 공간으로 변환
    float4 clipPos = mul(projMatrix, viewPos);
    
    // 동차 좌표를 NDC로 변환
    float2 ndc = clipPos.xy / clipPos.w;
    
    // NDC를 뷰포트 기준 스크린 좌표로 변환
    float2 screenPos;
    screenPos.x = (ndc.x + 1.0) * 0.5 * viewportSize.x + viewportOffset.x;
    screenPos.y = (1.0 - ndc.y) * 0.5 * viewportSize.y + viewportOffset.y;
    
    // 스크린 좌표에서 타일 인덱스 계산
    return GetTileIndexFromPixelViewport(screenPos, viewportOffset);
}

// 버텍스 셰이더용: 클립 좌표에서 타일 인덱스 계산
uint2 GetTileIndexFromClipPos(float4 clipPos, uint2 viewportOffset, uint2 viewportSize)
{
    // 동차 좌표를 NDC로 변환
    float2 ndc = clipPos.xy / clipPos.w;
    
    // NDC를 뷰포트 기준 스크린 좌표로 변환
    float2 screenPos;
    screenPos.x = (ndc.x + 1.0) * 0.5 * viewportSize.x + viewportOffset.x;
    screenPos.y = (1.0 - ndc.y) * 0.5 * viewportSize.y + viewportOffset.y;
    
    // 스크린 좌표에서 타일 인덱스 계산
    return GetTileIndexFromPixelViewport(screenPos, viewportOffset);
}

// =================================================================

// 광원 타입 정의
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_AMBIENT 1
#define LIGHT_TYPE_POINT 2
#define LIGHT_TYPE_SPOT 3

// 점 광원과 스포트라이트를 모두 표현하기 위한 범용 광원 구조체
struct Light
{
    float4 position;    // xyz: 월드 위치, w: 영향 반경
    float4 color;       // xyz: 색상, w: 강도
    float4 direction;   // xyz: 방향 (스포트라이트용), w: 광원 타입
    float4 angles;      // x: 내부 원뿔 각도(cos), y: 외부 원뿔 각도(cos), z: falloff extent/falloff, w: InvRange2 (스포트전용)
};

// Z 범위(Near/Far) 컬링을 수행하는 함수
bool IsInZRange(float4 lightPosView, float lightRadius)
{
    // isVisible 기본값을 true로 가정하고, 범위를 벗어날 때만 false를 반환
    if (lightPosView.z + lightRadius < 0.1f || lightPosView.z - lightRadius > 1000.0f)
    {
        return false;
    }
    return true;
}

// 씬에 존재하는 모든 광원의 정보
StructuredBuffer<Light> AllLights;

// 컬링 계산에 필요한 파라미터들을 담는 상수 버퍼
cbuffer CullingParams : register(b0)
{
    matrix View; // 뷰 행렬
    matrix Projection; // 투영 행렬
    uint2 RenderTargetSize; // 전체 렌더 타겟 크기
    uint2 ViewportOffset; // 현재 뷰포트의 시작 오프셋
    uint2 ViewportSize; // 현재 뷰포트의 크기
    uint NumLights; // 씬에 있는 전체 광원 개수
    uint EnableCulling; // Light Culling 활성화 여부 (1=활성화, 0=모든라이트저장)
};

// 출력용 UAV(Unordered Access View) 버퍼
RWStructuredBuffer<uint> LightIndexBuffer; // 타일별로 컬링된 광원 인덱스들이 저장될 전역 버퍼
RWStructuredBuffer<uint2> TileLightInfo;   // 각 타일의 광원 정보 (LightIndexBuffer에서의 시작 오프셋, 광원 개수)

// 스레드 그룹 내에서만 사용하는 공유 메모리. 한 타일에 속한 광원 인덱스들을 임시 저장
groupshared uint visibleLightIndices[1024]; // 최대 1024개의 광원 인덱스 저장
groupshared uint numVisibleLights;          // 현재 타일에서 찾은 광원의 개수

// 컴퓨트 셰이더 메인 함수. TILE_SIZE x TILE_SIZE x 1 개의 스레드가 하나의 그룹으로 실행
[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 Tid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    // --- 초기화  ---
    if (GI == 0)
    {
        numVisibleLights = 0;
    }
    // 그룹 내 모든 스레드가 초기화를 마칠 때까지 대기.
    GroupMemoryBarrierWithGroupSync();

    // --- 1단계: 타일 프러스텀 계산 (뷰 공간 기준) ---
    
    // --- 타일의 네 꼭짓점에 해당하는 뷰 공간 방향 벡터 계산 ---
    //      카메라 원점(뷰 공간의 0,0,0)에서의 3D 방향 벡터를 계산

    // (1) 뷰 공간 계산에 필요한 FOV(시야각) 정보를 투영 행렬로부터 역산
    float tanHalfFovX = 1.0f / Projection[0][0];
    float tanHalfFovY = 1.0f / Projection[1][1];

    // (2) 타일의 뷰포트 내 상대 픽셀 좌표 범위를 계산 (뷰포트 기준)
    // Gid.xy는 뷰포트 내에서의 타일 인덱스이므로 직접 사용
    float2 tileMinPixelPos = Gid.xy * TILE_SIZE;
    float2 tileMaxPixelPos = (Gid.xy + 1) * TILE_SIZE;

    // (3) 뷰포트 내 상대 좌표를 UV(0~1)로 변환 후 NDC(-1 ~ 1 범위)로 변환
    float2 uv_LT = tileMinPixelPos / ViewportSize;
    float2 uv_RB = tileMaxPixelPos / ViewportSize;
    float2 ndc_LT = float2(uv_LT.x * 2.0f - 1.0f, 1.0f - uv_LT.y * 2.0f);
    float2 ndc_RB = float2(uv_RB.x * 2.0f - 1.0f, 1.0f - uv_RB.y * 2.0f);

    // (4) NDC 좌표를 뷰 공간의 방향 벡터로 변환
    //     z=1 평면에 투영된 지점의 좌표를 계산하는 것과 동일
    float view_L = ndc_LT.x * tanHalfFovX; // 왼쪽 X
    float view_R = ndc_RB.x * tanHalfFovX; // 오른쪽 X
    float view_T = ndc_LT.y * tanHalfFovY; // 위쪽 Y
    float view_B = ndc_RB.y * tanHalfFovY; // 아래쪽 Y

    // (5) 최종적으로 뷰 공간의 네 꼭짓점 방향 벡터(c1~c4)를 정의
    //     (이후 평면 계산 순서에 맞춰 c1~c4를 정의)
    // c1: 좌하단 (Left-Bottom)
    // c2: 우하단 (Right-Bottom)
    // c3: 좌상단 (Left-Top)
    // c4: 우상단 (Right-Top)
    float4 c1 = float4(view_L, view_B, 1.0f, 0.0f);
    float4 c2 = float4(view_R, view_B, 1.0f, 0.0f);
    float4 c3 = float4(view_L, view_T, 1.0f, 0.0f);
    float4 c4 = float4(view_R, view_T, 1.0f, 0.0f);
    
    // 타일 프러스텀을 구성하는 4개의 평면 계산 (카메라 원점에서 시작하는 방향 벡터 기반)
    // 각 평면은 원점(0,0,0)과 두 방향 벡터로 정의됨
    float4 frustumPlanes[4];
    
    // Left 평면: 원점, c1, c3로 정의되는 평면
    float3 leftNormal = normalize(cross(c3.xyz, c1.xyz));
    frustumPlanes[0] = float4(leftNormal, 0.0f);
    
    // Right 평면: 원점, c2, c4로 정의되는 평면  
    float3 rightNormal = normalize(cross(c2.xyz, c4.xyz));
    frustumPlanes[1] = float4(rightNormal, 0.0f);
    
    // Top 평면: 원점, c3, c4로 정의되는 평면
    float3 topNormal = normalize(cross(c4.xyz, c3.xyz));
    frustumPlanes[2] = float4(topNormal, 0.0f);
    
    // Bottom 평면: 원점, c1, c2로 정의되는 평면
    float3 bottomNormal = normalize(cross(c1.xyz, c2.xyz));
    frustumPlanes[3] = float4(bottomNormal, 0.0f);
    
    // --- 2단계: Light Culling 활성화 여부에 따른 처리 ---
    
    // 그룹 내 스레드들이 전체 광원 목록을 나누어 처리하도록 범위 설정
    const uint threadsPerGroup = TILE_SIZE * TILE_SIZE;
    const uint lightsPerThread = (NumLights + threadsPerGroup - 1) / threadsPerGroup;
    const uint startLightIndex = GI * lightsPerThread;
    const uint endLightIndex = min(startLightIndex + lightsPerThread, NumLights);
    
    // Light Culling이 비활성화된 경우 모든 라이트를 타일에 저장
    if (EnableCulling == 0)
    {
        // 모든 라이트를 타일에 저장 (Culling 없이)
        for (uint lightIndex = startLightIndex; lightIndex < endLightIndex; ++lightIndex)
        {
            uint index;
            InterlockedAdd(numVisibleLights, 1, index);
            if (index < 1024)
            {
                visibleLightIndices[index] = lightIndex;
            }
        }
    }
    else
    {
        // Light Culling 활성화: 기존 프러스텀 검사 로직 사용
        for (uint lightIndex = startLightIndex; lightIndex < endLightIndex; ++lightIndex)
        {
            Light light = AllLights[lightIndex];
            
            // 광원 위치를 월드 공간에서 뷰 공간으로 변환
            float4 lightPosView = mul(View, float4(light.position.xyz, 1.0));
            float lightRadius = light.position.w;
            uint lightType = (uint)light.direction.w;
            
            bool isVisible = true;
            
            if (lightType == LIGHT_TYPE_AMBIENT)
            {
                isVisible = true;     
            }
            else if (lightType == LIGHT_TYPE_DIRECTIONAL)
            {
                isVisible = true;
            }
            // 포인트 라이트: 구-프러스텀 교차 검사를 수행
            else if (lightType == LIGHT_TYPE_POINT)
            {
                //반복 없이 풀어 쓰라는 키워드 => jump 배제로 성능 향상
                [unroll]
                for (uint planeIndex = 0; planeIndex < 4; ++planeIndex)
                {
                    // 광원 중심과 평면 사이의 거리를 계산 거리가 반지름보다 작으면 평면 뒤에 있음
                    float distance = dot(frustumPlanes[planeIndex].xyz, lightPosView.xyz) + frustumPlanes[planeIndex].w;
                    if (distance < -lightRadius)
                    {
                        isVisible = false;
                        break;
                    }
                }

                // Z-테스트는 Point/Spot 라이트에만 적용
                if (isVisible)
                {
                    isVisible = IsInZRange(lightPosView, lightRadius);
                }
            }
            // 스포트 라이트: 원뿔-프러스텀 교차 검사를 수행
            else if (lightType == LIGHT_TYPE_SPOT)
            {
                // 원뿔을 감싸는 바운딩 스피어로 근사하여 검사
                // 뷰 공간에서 원뿔 지오메트리 계산
                float3 lightDirView = normalize(mul(View, float4(light.direction.xyz, 0.0)).xyz); // 뷰 공간에서의 라이트 방향
                float cosOuterAngle = light.angles.y; // 외부 원뿔 각도의 코사인 값

                // light.position.w (lightRadius)는 원뿔의 높이(range)
                float coneHeight = lightRadius;
                
                // tan(angle) = sqrt(1 - cos^2) / cos
                float tanOuterAngle = sqrt(1.0f - cosOuterAngle * cosOuterAngle) / cosOuterAngle;
                float baseRadius = coneHeight * tanOuterAngle;

                // 원뿔 밑면 원의 중심
                float3 coneEnd = lightPosView.xyz + lightDirView * coneHeight;
                
                // 바운딩 스피어의 중심은 원뿔의 꼭짓점과 밑면 중심의 중간입니다.
                float3 sphereCenter = (lightPosView.xyz + coneEnd) * 0.5f;
                
                // 바운딩 스피어의 반지름은 중심점에서 원뿔 꼭짓점까지의 거리와 같습니다.
                // (피타고라스 정리로 계산)
                float halfHeight = coneHeight * 0.5f;
                float sphereRadius = sqrt(halfHeight * halfHeight + baseRadius * baseRadius);
                
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

                // Z-테스트는 Point/Spot 라이트에만 적용
                if (isVisible)
                {
                    isVisible = IsInZRange(lightPosView, lightRadius);
                }
            }
            
            // 최종적으로 보이는 광원이면, 공유 메모리에 해당 광원의 인덱스를 추가
            if (isVisible)
            {
                uint index;
                // 여러 스레드가 동시에 접근할 수 있으므로 원자적 연산(InterlockedAdd)으로 카운터를 안전하게 증가
                InterlockedAdd(numVisibleLights, 1, index);
                if (index < 1024) // 공유 메모리 크기를 넘지 않는지 확인
                {
                    visibleLightIndices[index] = lightIndex;
                }
            }
        }
    }
    
    // 그룹 내 모든 스레드가 광원 검사를 마칠 때까지 대기
    GroupMemoryBarrierWithGroupSync();
    
    // --- 3단계: UAV에 결과 저장 ---
    
    // 그룹의 첫 스레드가, 보이는 광원이 있을 경우에만 결과를 전역 버퍼에 기록
    if (GI == 0 && numVisibleLights > 0)
    {
        uint globalOffset;
        // LightIndexBuffer의 전역 카운터(0번 인덱스)를 원자적으로 증가시켜, 이 타일이 사용할 공간을 예약
        InterlockedAdd(LightIndexBuffer[0], numVisibleLights, globalOffset);
        globalOffset += 1; // 0번 인덱스는 카운터이므로 1부터 실제 데이터 저장
        
        uint globalTileX = (ViewportOffset.x / TILE_SIZE) + Gid.x;
        uint globalTileY = (ViewportOffset.y / TILE_SIZE) + Gid.y;
        uint globalWindowTileWidth = (RenderTargetSize.x + TILE_SIZE - 1) / TILE_SIZE;
        uint tileIndex = globalTileY * globalWindowTileWidth + globalTileX;

        TileLightInfo[tileIndex] = uint2(globalOffset, min(numVisibleLights, 1024u));
    }
    
    // 그룹 내 모든 스레드가 협력하여 공유 메모리의 내용을 전역 버퍼(LightIndexBuffer)에 복사
    if (numVisibleLights > 0)
    {
        uint globalTileX = (ViewportOffset.x / TILE_SIZE) + Gid.x;
        uint globalTileY = (ViewportOffset.y / TILE_SIZE) + Gid.y;
        uint globalWindowTileWidth = (RenderTargetSize.x + TILE_SIZE - 1) / TILE_SIZE;
        uint tileIndex = globalTileY * globalWindowTileWidth + globalTileX;

        uint globalOffset = TileLightInfo[tileIndex].x;
        uint actualCount = min(numVisibleLights, 1024u);
        
        // 스레드들이 순번대로 돌아가며 visibleLightIndices의 내용을 LightIndexBuffer로 복사
        for (uint i = GI; i < actualCount; i += threadsPerGroup)
        {
            LightIndexBuffer[globalOffset + i] = visibleLightIndices[i];
        }
    }
}