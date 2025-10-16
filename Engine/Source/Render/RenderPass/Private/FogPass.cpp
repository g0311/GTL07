#include "pch.h"
#include "Render/RenderPass/Public/FogPass.h"

#include "Component/Public/HeightFogComponent.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

FFogPass::FFogPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj,
                   ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS_Read,
                   ID3D11BlendState* InBlendState)
        : FRenderPass(InPipeline, InConstantBufferViewProj, nullptr),
            VS(InVS), PS(InPS), InputLayout(InLayout), DS_Read(InDS_Read), BlendState(InBlendState)
{
    ConstantBufferFog = FRenderResourceFactory::CreateConstantBuffer<FFogConstants>();
    ConstantBufferCameraInverse = FRenderResourceFactory::CreateConstantBuffer<FCameraInverseConstants>();
    ConstantBufferViewportInfo = FRenderResourceFactory::CreateConstantBuffer<FViewportConstants>();
}

void FFogPass::Execute(FRenderingContext& Context)
{
    TIME_PROFILE(FogPass)

    if (!(Context.ShowFlags & EEngineShowFlags::SF_Fog)) return;

    //--- Get Renderer Singleton ---//
    URenderer& Renderer = URenderer::GetInstance();

    //--- Detatch DSV from GPU ---//
    ID3D11RenderTargetView* RTV;
    if (!(Context.ShowFlags & EEngineShowFlags::SF_FXAA))
    {
        RTV = Renderer.GetDeviceResources()->GetRenderTargetView();
    }
    else
    {
        RTV = Renderer.GetDeviceResources()->GetSceneColorRenderTargetView();
    }
    auto* DSV = Renderer.GetDeviceResources()->GetDepthStencilView();
    Renderer.GetDeviceContext()->OMSetRenderTargets(1, &RTV, nullptr);
    
    // --- Set Pipeline State --- //
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }),
        DS_Read, PS, BlendState };
    Pipeline->UpdatePipeline(PipelineInfo);
    
    // --- Draw Fog --- //
    for (UHeightFogComponent* Fog : Context.Fogs)
    {
        // Update Fog Constant Buffer (Slot 0)
        FFogConstants FogConstant;
        FVector color3 = Fog->GetFogInscatteringColor();
        FogConstant.FogColor = FVector4(color3.X, color3.Y, color3.Z, 1.0f);
        FogConstant.FogDensity = Fog->GetFogDensity();
        FogConstant.FogHeightFalloff = Fog->GetFogHeightFalloff();
        FogConstant.StartDistance = Fog->GetStartDistance();
        FogConstant.FogCutoffDistance = Fog->GetFogCutoffDistance();
        FogConstant.FogMaxOpacity = Fog->GetFogMaxOpacity();
        FogConstant.FogZ = Fog->GetWorldLocation().Z;
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferFog, FogConstant);
        Pipeline->SetConstantBuffer(0, false, ConstantBufferFog);

        // Update CameraInverse Constant Buffer (Slot 1)
        FCameraInverseConstants CameraInverseConstants;
        FCameraConstants ViewProjConstants = Context.CurrentCamera->GetFViewProjConstantsInverse();
        CameraInverseConstants.ProjectionInverse =  ViewProjConstants.Projection;
        CameraInverseConstants.ViewInverse =  ViewProjConstants.View;
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferCameraInverse, CameraInverseConstants);
        Pipeline->SetConstantBuffer(1, false, ConstantBufferCameraInverse);

        // Update ViewportInfo Constant Buffer (Slot 2)
        FViewportConstants ViewportConstants;
        ViewportConstants.RenderTargetSize = { Context.RenderTargetSize.X, Context.RenderTargetSize.Y };
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewportInfo, ViewportConstants);
        Pipeline->SetConstantBuffer(2, false, ConstantBufferViewportInfo);

        // Set Resources
        Pipeline->SetTexture(0, false, Renderer.GetDepthSRV());
        Pipeline->SetSamplerState(0, false, Renderer.GetDefaultSampler());

        Pipeline->Draw(3,0);
    }
    Pipeline->SetTexture(0, false, nullptr);
    
    Renderer.GetDeviceContext()->OMSetRenderTargets(1, &RTV, DSV);
}

void FFogPass::Release()
{
    SafeRelease(ConstantBufferFog);
    SafeRelease(ConstantBufferCameraInverse);
    SafeRelease(ConstantBufferViewportInfo);
}
