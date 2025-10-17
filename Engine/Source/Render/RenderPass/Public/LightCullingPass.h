#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

// Forward declarations
class UDeviceResources;
class UPipeline;

// Must match the cbuffer in LightCulling.hlsl
struct FCullingParams
{
    FMatrix View;                // 64 bytes
    FMatrix Projection;          // 64 bytes
    uint32 ScreenDimensions[2];  // 8 bytes
    uint32 ViewportOffset[2];    // 8 bytes
    uint32 ViewportSize[2];      // 8 bytes
    uint32 NumLights;            // 4 bytes
    uint32 Padding;              // 4 bytes 
};

// 라이트 타입 상수
enum class ELightType : uint32
{
    Point = 0,
    Spot = 1
};

// 셰이더와 일치하는 라이트 구조체
struct FLightParams
{
    FVector4 Position;    // xyz: world position, w: radius
    FVector4 Color;       // xyz: color, w: intensity
    FVector4 Direction;   // xyz: direction (for spot), w: light type
    FVector4 Angles;      // x: inner cone angle (cos), y: outer cone angle (cos), z,w: unused
};

class FLightCullingPass : public FRenderPass
{
public:
    FLightCullingPass(UPipeline* InPipeline, UDeviceResources* InDeviceResources);
    ~FLightCullingPass() override;

    void PreExecute(FRenderingContext& Context) override;
    void Execute(FRenderingContext& Context) override;
    void PostExecute(FRenderingContext& Context) override;

    void Release() override;
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
    
    // 라이트 데이터 버퍼 (고정 크기)
    ID3D11Buffer* AllLightsBuffer = nullptr;
    ID3D11ShaderResourceView* AllLightsSRV = nullptr;
    static constexpr uint32 MAX_LIGHTS = 1024; // 최대 라이트 개수

    // 리소스가 생성된 화면 크기 추적
    uint32 LastScreenWidth = 0;
    uint32 LastScreenHeight = 0;
};
