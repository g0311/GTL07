# Week7 Team5(이준용, 국동희, 정찬호, 정석영) 기술문서

## 1. 개요

본 문서는 KTL 엔진에 구현된 데칼 렌더링 시스템의 기술적 사양과 아키텍처를 설명합니다.

데칼 시스템은 씬(Scene) 내의 물체 표면에 텍스처를 투영(Projection)하여 총알 자국, 혈흔, 특정 문양 등 다양한 시각 효과를 표현하는 기능입니다. 본 엔진의 데칼 시스템은 포워드 렌더링(Forward Rendering) 파이프라인 내의 커스텀 렌더링 패스(Custom Rendering Pass)를 통해 구현되었습니다.

---
## 2. 핵심 아키텍처

데칼 시스템은 아래의 주요 클래스들 간의 상호작용을 통해 동작합니다.

-   **`ADecalActor`**: 레벨에 배치할 수 있는 액터(Actor)입니다. 데칼의 위치, 회전, 크기를 나타내는 컨테이너 역할을 합니다.
-   **`UDecalComponent`**: `ADecalActor`의 루트 컴포넌트(Root Component)로, 데칼의 모든 핵심 데이터와 로직을 포함합니다. 투영할 텍스처, 페이드 효과, 프로젝션 설정 등을 관리합니다.
-   **`FDecalPass`**: 렌더링 파이프라인의 한 단계로, 매 프레임 씬에 있는 모든 `UDecalComponent`를 찾아 대상 물체에 데칼을 그리는 모든 실제 작업을 수행합니다.

---
## 3. 컴포넌트 및 액터 구현

### 3.1. `UDecalComponent`

-   데칼의 모든 속성을 정의하는 핵심 컴포넌트입니다.
-   멤버 변수로 `DecalTexture`와 `FadeTexture`를 가져 텍스처 정보를 관리합니다.
-   직렬화(Serialization) 로직을 오버라이드하여, 텍스처 파일의 경로를 레벨 파일에 저장하고 로드합니다.

### 3.2. `ADecalActor`

-   `UDecalComponent`를 루트 컴포넌트로 사용하는 간단한 액터입니다.
-   에디터에서 사용자가 데칼을 시각적으로 확인하고 배치할 수 있도록, 식별용 아이콘을 표시하는 `UBillboardComponent`를 자식으로 가집니다. (`InitializeComponents` 함수에서 생성)
-   별도의 `Serialize` 함수 없이 부모인 `AActor`의 직렬화 로직을 상속받아, `OwnedComponents` 배열에 있는 모든 컴포넌트(데칼, 빌보드)의 정보를 재귀적으로 저장하고 로드합니다.

---
## 4. 렌더링 파이프라인 (`FDecalPass`)

`FDecalPass`는 포워드 렌더링 루프에서 데칼을 그리는 책임을 가집니다.

### 4.1. Show Flag 검사
`RenderingContext`에 포함된 `ShowFlags`를 확인하여, `EEngineShowFlags::SF_Decal` 플래그가 켜져 있을 때만 렌더링을 수행합니다. 이를 통해 에디터에서 데칼 표시 여부를 토글할 수 있습니다.

### 4.2. 대상 객체 선정 (Culling)
-   모든 데칼 컴포넌트를 순회하며, 씬의 모든 물체를 대상으로 데칼을 그리는 것은 매우 비효율적입니다.
-   따라서, **Octree**를 사용하여 데칼의 바운딩 박스(OBB)와 겹칠 가능성이 있는 물체들만 1차적으로 선별합니다.
-   선별된 물체들에 한해, **분리 축 이론(SAT)**을 구현한 `Intersects(OBB, AABB)` 함수로 더욱 정밀한 충돌 검사를 수행하여, 데칼을 다시 그려야 할 최종 대상 목록을 확정합니다.

### 4.3. 재-렌더링 (Re-Rendering) 및 Z-파이팅 해결
-   `FDecalPass`는 선별된 대상 객체들을 **`DecalShader.hlsl`이라는 전용 셰이더를 사용해서 한 번 더 그립니다.**
-   이때 동일한 위치에 지오메트리를 다시 그리면서 발생하는 Z-파이팅(표면이 겹쳐 지지직거리는 현상)을 방지하기 위해, 데칼 전용으로 생성된 **`DepthStencilState`**를 사용합니다.
-   이 상태는 뎁스 테스트는 정상적으로 수행(`DepthEnable = TRUE`, `DepthFunc = D3D11_COMPARISON_LESS_EQUAL`)하되, 뎁스 버퍼에 새로운 값을 쓰지는 않도록(`DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO`) 설정되어 있습니다. 이를 통해 데칼이 기존 지오메트리 위에 올바르게 그려지면서도 뎁스 값의 충돌을 회피합니다.
    ```cpp
    // Decal Depth Stencil (Depth Read, Stencil X)
    D3D11_DEPTH_STENCIL_DESC DecalDescription = {};
    DecalDescription.DepthEnable = TRUE;
    DecalDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DecalDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    
    Device->CreateDepthStencilState(&DecalDescription, &DecalDepthStencilState);
    ```
-   알파 블렌딩(Alpha Blending)을 활성화하여, 기존에 그려진 물체 표면 위에 데칼 텍스처가 자연스럽게 덧입혀지도록 합니다.

