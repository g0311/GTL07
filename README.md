# Week7 Team7(SignBridge, AddressKite, ParkStandDown, StandardZero) 기술문서

## 개요

본 문서는 GTL07 프로젝트에서 구현된 렌더링 기능들에 대한 기술 명세서입니다. 주요 구현 기능으로는 4가지 라이트 타입, 4가지 라이팅 모델, 노말 매핑, 월드 노말 뷰, POM(Parallax Occlusion Mapping) 등이 있습니다.

---

## 1. 라이트 시스템

프로젝트에서는 4가지 타입의 라이트를 지원합니다.

### 1.1 Ambient Light (환경광)

- **파일 위치**: `Engine/Source/Actor/Light/Private/AmbientLightActor.cpp`
- **셰이더 위치**: `Engine/Asset/Shader/LightingCommon.hlsli:215-218`
- **설명**: 씬 전체에 균일하게 적용되는 기본 조명입니다.
- **주요 파라미터**:
  - `Color`: 환경광 색상 (RGB)
  - `Intensity`: 강도

```hlsl
float3 CalculateAmbientLight(FAmbientLightInfo AmbientLight)
{
    return AmbientLight.Color.rgb * AmbientLight.Intensity;
}
```

### 1.2 Directional Light (방향광)

- **파일 위치**: `Engine/Source/Actor/Light/Private/DirectionalLightActor.cpp`
- **셰이더 위치**: `Engine/Asset/Shader/LightingCommon.hlsli:220-223`
- **설명**: 태양광과 같이 특정 방향에서 오는 평행광입니다.
- **주요 파라미터**:
  - `Direction`: 빛의 방향
  - `Color`: 빛의 색상
  - `Intensity`: 강도

### 1.3 Point Light (점광원)

- **파일 위치**: `Engine/Source/Actor/Light/Private/PointLightActor.cpp`
- **셰이더 위치**: `Engine/Asset/Shader/LightingCommon.hlsli:225-237`
- **설명**: 특정 위치에서 모든 방향으로 퍼져나가는 빛입니다.
- **주요 파라미터**:
  - `Position`: 빛의 위치
  - `Radius`: 영향 반경
  - `Color`: 빛의 색상
  - `Intensity`: 강도
  - `FalloffExtent`: 감쇠 정도

**감쇠 계산**:

```hlsl
float RangeAttenuation = saturate(1.0 - (Distance * Distance) / (Radius * Radius));
RangeAttenuation = pow(RangeAttenuation, max(FalloffExtent, 0.0));
```

### 1.4 Spot Light (스포트라이트)

- **파일 위치**: `Engine/Source/Actor/Light/Private/SpotLightActor.cpp`
- **셰이더 위치**: `Engine/Asset/Shader/LightingCommon.hlsli:239-257`
- **설명**: 특정 방향으로 원뿔 모양으로 퍼지는 빛입니다.
- **주요 파라미터**:
  - `Position`: 빛의 위치
  - `Direction`: 빛의 방향
  - `Color`: 빛의 색상
  - `Intensity`: 강도
  - `CosInner/CosOuter`: 내부/외부 원뿔 각도
  - `Falloff`: 감쇠 정도

**원뿔 감쇠 계산**:

```hlsl
float CurCos = dot(-normalize(LightVec), normalize(Direction));
float DirectionAttenuation = saturate((CurCos - CosOuter) / max(CosInner - CosOuter, 1e-5));
```

---

## 2. 라이팅 모델

프로젝트에서는 4가지 라이팅 모델을 지원합니다.

### 2.1 Gouraud Shading (구로 셰이딩)

- **셰이더 위치**: `Engine/Asset/Shader/UberShader.hlsl:370-380`
- **설명**: 버텍스 셰이더에서 라이팅을 계산하고 픽셀 셰이더에서 보간하는 방식입니다.
- **특징**:
  - 성능이 우수 (Per-Vertex 계산)
  - 품질이 낮음 (보간으로 인한 품질 저하)
  - 고전적인 라이팅 기법

