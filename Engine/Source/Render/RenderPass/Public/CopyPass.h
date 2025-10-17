#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

// Forward declarations
class UDeviceResources;
class UPipeline;

// This is also used by the simple copy pass to get RenderTargetSize
struct FCopyViewportConstants
{
	FVector2 RenderTargetSize;
	float Padding[2];
};

class FCopyPass : public FRenderPass
{
public:
    FCopyPass(
        UPipeline* InPipeline,
        UDeviceResources* InDeviceResources,
        ID3D11VertexShader* InVS,
        ID3D11PixelShader* InPS,
        ID3D11InputLayout* InLayout,
        ID3D11SamplerState* InSampler
    );
    ~FCopyPass();

    void PreExecute(FRenderingContext& Context) override;
    void Execute(FRenderingContext& Context) override;
    void PostExecute(FRenderingContext& Context) override;
    void Release() override;

private:
    UDeviceResources* DeviceResources = nullptr;
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11SamplerState* SamplerState = nullptr;
    ID3D11Buffer* ViewportBuffer = nullptr; // Created and owned by this pass
};