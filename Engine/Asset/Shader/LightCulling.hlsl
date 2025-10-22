/**
 * @file LightCulling.hlsl
 * @brief 클러스터 기반 광원 컬링 컴퓨트 셰이더 (3D Clustered Light Culling)
 */

// 클러스터 그리드 크기 정의
#define CLUSTER_SIZE_X 32
#define CLUSTER_SIZE_Y 32
#define CLUSTER_SIZE_Z 16

// 광원 타입 정의 (C++ ELightType enum과 동일한 순서)
#define LIGHT_TYPE_AMBIENT 0
#define LIGHT_TYPE_DIRECTIONAL 1
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

// 씬에 존재하는 모든 광원의 정보
StructuredBuffer<Light> AllLights;

// 컬링 계산에 필요한 파라미터들을 담는 상수 버퍼
cbuffer CullingParams : register(b0)
{
    matrix View; // 뷰 행렬
    matrix Projection; // 투영 행렬
    uint2 ViewportOffset; // 현재 뷰포트의 시작 오프셋
    uint2 ViewportSize; // 현재 뷰포트의 크기
    uint NumLights; // 씬에 있는 전체 광원 개수
    float NearClip; // Near 클리핑 평면
    float FarClip; // Far 클리핑 평면
    uint Padding; // 16바이트 정렬을 위한 패딩
};

// 출력용 UAV(Unordered Access View) 버퍼
RWStructuredBuffer<uint> LightIndexBuffer; // 클러스터별로 컬링된 광원 인덱스들이 저장될 전역 버퍼
RWStructuredBuffer<uint2> ClusterLightInfo;   // 각 클러스터의 광원 정보 (LightIndexBuffer에서의 시작 오프셋, 광원 개수)

// 스레드 그룹 내에서만 사용하는 공유 메모리. 한 클러스터에 속한 광원 인덱스들을 임시 저장
groupshared uint visibleLightIndices[1024]; // 최대 1024개의 광원 인덱스 저장
groupshared uint numVisibleLights;          // 현재 클러스터에서 찾은 광원의 개수

// 클러스터 Z 인덱스로부터 뷰 공간 Z 범위 계산 (선형 분포)
float2 GetClusterViewZRange(uint clusterZ)
{
    // Near ~ Far 사이를 CLUSTER_SIZE_Z개로 선형 분할
    float t0 = (float)clusterZ / (float)CLUSTER_SIZE_Z;
    float t1 = (float)(clusterZ + 1) / (float)CLUSTER_SIZE_Z;
    
    float nearZ = NearClip + t0 * (FarClip - NearClip);
    float farZ = NearClip + t1 * (FarClip - NearClip);
    
    return float2(nearZ, farZ);
}