**구현 방식**:

```hlsl
#if LIGHTING_MODEL_GOURAUD
    // Vertex Shader에서 모든 라이트 계산
    FLightSegment LightColor = CalculateAllLights(Output.WorldPosition, Output.WorldNormal, ViewDir, Shininess);
    Output.Ambient = LightColor.Ambient;
    Output.Diffuse = LightColor.Diffuse;
    Output.Specular = LightColor.Specular;
#endif
```

### 2.2 Lambert (램버트)

- **셰이더 위치**: `Engine/Asset/Shader/LightingCommon.hlsli:176-182`
- **설명**: Diffuse만 계산하는 기본적인 라이팅 모델입니다.
- **수식**: `L = LightColor * max(N · L, 0) / π`
- **특징**:
  - Specular 성분 없음
  - 부드러운 확산광 표현

```hlsl
float3 CalculateDiffuse(float3 LightColor, float3 WorldNormal, float3 LightDir)
{
    WorldNormal = normalize(WorldNormal);
    LightDir = normalize(LightDir);
    float NdotL = dot(WorldNormal, LightDir);
    return LightColor * NdotL * INV_PI;
}
```

### 2.3 Phong (퐁)

- **셰이더 위치**: `Engine/Asset/Shader/LightingCommon.hlsli:197-203`
- **설명**: Reflection Vector를 사용한 Specular 계산 방식입니다.
- **수식**: `Specular = LightColor * (R · V)^Shininess`
- **특징**:
  - 정확한 반사광 표현
  - 계산 비용이 높음

```hlsl
#elif LIGHTING_MODEL_PHONG
    float3 R = reflect(-LightDir, WorldNormal);
    float VdotR = saturate(dot(R, ViewDir));
    if (VdotR <= EPS)
        return 0;
    return LightColor * pow(VdotR, Shininess);
#endif
```

### 2.4 Blinn-Phong (블린퐁)

- **셰이더 위치**: `Engine/Asset/Shader/LightingCommon.hlsli:204-212`
- **설명**: Half Vector를 사용한 최적화된 Specular 계산 방식입니다.
- **수식**: `Specular = LightColor * (N · H)^Shininess`, 여기서 `H = normalize(L + V)`
- **특징**:
  - Phong보다 성능이 우수
  - 더 넓은 하이라이트 영역

```hlsl
#elif LIGHTING_MODEL_BLINNPHONG
    float3 H = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(WorldNormal, H));
    return LightColor * pow(NdotH, Shininess);
#endif
```

---

## 3. 노말 매핑 (Normal Mapping)

- **셰이더 위치**: `Engine/Asset/Shader/UberShader.hlsl:288-326`
- **설명**: 법선 맵을 사용하여 표면의 세밀한 디테일을 표현하는 기법입니다.

### 3.1 구현 방식

1. **Tangent Space Normal 샘플링**:

```hlsl
float3 TangentNormal = NormalTexture.Sample(SamplerWrap, UV).rgb;
TangentNormal = TangentNormal * 2.0f - 1.0f; // [0,1] → [-1,1] 변환
TangentNormal.x = -TangentNormal.x; // DirectX 좌표계 보정
```

2. **TBN 행렬을 사용한 World Space 변환**:

```hlsl
float3 N = WorldNormal;    // Normal
float3 T = WorldTangent;   // Tangent
float3 B = WorldBitangent; // Bitangent

// Tangent space → World space 변환
WorldNormal = normalize(TangentNormal.x * T + TangentNormal.y * B + TangentNormal.z * N);
```

### 3.2 주요 특징

- TBN (Tangent, Bitangent, Normal) 행렬 사용
- DirectX 좌표계에 맞춘 X축 반전 처리
- Safety check를 통한 안정성 확보

---

## 4. 월드 노말 뷰 (World Normal View)

- **셰이더 위치**: `Engine/Asset/Shader/WorldNormalShader.hlsl`
- **설명**: 월드 공간의 노말을 RGB 색상으로 시각화하는 디버그 뷰입니다.

