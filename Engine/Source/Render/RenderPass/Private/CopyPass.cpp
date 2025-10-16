#include "pch.h"
#include "Render/RenderPass/Public/CopyPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/DeviceResources.h"

FCopyPass::FCopyPass(
    UPipeline* InPipeline,
    UDeviceResources* InDeviceResources,
    ID3D11VertexShader* InVS,
    ID3D11PixelShader* InPS,
    ID3D11InputLayout* InLayout,
    ID3D11SamplerState* InSampler
)
    : FRenderPass(InPipeline, nullptr, nullptr)
    , DeviceResources(InDeviceResources)
    , VertexShader(InVS)
    , PixelShader(InPS)
    , InputLayout(InLayout)
    , SamplerState(InSampler)
{
    ViewportBuffer = FRenderResourceFactory::CreateConstantBuffer<FCopyViewportConstants>();
}

FCopyPass::~FCopyPass()
{
    Release();
}

void FCopyPass::Release()
{
    SafeRelease(ViewportBuffer);
}

void FCopyPass::Execute(FRenderingContext& Context)
{
    if (Context.ShowFlags & EEngineShowFlags::SF_FXAA) return;

    ID3D11ShaderResourceView* SceneSRV = DeviceResources->GetSceneColorShaderResourceView();
    if (!SceneSRV) return;

    // Update viewport constant buffer
    FCopyViewportConstants ViewportConstants;
    ViewportConstants.RenderTargetSize = { Context.RenderTargetSize.X, Context.RenderTargetSize.Y };
    FRenderResourceFactory::UpdateConstantBufferData(ViewportBuffer, ViewportConstants);

    // Set Render Target to Back Buffer
    ID3D11RenderTargetView* RTV = DeviceResources->GetFrameBufferRTV();
    DeviceResources->GetDeviceContext()->OMSetRenderTargets(1, &RTV, nullptr);

    // Set Pipeline
    FPipelineInfo PipelineInfo = {};
    PipelineInfo.InputLayout = InputLayout;
    PipelineInfo.VertexShader = VertexShader;
    PipelineInfo.PixelShader = PixelShader;
    PipelineInfo.DepthStencilState = nullptr;
    PipelineInfo.RasterizerState = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });
    PipelineInfo.BlendState = nullptr;
    PipelineInfo.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    Pipeline->UpdatePipeline(PipelineInfo);

    // Set Resources
    Pipeline->SetConstantBuffer(1, false, ViewportBuffer); // Bind the viewport buffer
    Pipeline->SetTexture(0, false, SceneSRV);
    Pipeline->SetSamplerState(0, false, SamplerState);

    // Draw
    Pipeline->Draw(3, 0);

    // Unbind
    Pipeline->SetTexture(0, false, nullptr);
}