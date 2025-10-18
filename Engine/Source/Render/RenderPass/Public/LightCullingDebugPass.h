
#pragma once
#include "Render/RenderPass/Public/RenderPass.h"
#include <d2d1.h>
#include <dwrite.h>

class UDeviceResources;
class UPipeline;

class FLightCullingDebugPass : public FRenderPass
{
public:
    FLightCullingDebugPass(UPipeline* InPipeline, UDeviceResources* InDeviceResources);
    ~FLightCullingDebugPass() override;

    void PreExecute(FRenderingContext& Context) override;
    void Execute(FRenderingContext& Context) override;
    void PostExecute(FRenderingContext& Context) override;
    void Release() override;

private:
    void CreateResources();
    void ReleaseResources();
    void RenderDebugInfo(FRenderingContext& Context);
    std::wstring ToWString(const FString& InStr);

private:
    UDeviceResources* DeviceResources = nullptr;
    ID3D11Buffer* TileLightInfoStagingBuffer = nullptr;

    // D2D/DWrite resources
    IDWriteTextFormat* TextFormat = nullptr;
    IDWriteFactory* DWriteFactory = nullptr;
    ID2D1Factory1* D2DFactory = nullptr;
    ID2D1Device* D2DDevice = nullptr;
    ID2D1DeviceContext* D2DContext = nullptr;
    ID2D1SolidColorBrush* TextBrush = nullptr;
};
