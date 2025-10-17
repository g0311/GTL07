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

void FFogPass::PreExecute(FRenderingContext& Context)
{
    const auto& Renderer = URenderer::GetInstance();
    const auto& DeviceResources = Renderer.GetDeviceResources();
    ID3D11RenderTargetView* RTV = DeviceResources->GetSceneColorRenderTargetView();	
    ID3D11RenderTargetView* RTVs[2] = { RTV, DeviceResources->GetNormalRenderTargetView() };
    Pipeline->SetRenderTargets(2, RTVs, nullptr);
}

void FFogPass::Execute(FRenderingContext& Context)
{
    TIME_PROFILE(FogPass)

    if (!(Context.ShowFlags & EEngineShowFlags::SF_Fog)) return;

    //--- Get Renderer Singleton ---//
    URenderer& Renderer = URenderer::GetInstance();
    
    // --- Set Pipeline State --- //
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }),
        DS_Read, PS, BlendState,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST};
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
}

void FFogPass::PostExecute(FRenderingContext& Context)
{
    Pipeline->SetTexture(0, false, nullptr);
}

void FFogPass::Release()
{
    SafeRelease(ConstantBufferFog);
    SafeRelease(ConstantBufferCameraInverse);
    SafeRelease(ConstantBufferViewportInfo);
}