### 4.1 구현 방식

1. **Normal Texture 샘플링 및 디코딩**:

```hlsl
float4 encodedNormal = NormalTexture.Sample(PointSampler, uv);
float3 worldNormal = encodedNormal.rgb * 2.0f - 1.0f; // [0,1] → [-1,1]
worldNormal = normalize(worldNormal);
```

2. **시각화를 위한 색상 인코딩**:

```hlsl
// Unreal Engine 스타일
// R channel = X axis (Red: +X, Cyan: -X)
// G channel = Y axis (Green: +Y, Magenta: -Y)
// B channel = Z axis (Blue: +Z, Yellow: -Z)
float3 visualColor = worldNormal * 0.5f + 0.5f;
```

### 4.2 색상 맵핑

- **Red(빨강)**: +X 방향
- **Cyan(청록)**: -X 방향
- **Green(초록)**: +Y 방향
- **Magenta(자홍)**: -Y 방향
- **Blue(파랑)**: +Z 방향
- **Yellow(노랑)**: -Z 방향

---

## 5. POM (Parallax Occlusion Mapping)

- **셰이더 위치**: `Engine/Asset/Shader/UberShader.hlsl:82-162`
- **설명**: Height Map을 사용하여 표면의 깊이감을 표현하는 고급 텍스처링 기법입니다.

### 5.1 주요 파라미터

- `HeightScale`: 깊이 스케일 (cbuffer로 전달)
- `MinLayers`: 최소 샘플링 레이어 수 (32)
- `MaxLayers`: 최대 샘플링 레이어 수 (128)

### 5.2 구현 단계

#### Step 1: Tangent Space 변환

```hlsl
float3 WorldToTangentSpace(
    float3 WorldVec,
    float3 WorldNormal,
    float3 WorldTangent,
    float3 WorldBitangent)
{
    float3x3 TBN = transpose(float3x3(T, B, N));
    return mul(WorldVec, TBN);
}
```

#### Step 2: 동적 레이어 수 계산

```hlsl
// 각도에 따라 레이어 수 조절 (각진 각도일수록 더 많은 샘플링)
float NumLayers = lerp(MaxLayers, MinLayers, abs(dot(float3(0, 0, 1), ViewDirTangent)));
```

#### Step 3: Ray Marching

```hlsl
float2 DeltaUV = ParallaxDir * HeightScale / NumLayers;
float2 CurrentUV = UV;
float CurrentHeight = BumpTexture.Sample(SamplerWrap, CurrentUV).r;

[unroll(64)]
for(int i = 0; i < NumLayers; i++)
{
    if(CurrentLayerDepth >= CurrentHeight)
        break;

    CurrentUV -= DeltaUV;
    CurrentHeight = BumpTexture.Sample(SamplerWrap, CurrentUV).r;
    CurrentLayerDepth += LayerDepth;
}
```

#### Step 4: Relief Mapping (선형 보간)

```hlsl
// 교차점 정밀화
float2 PrevUV = CurrentUV + DeltaUV;
float AfterDepth = CurrentHeight - CurrentLayerDepth;
float BeforeDepth = BumpTexture.Sample(SamplerWrap, PrevUV).r - (CurrentLayerDepth - LayerDepth);

float Weight = saturate(AfterDepth / (AfterDepth - BeforeDepth));
float2 FinalUV = lerp(CurrentUV, PrevUV, Weight);
```

### 5.3 최적화 및 안전장치

- **View Direction 체크**: 너무 작거나 수평에 가까운 경우 POM 비활성화
- **UV 이동 제한**: 0.5 이상 이동하면 원본 UV 반환
- **Division by Zero 방지**: 분모가 0에 가까우면 기본값 사용

### 5.4 특징

- 각도에 따른 동적 레이어 수 조절로 성능 최적화
- Relief Mapping을 통한 정밀한 교차점 계산
- DirectX 좌표계에 맞춘 X축 반전 처리

---

