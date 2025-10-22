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
    uint32 ViewportOffset[2];    // 8 bytes
    uint32 ViewportSize[2];      // 8 bytes
    uint32 NumLights;            // 4 bytes
    float NearClip;              // 4 bytes
    float FarClip;               // 4 bytes
    uint32 Padding;              // 4 bytes - 16바이트 정렬을 위한 패딩
};

// 라이트 타입 상수
enum class ELightType : uint32
{
    Ambient = 0,
    Directional = 1,
    Point = 2,
    Spot = 3
};

// 셰이더와 일치하는 라이트 구조체
struct FLightParams
{
    FVector4 Position;    // xyz: world position, w: radius
    FVector4 Color;       // xyz: color, w: intensity
    FVector4 Direction;   // xyz: direction (for spot), w: light type
    FVector4 Angles;      // x: inner cone angle (cos), y: outer cone angle (cos), z: falloff extent/falloff, w: InvRange2 (spot only)
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

private:
    UDeviceResources* DeviceResources = nullptr;

    // Shader
    ID3D11ComputeShader* CullingCS = nullptr;

    // Constant Buffer
    ID3D11Buffer* CullingParamsCB = nullptr;

    // UAV Buffers for light lists
    ID3D11UnorderedAccessView* LightIndexBufferUAV = nullptr;
    ID3D11UnorderedAccessView* ClusterLightInfoUAV = nullptr;
    
    // 라이트 데이터 버퍼 (고정 크기)
    ID3D11Buffer* AllLightsBuffer = nullptr;
    ID3D11ShaderResourceView* AllLightsSRV = nullptr;
    static constexpr uint32 MAX_LIGHTS = 1024; // 최대 라이트 개수
    
    // 클러스터 그리드 상수
    static constexpr uint32 CLUSTER_SIZE_X = 32;
    static constexpr uint32 CLUSTER_SIZE_Y = 32;
    static constexpr uint32 CLUSTER_SIZE_Z = 16;
};
