#include "pch.h"
#include "Render/RenderPass/Public/FXAAPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/DeviceResources.h"

FFXAAPass::FFXAAPass(UPipeline* InPipeline, UDeviceResources* InDeviceResources, ID3D11VertexShader* InVS,
    ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11SamplerState* InSampler)
    :FRenderPass(InPipeline,nullptr,nullptr)
    , DeviceResources(InDeviceResources)
    , VertexShader(InVS)
    , PixelShader(InPS)
    , InputLayout(InLayout)
    , SamplerState(InSampler)
{
    FXAAConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FFXAAConstants>();
    FXAAViewportBuffer = FRenderResourceFactory::CreateConstantBuffer<FFXAAViewportConstants>();
}

FFXAAPass::~FFXAAPass()
{
    Release();
}

void FFXAAPass::PreExecute(FRenderingContext& Context)
{
    // Set Render Target to Back Buffer
    ID3D11RenderTargetView* RTV = DeviceResources->GetFrameBufferRTV(); // 스왑체인 RTV
    Pipeline->SetRenderTargets(1, &RTV, nullptr);
}

void FFXAAPass::Execute(FRenderingContext& Context)
{
    ID3D11ShaderResourceView* SceneSRV = DeviceResources->GetSceneColorShaderResourceView(); // 오프스크린 컬러입력
    if (!SceneSRV ) return;
    if (!(Context.ShowFlags & EEngineShowFlags::SF_FXAA)) return;

    // Skip FXAA in WorldNormal mode - WorldNormalPass renders directly to FrameBuffer
    if (Context.ViewMode == EViewModeIndex::VMI_WorldNormal) return;

    UpdateConstants(Context);

    FPipelineInfo PipelineInfo = {};
    PipelineInfo.InputLayout = InputLayout;
    PipelineInfo.VertexShader = VertexShader;
    PipelineInfo.PixelShader = PixelShader;
    PipelineInfo.DepthStencilState = nullptr;
    PipelineInfo.RasterizerState = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });
    PipelineInfo.BlendState = nullptr;
    PipelineInfo.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    Pipeline->UpdatePipeline(PipelineInfo);
    
    Pipeline->SetConstantBuffer(0, false, FXAAConstantBuffer);
    Pipeline->SetConstantBuffer(1, false, FXAAViewportBuffer);
    
    Pipeline->SetTexture(0, false, SceneSRV);
    Pipeline->SetSamplerState(0, false, SamplerState);

    Pipeline->Draw(3, 0);
}

void FFXAAPass::PostExecute(FRenderingContext& Context)
{
    // 정리
    Pipeline->SetTexture(0, false, nullptr);
}

void FFXAAPass::Release()
{
    SafeRelease(FXAAConstantBuffer);
    SafeRelease(FXAAViewportBuffer);
}

void FFXAAPass::UpdateConstants(FRenderingContext& Context)
{
    const D3D11_VIEWPORT& VP = DeviceResources->GetViewportInfo();
    FXAAParams.InvResolution = FVector2(1.0f / VP.Width, 1.0f / VP.Height);
    // FXAA 품질 설정값을 명시적으로 업데이트                                           
    FXAAParams.FXAASpanMax = 8.0f;                                        
    FXAAParams.FXAAReduceMul = 1.0f / 8.0f;                               
    FXAAParams.FXAAReduceMin = 1.0f / 128.0f;
    FRenderResourceFactory::UpdateConstantBufferData(FXAAConstantBuffer, FXAAParams);

    FFXAAViewportConstants ViewportConstants;
    ViewportConstants.RenderTargetSize = { Context.RenderTargetSize.X, Context.RenderTargetSize.Y };
    FRenderResourceFactory::UpdateConstantBufferData(FXAAViewportBuffer, ViewportConstants);
}
