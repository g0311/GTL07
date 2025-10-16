#include "pch.h"
#include "Component/Public/PointLightComponent.h"
#include "Editor/Public/Camera.h"
#include "Render/RenderPass/Public/PointLightPass.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

FPointLightPass::FPointLightPass(UPipeline* InPipeline,
    ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout,
    ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS)
        : FRenderPass(InPipeline, nullptr, nullptr), VS(InVS), PS(InPS), InputLayout(InLayout), DS(InDS), BS(InBS)
{
    FNormalVertex NormalVertices[3] = {};
    NormalVertices[0].Position = { -1.0f, -1.0f, 0.0f };
    NormalVertices[1].Position = { 3.0f, -1.0f, 0.0f };
    NormalVertices[2].Position = { -1.0f, 3.0f, 0.0f };
    
    VertexBuffer = FRenderResourceFactory::CreateVertexBuffer(NormalVertices, sizeof(NormalVertices));
    ConstantBufferPerFrame = FRenderResourceFactory::CreateConstantBuffer<FPointLightPerFrame>();
    ConstantBufferPointLightData = FRenderResourceFactory::CreateConstantBuffer<FPointLightData>();
    PointLightSampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
}

void FPointLightPass::Execute(FRenderingContext& Context)
{
    if (Context.ViewMode != EViewModeIndex::VMI_Lit)
    {
        return;
    }
    
	const auto& Renderer = URenderer::GetInstance();
    const auto& DeviceResources = Renderer.GetDeviceResources();
    ID3D11RenderTargetView* RTV = nullptr;
    RTV = DeviceResources->GetSceneColorRenderTargetView();	

    ID3D11RenderTargetView* RTVs[1] = { RTV };
    Pipeline->SetRenderTargets(1, RTVs, nullptr);
    auto RS = FRenderResourceFactory::GetRasterizerState( { ECullMode::None, EFillMode::Solid }); 

    FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, PS, BS, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST};
    Pipeline->UpdatePipeline(PipelineInfo);
    Pipeline->SetVertexBuffer(VertexBuffer, sizeof(FNormalVertex));

    Pipeline->SetTexture(1, false, DeviceResources->GetNormalSRV());
    Pipeline->SetTexture(2, false, DeviceResources->GetDepthStencilSRV());
    Pipeline->SetSamplerState(0, false, PointLightSampler);

    for (auto PointLight : Context.PointLights)
    {
        // if (!PointLight || !PointLight->IsVisible()) { continue; }
        if (!PointLight) { continue; }

        auto ViewProjConstantsInverse = Context.CurrentCamera->GetFViewProjConstantsInverse();
        
        FPointLightPerFrame PointLightPerFrame = {};
        PointLightPerFrame.InvView = ViewProjConstantsInverse.View;
        PointLightPerFrame.InvProjection = ViewProjConstantsInverse.Projection;
        PointLightPerFrame.CameraLocation = Context.CurrentCamera->GetLocation();
        
        const D3D11_VIEWPORT& ViewportInfo = Context.Viewport;
        PointLightPerFrame.Viewport = FVector4(ViewportInfo.TopLeftX, ViewportInfo.TopLeftY, ViewportInfo.Width, ViewportInfo.Height);
        PointLightPerFrame.RenderTargetSize = FVector2(Context.RenderTargetSize.X, Context.RenderTargetSize.Y);
        
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPerFrame, PointLightPerFrame);
        Pipeline->SetConstantBuffer(0, false, ConstantBufferPerFrame);
        
        FPointLightData PointLightData = {};
        PointLightData.LightLocation = PointLight->GetWorldLocation();
        PointLightData.LightIntensity = PointLight->GetIntensity();
        PointLightData.LightColor = PointLight->GetLightColor();
        PointLightData.LightRadius = PointLight->GetSourceRadius();
        PointLightData.LightFalloffExtent = PointLight->GetLightFalloffExtent();

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPointLightData, PointLightData);
        Pipeline->SetConstantBuffer(1, false, ConstantBufferPointLightData);

        Pipeline->Draw(3, 0);
    }
    Pipeline->SetTexture(1, false, nullptr);
    Pipeline->SetTexture(2, false, nullptr);

    ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
    ID3D11RenderTargetView* NormalRTV = DeviceResources->GetNormalRenderTargetView();

    ID3D11RenderTargetView* NRTVs[] = { DeviceResources->GetSceneColorRenderTargetView(), NormalRTV };
    Pipeline->SetRenderTargets(2, NRTVs, DSV);
}

void FPointLightPass::Release()
{
    SafeRelease(VertexBuffer);
    SafeRelease(ConstantBufferPerFrame);
    SafeRelease(ConstantBufferPointLightData);
    SafeRelease(PointLightSampler);
}