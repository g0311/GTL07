#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

// Forward declarations
class UDeviceResources;
class UPipeline;

// Must match the cbuffer in LightCulling.hlsl
struct FCullingParams
{
    FMatrix View;
    FMatrix Projection;
    uint32 ScreenDimensions[2];
    uint32 ViewportOffset[2];  // 뷰포트 시작 위치 (x, y)
    uint32 ViewportSize[2];    // 뷰포트 크기 (width, height)
    uint32 NumLights;
    uint32 Padding[3];         // 패딩 조정
};

struct FPointLightParams
{
    FVector Position; //W에 Rddius 값 전달..
    FVector Color;
};

class FLightCullingPass : public FRenderPass
{
public:
    FLightCullingPass(UPipeline* InPipeline, UDeviceResources* InDeviceResources);
    ~FLightCullingPass() override;

    void Execute(FRenderingContext& Context) override;

private:
    void CreateResources();
    void ReleaseResources();
    void RecreateResourcesIfNeeded();

private:
    UDeviceResources* DeviceResources = nullptr;

    // Shader
    ID3D11ComputeShader* CullingCS = nullptr;

    // Constant Buffer
    ID3D11Buffer* CullingParamsCB = nullptr;

    // UAV Buffers for light lists
    ID3D11Buffer* LightIndexBuffer = nullptr;
    ID3D11UnorderedAccessView* LightIndexBufferUAV = nullptr;
    ID3D11ShaderResourceView* LightIndexBufferSRV = nullptr;

    ID3D11Buffer* TileLightInfoBuffer = nullptr;
    ID3D11UnorderedAccessView* TileLightInfoUAV = nullptr;
    ID3D11ShaderResourceView* TileLightInfoSRV = nullptr;

    // 리소스가 생성된 화면 크기 추적
    uint32 LastScreenWidth = 0;
    uint32 LastScreenHeight = 0;
};