// 컴퓨트 셰이더 메인 함수. 각 스레드 그룹이 하나의 클러스터를 처리
[numthreads(16, 16, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 Tid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    // --- 초기화  ---
    if (GI == 0)
    {
        numVisibleLights = 0;
    }
    // 그룹 내 모든 스레드가 초기화를 마칠 때까지 대기.
    GroupMemoryBarrierWithGroupSync();

    // --- 1단계: 클러스터 프러스텀 계산 (뷰 공간 기준) ---
    // Gid.xyz = 클러스터의 3D 인덱스
    
    // (1) 뷰 공간 계산에 필요한 FOV(시야각) 정보를 투영 행렬로부터 역산
    float tanHalfFovX = 1.0f / Projection[0][0];
    float tanHalfFovY = 1.0f / Projection[1][1];

    // (2) 클러스터의 뷰포트 내 상대 픽셀 좌표 범위를 계산 (뷰포트 기준)
    float2 clusterMinPixelPos = Gid.xy * float2(CLUSTER_SIZE_X, CLUSTER_SIZE_Y);
    float2 clusterMaxPixelPos = (Gid.xy + 1) * float2(CLUSTER_SIZE_X, CLUSTER_SIZE_Y);

    // (3) 뷰포트 내 상대 좌표를 UV(0~1)로 변환 후 NDC(-1 ~ 1 범위)로 변환
    float2 uv_LT = clusterMinPixelPos / ViewportSize;
    float2 uv_RB = clusterMaxPixelPos / ViewportSize;
    float2 ndc_LT = float2(uv_LT.x * 2.0f - 1.0f, 1.0f - uv_LT.y * 2.0f);
    float2 ndc_RB = float2(uv_RB.x * 2.0f - 1.0f, 1.0f - uv_RB.y * 2.0f);

    // (4) NDC 좌표를 뷰 공간의 방향 벡터로 변환
    float view_L = ndc_LT.x * tanHalfFovX; // 왼쪽 X
    float view_R = ndc_RB.x * tanHalfFovX; // 오른쪽 X
    float view_T = ndc_LT.y * tanHalfFovY; // 위쪽 Y
    float view_B = ndc_RB.y * tanHalfFovY; // 아래쪽 Y

    // (5) 클러스터 Z 범위 계산 (뷰 공간 Z 기반)
    float2 viewZRange = GetClusterViewZRange(Gid.z);
    float nearZ = viewZRange.x;
    float farZ = viewZRange.y;
    
    // (6) 클러스터의 4개 꼭짓점 계산 (뷰 공간)
    // view_L/R/T/B는 z=1 평면에서의 방향이므로, 실제 깊이로 스케일링
    // Near 평면의 4개 꼭짓점
    float4 c1_near = float4(view_L, view_B , 1.0f, 1.0f);
    float4 c2_near = float4(view_R, view_B, 1.0f, 1.0f);
    float4 c3_near = float4(view_L, view_T, 1.0f, 1.0f);
    float4 c4_near = float4(view_R, view_T, 1.0f, 1.0f);
    
    // (7) 클러스터 프러스텀의 측면 4개 평면 계산
    // 평면 방정식: dot(normal, point) + d = 0
    // 법선은 프러스텀 안쪽을 향함 (distance > -radius면 보임)
    // Near/Far 평면은 Z 범위 비교로 별도 처리
    float4 frustumPlanes[4];
    
    // 측면 4개 평면: 카메라 원점(0,0,0)에서 시작하는 평면
    // 각 평면은 원점을 통과하므로 d=0
    
    float3 leftNormal = normalize(cross(c3_near.xyz, c1_near.xyz));
    frustumPlanes[0] = float4(leftNormal, 0.0f);
    
    float3 rightNormal = normalize(cross(c2_near.xyz, c4_near.xyz));
    frustumPlanes[1] = float4(rightNormal, 0.0f);
    
    float3 topNormal = normalize(cross(c4_near.xyz, c3_near.xyz));
    frustumPlanes[2] = float4(topNormal, 0.0f);
    
    float3 bottomNormal = normalize(cross(c1_near.xyz, c2_near.xyz));
    frustumPlanes[3] = float4(bottomNormal, 0.0f);
    
    // --- 2단계: Light Culling 수행 ---
    
    // 그룹 내 스레드들이 전체 광원 목록을 나누어 처리하도록 범위 설정
    const uint threadsPerGroup = 16 * 16;
    const uint lightsPerThread = (NumLights + threadsPerGroup - 1) / threadsPerGroup;
    const uint startLightIndex = GI * lightsPerThread;
    const uint endLightIndex = min(startLightIndex + lightsPerThread, NumLights);
    
    // Light Culling 활성화: 프러스텀 검사 로직 수행
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
            float lightMinZ = lightPosView.z - lightRadius;
            float lightMaxZ = lightPosView.z + lightRadius;
                
            // 클러스터 범위: [nearZ, farZ]
            if (lightMaxZ < nearZ || lightMinZ > farZ)
            {
                isVisible = false;
            }
            // Near/Far 평면 체크 (구의 Z 범위와 클러스터 Z 범위 비교)
            if (isVisible)
            {
                // 측면 4개 평면 체크 (원점 통과, d=0)
                [unroll]
                for (uint planeIndex = 0; planeIndex < 4; ++planeIndex)
                {
                    float distance = dot(frustumPlanes[planeIndex].xyz, lightPosView.xyz);
                    if (distance < -lightRadius)
                    {
                        isVisible = false;
                        break;
                    }
                }
            }
        }
        // 스포트 라이트: 원뿔-프러스텀 교차 검사를 수행
        else if (lightType == LIGHT_TYPE_SPOT)
        {
            // 원뿔을 감싸는 바운딩 스피어로 근사하여 검사
            float3 lightDirView = normalize(mul(View, float4(light.direction.xyz, 0.0)).xyz);
            float cosOuterAngle = light.angles.y;
            float coneHeight = lightRadius;
            
            float tanOuterAngle = sqrt(1.0f - cosOuterAngle * cosOuterAngle) / cosOuterAngle;
            float baseRadius = coneHeight * tanOuterAngle;
            float3 coneEnd = lightPosView.xyz + lightDirView * coneHeight;
            float3 sphereCenter = (lightPosView.xyz + coneEnd) * 0.5f;
            float halfHeight = coneHeight * 0.5f;
            float sphereRadius = sqrt(halfHeight * halfHeight + baseRadius * baseRadius);

            float sphereMinZ = sphereCenter.z - sphereRadius;
            float sphereMaxZ = sphereCenter.z + sphereRadius;
                
            if (sphereMaxZ < nearZ || sphereMinZ > farZ)
            {
                isVisible = false;
            }

            if (isVisible)
            {
                // 측면 4개 평면 체크
                [unroll]
                for (uint planeIndex = 0; planeIndex < 4; ++planeIndex)
                {
                    float distance = dot(frustumPlanes[planeIndex].xyz, sphereCenter);
                    if (distance < -sphereRadius)
                    {
                        isVisible = false;
                        break;
                    }
                }
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
    
    // 그룹 내 모든 스레드가 광원 검사를 마칠 때까지 대기
    GroupMemoryBarrierWithGroupSync();
    
    // --- 3단계: UAV에 결과 저장 ---
    
    // 그룹의 첫 스레드가, 보이는 광원이 있을 경우에만 결과를 전역 버퍼에 기록
    if (GI == 0 && numVisibleLights > 0)
    {
        uint globalOffset;
        // LightIndexBuffer의 전역 카운터(0번 인덱스)를 원자적으로 증가시켜, 이 클러스터가 사용할 공간을 예약
        InterlockedAdd(LightIndexBuffer[0], numVisibleLights, globalOffset);
        globalOffset += 1; // 0번 인덱스는 카운터이므로 1부터 실제 데이터 저장
        
        // 클러스터 3D 인덱스 계산
        uint clustersX = (ViewportSize.x + CLUSTER_SIZE_X - 1) / CLUSTER_SIZE_X;
        uint clustersY = (ViewportSize.y + CLUSTER_SIZE_Y - 1) / CLUSTER_SIZE_Y;
        uint clusterIndex = Gid.z * (clustersX * clustersY) + Gid.y * clustersX + Gid.x;

        ClusterLightInfo[clusterIndex] = uint2(globalOffset, min(numVisibleLights, 1024u));
    }
    
    GroupMemoryBarrierWithGroupSync();

    // 그룹 내 모든 스레드가 협력하여 공유 메모리의 내용을 전역 버퍼(LightIndexBuffer)에 복사
    if (numVisibleLights > 0)
    {
        // 클러스터 3D 인덱스 계산
        uint clustersX = (ViewportSize.x + CLUSTER_SIZE_X - 1) / CLUSTER_SIZE_X;
        uint clustersY = (ViewportSize.y + CLUSTER_SIZE_Y - 1) / CLUSTER_SIZE_Y;
        uint clusterIndex = Gid.z * (clustersX * clustersY) + Gid.y * clustersX + Gid.x;

        uint globalOffset = ClusterLightInfo[clusterIndex].x;
        uint actualCount = min(numVisibleLights, 1024u);
        
        // 스레드들이 순번대로 돌아가며 visibleLightIndices의 내용을 LightIndexBuffer로 복사
        for (uint i = GI; i < actualCount; i += threadsPerGroup)
        {
            LightIndexBuffer[globalOffset + i] = visibleLightIndices[i];
        }
    }
}