### 4.4. 통계(Stat) 수집
-   패스 실행 시 렌더링된 데칼의 총 개수, 충돌 검사를 수행한 컴포넌트의 수 등을 카운트합니다.
-   수집된 정보는 `UStatOverlay::GetInstance().RecordDecalStats()`를 통해 에디터의 통계 오버레이에 전달되어 렌더링 성능을 모니터링하는 데 사용됩니다.

---
## 5. 데칼 투영 및 셰이더 (`DecalShader.hlsl`)

### 5.1. 투영 방향 및 행렬 계산

-   **투영 방향**: 데칼은 `UDecalComponent`의 **로컬 X축을 Forward Vector**로 간주하고, 이 방향으로 텍스처를 투영합니다.
-   **행렬 계산**:
    -   **`DecalViewProjection`**: `FDecalPass`에서 계산되며, 월드 좌표계의 점을 데칼의 로컬 직육면체 공간으로 변환하는 행렬입니다. (`데칼의 World 역행렬 * 데칼의 Projection 행렬`)
    -   **`ConstantBufferViewProj`**: 렌더링 대상 물체를 화면에 올바르게 그리기 위해 필요한 **메인 카메라**의 View/Projection 행렬입니다.

### 5.2. 버텍스 셰이더 (`mainVS`)

버텍스 셰이더는 대상 물체의 정점을 최종 화면 위치로 변환하는 표준적인 역할을 수행합니다. 메인 카메라의 `View`와 `Projection` 행렬을 사용하여 MVP(Model-View-Projection) 변환을 계산하고, 픽셀 셰이더에 필요한 월드 좌표 등의 정보를 전달합니다.

```hlsl
PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    float4 Pos = mul(float4(Input.Position, 1.0f), World);
    Output.Position = mul(mul(Pos, View), Projection);
    Output.WorldPos = Pos;
    Output.Normal = normalize(mul(float4(Input.Normal, 0.0f), WorldInverseTranspose));
    Output.Tex = Input.Tex;

    return Output;
}
```

### 5.3. 픽셀 셰이더 (`mainPS`)

픽셀 셰이더는 실제 데칼 텍스처를 픽셀에 적용하는 핵심 로직을 담고 있습니다.

1.  **수동 클리핑 (Manual Clipping)**:
    -   버텍스 셰이더에서 전달받은 픽셀의 월드 좌표를 `DecalViewProjection` 행렬로 변환하여 데칼의 로컬 좌표를 얻습니다.
    -   이 좌표가 데칼의 직육면체 볼륨(예: x, y, z가 모두 -1.01 ~ 1.01 사이)을 벗어나는 경우, `discard` 명령으로 해당 픽셀의 렌더링을 즉시 중단합니다.
    -   경계 값에 `0.01f`와 같은 허용 오차(Tolerance)를 두어, 가장자리에서 발생하는 아티팩트를 방지합니다.

2.  **UV 좌표 계산**: 픽셀이 볼륨 안에 있다면, 로컬 좌표의 Y, Z 값을 변환하여 데칼 텍스처를 샘플링할 UV 좌표로 사용합니다.

3.  **페이드 효과 적용**: `FadeProgress` 값과 노이즈 텍스처인 `FadeTexture`를 사용하여 픽셀의 최종 알파(투명도) 값을 계산합니다. 이를 통해 텍스처의 어두운 부분부터 먼저 사라지는 등 유기적인 페이드 아웃 효과를 구현합니다.
    ```hlsl
    DecalColor.a *= 1.0f - saturate(FadeProgress / (FadeValue + 1e-6));
    ```

---
## 6. 응용: 가짜 스포트라이트 구현 (Application: Fake Spot Light)

본 엔진에 구현된 `UDecalComponent`와 `UBillboardComponent`를 조합하여, 동적 조명 계산 없이 효율적으로 스포트라이트 효과를 모방할 수 있습니다.

### 6.1. 개요

가짜 스포트라이트는 실제 광원을 계산하는 대신, 빛이 비치는 부분과 광원 자체를 각각 데칼과 빌보드로 그려서 표현하는 기법입니다. 저렴한 비용으로 준수한 퀄리티의 스포트라이트 효과를 낼 수 있어 씬의 분위기를 연출하는 데 효과적입니다.

### 6.2. 구성 요소

-   **광원 표현 (`UBillboardComponent`):**
    -   스포트라이트의 램프나 빛이 시작되는 지점의 '광원(light source)' 자체를 표현하는 데 사용됩니다.
    -   항상 카메라를 바라보는 빌보드의 특성을 이용하여, 렌즈 플레어나 빛 번짐(Glow) 텍스처를 렌더링하면 어느 각도에서나 광원이 자연스럽게 보이도록 할 수 있습니다.

-   **빛 투사 (`UDecalComponent`):**
    -   스포트라이트의 핵심인 '빛이 표면에 닿아 밝혀지는 효과'를 구현합니다.
    -   **텍스처:** 중앙은 밝고 가장자리로 갈수록 부드럽게 어두워지는 원형 그라데이션(gradient) 텍스처를 사용합니다.
    -   **투영:** 데칼의 투영 볼륨(Projection Volume)이 스포트라이트의 원뿔(Cone) 역할을 합니다. 원근 투영(Perspective Projection)을 사용하는 것이 원뿔 형태를 표현하는 데 더 적합합니다.
