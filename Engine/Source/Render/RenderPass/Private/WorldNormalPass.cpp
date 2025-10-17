#include "pch.h"
#include "Render/RenderPass/Public/WorldNormalPass.h"

#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

FWorldNormalPass::FWorldNormalPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline, InConstantBufferCamera, nullptr), DS(InDS)
{
	TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDesc = {};

	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/WorldNormalShader.hlsl", LayoutDesc, &VertexShader, nullptr);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/WorldNormalShader.hlsl", &PixelShader);

	SamplerState = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
	ConstantBufferPerFrame = FRenderResourceFactory::CreateConstantBuffer<FWorldNormalConstants>();
}

void FWorldNormalPass::PreExecute(FRenderingContext& Context)
{
	if (Context.ViewMode != EViewModeIndex::VMI_WorldNormal) { return; }

	const auto& Renderer = URenderer::GetInstance();
	const auto& DeviceResources = Renderer.GetDeviceResources();

	// Unbind all RTVs first to allow reading NormalSRV
	// This is necessary because StaticMeshPass binds NormalRTV, and we can't read from a bound RTV
	Pipeline->SetRenderTargets(0, nullptr, nullptr);

	// Render directly to FrameBuffer using Linear RTV (UNORM format, no sRGB conversion)
	// WorldNormal data is directional vectors, not colors, so must remain linear
	ID3D11RenderTargetView* RTV = DeviceResources->GetFrameBufferLinearRTV();
	Pipeline->SetRenderTargets(1, &RTV, nullptr);
}

void FWorldNormalPass::Execute(FRenderingContext& Context)
{
	if (Context.ViewMode != EViewModeIndex::VMI_WorldNormal) { return; }

	const auto& Renderer = URenderer::GetInstance();
	const auto& DeviceResources = Renderer.GetDeviceResources();
	auto RS = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });

	// Disable blending - we want opaque output
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

	FPipelineInfo PipelineInfo = { nullptr, VertexShader, RS, nullptr, PixelShader, nullptr, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
	Pipeline->UpdatePipeline(PipelineInfo);

	FWorldNormalConstants WorldNormalConstants;
	WorldNormalConstants.RenderTargetSize = FVector2(Context.RenderTargetSize.X, Context.RenderTargetSize.Y);
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPerFrame, WorldNormalConstants);
	Pipeline->SetConstantBuffer(1, false, ConstantBufferPerFrame);
	Pipeline->SetTexture(0, false, DeviceResources->GetNormalSRV());
	Pipeline->SetSamplerState(0, false, SamplerState);

	Pipeline->Draw(3, 0);
}

void FWorldNormalPass::PostExecute(FRenderingContext& Context)
{
	Pipeline->SetTexture(0, false, nullptr);
}

void FWorldNormalPass::Release()
{
	SafeRelease(PixelShader);
	SafeRelease(VertexShader);
	SafeRelease(SamplerState);
	SafeRelease(ConstantBufferPerFrame);
}
