#include "pch.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"

#include "Component/Light/Public/AmbientLightComponent.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Texture/Public/Texture.h"

FStaticMeshPass::FStaticMeshPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11Buffer* InConstantBufferModel,
	ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11PixelShader* InPSWithNormalMap, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline, InConstantBufferCamera, InConstantBufferModel), VS(InVS), PS(InPS), PSWithNormalMap(InPSWithNormalMap), InputLayout(InLayout), DS(InDS)
{
	ConstantBufferMaterial = FRenderResourceFactory::CreateConstantBuffer<FMaterialConstants>();
	ConstantBufferTiledLighting = FRenderResourceFactory::CreateConstantBuffer<FTiledLightingParams>();
}

void FStaticMeshPass::PreExecute(FRenderingContext& Context)
{
	const auto& Renderer = URenderer::GetInstance();
	const auto& DeviceResources = Renderer.GetDeviceResources();
	ID3D11RenderTargetView* RTV = DeviceResources->GetSceneColorRenderTargetView();	

	ID3D11RenderTargetView* RTVs[2] = { RTV, DeviceResources->GetNormalRenderTargetView() };
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
	Pipeline->SetRenderTargets(2, RTVs, DSV);

	// TODO : Set Normal map version
	ID3D11PixelShader* InPS = Renderer.GetPixelShaderForLightingModel(false);
	ID3D11VertexShader* InVS = Renderer.GetVertexShaderForLightingModel();
	PS = InPS;
	VS = InVS;
}

void FStaticMeshPass::Execute(FRenderingContext& Context)
{
	if (!(Context.ShowFlags & EEngineShowFlags::SF_StaticMesh)) { return; }

	FRenderState RenderState = UStaticMeshComponent::GetClassDefaultRenderState();
	if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderState.CullMode = ECullMode::None;
		RenderState.FillMode = EFillMode::WireFrame;
	}
	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(RenderState);
	// Set a default sampler to slot 0 to ensure one is always bound
	Pipeline->SetSamplerState(0, false, URenderer::GetInstance().GetDefaultSampler());
	Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferCamera);
	Pipeline->SetConstantBuffer(1, false, ConstantBufferCamera);

	// Tiled Lighting 설정을 Execute 시작 시 미리 바인딩
	SetUpTiledLighting(Context);
	BindTiledLightingBuffers();
	
	// Sort Static Mesh Components
	if (!(Context.ShowFlags & EEngineShowFlags::SF_StaticMesh)) { return; }
	TArray<UStaticMeshComponent*>& MeshComponents = Context.StaticMeshes;
	sort(MeshComponents.begin(), MeshComponents.end(),
		[](UStaticMeshComponent* A, UStaticMeshComponent* B) {
			int32 MeshA = A->GetStaticMesh() ? A->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			int32 MeshB = B->GetStaticMesh() ? B->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			return MeshA < MeshB;
		});

	FStaticMesh* CurrentMeshAsset = nullptr;
	UMaterial* CurrentMaterial = nullptr;

	// --- RTVs Setup End ---

	for (UStaticMeshComponent* MeshComp : MeshComponents) 
	{
		if (!MeshComp->GetStaticMesh()) { continue; }
		FStaticMesh* MeshAsset = MeshComp->GetStaticMesh()->GetStaticMeshAsset();
		if (!MeshAsset) { continue; }

		if (CurrentMeshAsset != MeshAsset)
		{
			Pipeline->SetVertexBuffer(MeshComp->GetVertexBuffer(), sizeof(FNormalVertex));
			Pipeline->SetIndexBuffer(MeshComp->GetIndexBuffer(), 0);
			CurrentMeshAsset = MeshAsset;
		}
		
		FModelConstants ModelConstants{ MeshComp->GetWorldTransformMatrix(), MeshComp->GetWorldTransformMatrixInverse().Transpose() };
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, ModelConstants);
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

		if (MeshAsset->MaterialInfo.empty() || MeshComp->GetStaticMesh()->GetNumMaterials() == 0)
		{
			// Material이 없어도 파이프라인은 설정해야 함
			FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, PS, nullptr, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
			Pipeline->UpdatePipeline(PipelineInfo);

			// 기본 Material 상수 설정
			FMaterialConstants MaterialConstants = {};
			MaterialConstants.Kd = FVector4(0.5f, 0.5f, 0.5f, 1.0f);
			MaterialConstants.MaterialFlags = 0;
			FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferMaterial, MaterialConstants);
			Pipeline->SetConstantBuffer(2, false, ConstantBufferMaterial);
			Pipeline->SetConstantBuffer(2, true, ConstantBufferMaterial);

			Pipeline->DrawIndexed(MeshAsset->Indices.size(), 0, 0);
			continue;
		}

		if (MeshComp->IsScrollEnabled()) 
		{
			MeshComp->SetElapsedTime(MeshComp->GetElapsedTime() + UTimeManager::GetInstance().GetDeltaTime());
		}

		for (const FMeshSection& Section : MeshAsset->Sections)
		{
			UMaterial* Material = MeshComp->GetMaterial(Section.MaterialSlot);
			if (CurrentMaterial != Material) 
			{
				// Select appropriate pixel shader based on normal map presence
				ID3D11PixelShader* SelectedPS = (Material->GetNormalTexture()) ? PSWithNormalMap : PS;
				FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, SelectedPS, nullptr, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
				Pipeline->UpdatePipeline(PipelineInfo);

				FMaterialConstants MaterialConstants = CreateMaterialConstants(Material, MeshComp);
				FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferMaterial, MaterialConstants);
				Pipeline->SetConstantBuffer(2, false, ConstantBufferMaterial);
				Pipeline->SetConstantBuffer(2, true, ConstantBufferMaterial);

				BindMaterialTextures(Material);

				CurrentMaterial = Material;
			}
			Pipeline->DrawIndexed(Section.IndexCount, Section.StartIndex, 0);
		}
	}
}