## 6. Light Culling 시스템

### 6.1 Tiled Light Culling

- **셰이더 위치**: `Engine/Asset/Shader/LightingCommon.hlsli:323-401`
- **설명**: 화면을 타일로 나누어 각 타일에 영향을 주는 라이트만 계산하는 최적화 기법입니다.

### 6.2 클러스터 기반 라이팅

```hlsl
uint3 GetClusterIndexFromSVPos(float4 svPosition, float viewZ, ...)
{
    float2 viewportRelativePos = svPosition.xy - viewportOffset;
    uint clusterX = (uint)floor(viewportRelativePos.x / CLUSTER_SIZE_X);
    uint clusterY = (uint)floor(viewportRelativePos.y / CLUSTER_SIZE_Y);
    uint clusterZ = (uint)min((uint)(t * CLUSTER_SIZE_Z), CLUSTER_SIZE_Z - 1);
    return uint3(clusterX, clusterY, clusterZ);
}
```

### 6.3 디버그 모드

- **Heat Map 시각화**: 픽셀당 라이트 개수를 색상으로 표현
  - 파란색: 라이트 적음
  - 초록색: 보통
  - 노란색: 많음
  - 빨간색: 매우 많음

---

## 7. 파일 구조

### 7.1 주요 셰이더 파일

```
Engine/Asset/Shader/
├── UberShader.hlsl              # 메인 셰이더 (라이팅, 노말맵, POM)
├── LightingCommon.hlsli         # 라이팅 공통 함수
├── WorldNormalShader.hlsl       # 월드 노말 시각화
├── PointLightShader.hlsl        # 포인트 라이트 전용
└── ShaderDefines.hlsli          # 셰이더 정의 및 플래그
```

### 7.2 주요 C++ 파일

```
Engine/Source/
├── Actor/Light/Private/
│   ├── AmbientLightActor.cpp
│   ├── DirectionalLightActor.cpp
│   ├── PointLightActor.cpp
│   └── SpotLightActor.cpp
└── Component/Light/Private/
    ├── LightComponent.cpp
    └── LightComponentBase.cpp
```

---

## 8. 성능 고려사항

### 8.1 라이팅 모델 성능 비교

1. **Gouraud**: 가장 빠름 (Vertex Shader에서 계산)
2. **Lambert**: 빠름 (Specular 계산 없음)
3. **Blinn-Phong**: 보통 (Half Vector 사용)
4. **Phong**: 느림 (Reflection Vector 계산)

### 8.2 POM 성능 팁

- 레이어 수를 각도에 따라 동적으로 조절
- 필요한 경우에만 활성화 (HAS_BUMP_MAP 플래그)
- UV 이동 제한으로 비정상적인 결과 방지

### 8.3 Light Culling 효과

- Tiled/Clustered 방식으로 불필요한 라이트 계산 제거
- 다수의 라이트가 있는 씬에서 큰 성능 향상
- 디버그 모드로 최적화 상태 확인 가능

---

## 9. 사용 예시

### 9.1 노말 매핑 활성화

```cpp
MaterialFlags |= HAS_NORMAL_MAP;
```

### 9.2 POM 활성화

```cpp
MaterialFlags |= HAS_BUMP_MAP;
HeightScale = 0.05f; // 깊이 스케일 조정
```

### 9.3 라이팅 모델 변경

셰이더 컴파일 시 다음 매크로 정의:

- `LIGHTING_MODEL_GOURAUD`
- `LIGHTING_MODEL_LAMBERT`
- `LIGHTING_MODEL_PHONG`
- `LIGHTING_MODEL_BLINNPHONG`
- `LIGHTING_MODEL_UNLIT`

---

## 10. 참고사항

- 모든 셰이더는 DirectX 11 기준으로 작성되었습니다.
- 언리얼 엔진 코딩 컨벤션을 따릅니다.
- CoreTypes.h, Types.h를 참고하여 타입을 정의했습니다.
- 수학 연산은 C++ 표준 라이브러리를 사용합니다.