void FStaticMeshPass::PostExecute(FRenderingContext& Context)
{
	// Material Texture 언바인딩
	Pipeline->SetTexture(0, false, nullptr);
	Pipeline->SetTexture(1, false, nullptr);
	Pipeline->SetTexture(2, false, nullptr);
	Pipeline->SetTexture(3, false, nullptr);
	Pipeline->SetTexture(4, false, nullptr);
	Pipeline->SetTexture(5, false, nullptr);

	// Tiled Lighting Structured Buffer SRV 언바인딩 (VS와 PS 모두)
	Pipeline->SetTexture(13, true, nullptr);
	Pipeline->SetTexture(13, false, nullptr);
	Pipeline->SetTexture(14, true, nullptr);
	Pipeline->SetTexture(14, false, nullptr);
	Pipeline->SetTexture(15, true, nullptr);
	Pipeline->SetTexture(15, false, nullptr);
}

void FStaticMeshPass::Release()
{
	SafeRelease(ConstantBufferMaterial);
}

FMaterialConstants FStaticMeshPass::CreateMaterialConstants(UMaterial* Material, UStaticMeshComponent* MeshComp)
{
	FMaterialConstants Constants = {};
	Constants.Ka = FVector4(Material->GetAmbientColor(), 1.0f);
	Constants.Kd = FVector4(Material->GetDiffuseColor(), 1.0f);
	Constants.Ks = FVector4(Material->GetSpecularColor(), 1.0f);
	Constants.Ns = Material->GetSpecularExponent();
	Constants.Ni = Material->GetRefractionIndex();
	Constants.D	 = Material->GetDissolveFactor();
	Constants.Time = MeshComp->GetElapsedTime();

	Constants.MaterialFlags = 0;
	if (Material->GetDiffuseTexture())	Constants.MaterialFlags |= HAS_DIFFUSE_MAP;
	if (Material->GetAmbientTexture())	Constants.MaterialFlags |= HAS_AMBIENT_MAP;
	if (Material->GetSpecularTexture())	Constants.MaterialFlags |= HAS_SPECULAR_MAP;
	if (Material->GetNormalTexture())	Constants.MaterialFlags |= HAS_NORMAL_MAP;
	if (Material->GetAlphaTexture())	Constants.MaterialFlags |= HAS_ALPHA_MAP;
	if (Material->GetBumpTexture())		Constants.MaterialFlags |= HAS_BUMP_MAP;

	return Constants;
}

void FStaticMeshPass::BindMaterialTextures(UMaterial* Material)
{
	auto Bind = [&](int Slot, UTexture* Tex, bool bSetSampler = false) {
		if (Tex)
		{
			Pipeline->SetTexture(Slot, false, Tex->GetTextureSRV());
			if (bSetSampler) {
				Pipeline->SetSamplerState(Slot, false, Tex->GetTextureSampler());
			}
		}
	};

	Bind(0, Material->GetDiffuseTexture(), true);
	Bind(1, Material->GetAmbientTexture());
	Bind(2, Material->GetSpecularTexture());
	Bind(3, Material->GetNormalTexture());
	Bind(4, Material->GetAlphaTexture());
}

void FStaticMeshPass::SetUpTiledLighting(const FRenderingContext& Context)
{
	// Tiled Lighting용 cbuffer 설정
	FTiledLightingParams tiledParams = {};
	tiledParams.ViewportOffset[0] = static_cast<uint32>(Context.Viewport.TopLeftX);
	tiledParams.ViewportOffset[1] = static_cast<uint32>(Context.Viewport.TopLeftY);
	tiledParams.ViewportSize[0] = static_cast<uint32>(Context.Viewport.Width);
	tiledParams.ViewportSize[1] = static_cast<uint32>(Context.Viewport.Height);
	tiledParams.NumLights = static_cast<uint32>(Context.Lights.size());  // Gouraud용 전체 라이트 개수
	tiledParams._padding = 0;

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferTiledLighting, tiledParams);
	Pipeline->SetConstantBuffer(3, false, ConstantBufferTiledLighting);
	Pipeline->SetConstantBuffer(3, true, ConstantBufferTiledLighting);
}

void FStaticMeshPass::BindTiledLightingBuffers()
{
	// UberShader에서 사용할 Light Culling Structured Buffer SRV 바인딩
	const auto& Renderer = URenderer::GetInstance();

	// AllLights: t13, LightIndexBuffer: t14, TileLightInfo: t15
	// VS와 PS 모두에 바인딩 (Gouraud 모드에서는 VS에서 라이팅 계산)
	Pipeline->SetTexture(13, true, Renderer.GetAllLightsSRV());
	Pipeline->SetTexture(13, false, Renderer.GetAllLightsSRV());
	Pipeline->SetTexture(14, true, Renderer.GetLightIndexBufferSRV());
	Pipeline->SetTexture(14, false, Renderer.GetLightIndexBufferSRV());
	Pipeline->SetTexture(15, true, Renderer.GetTileLightInfoSRV());
	Pipeline->SetTexture(15, false, Renderer.GetTileLightInfoSRV());
}
