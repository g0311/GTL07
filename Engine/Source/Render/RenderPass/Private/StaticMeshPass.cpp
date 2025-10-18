#include "pch.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Texture/Public/Texture.h"

FStaticMeshPass::FStaticMeshPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11Buffer* InConstantBufferModel,
	ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11PixelShader* InPSWithNormalMap, ID3D11PixelShader* InPSWithNormalAndHeightMap, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline, InConstantBufferCamera, InConstantBufferModel), VS(InVS), PS(InPS), PSWithNormalMap(InPSWithNormalMap), PSWithNormalAndHeightMap(InPSWithNormalAndHeightMap), InputLayout(InLayout), DS(InDS)
{
	ConstantBufferMaterial = FRenderResourceFactory::CreateConstantBuffer<FMaterialConstants>();
}

void FStaticMeshPass::PreExecute(FRenderingContext& Context)
{
	const auto& Renderer = URenderer::GetInstance();
	const auto& DeviceResources = Renderer.GetDeviceResources();
	ID3D11RenderTargetView* RTV = DeviceResources->GetSceneColorRenderTargetView();	

	ID3D11RenderTargetView* RTVs[2] = { RTV, DeviceResources->GetNormalRenderTargetView() };
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
	Pipeline->SetRenderTargets(2, RTVs, DSV);
}

void FStaticMeshPass::Execute(FRenderingContext& Context)
{
	FRenderState RenderState = UStaticMeshComponent::GetClassDefaultRenderState();
	if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderState.CullMode = ECullMode::None; RenderState.FillMode = EFillMode::WireFrame;
	}
	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(RenderState);

	// Set a default sampler to slot 0 to ensure one is always bound
	Pipeline->SetSamplerState(0, false, URenderer::GetInstance().GetDefaultSampler());

	Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferCamera);
	Pipeline->SetConstantBuffer(1, false, ConstantBufferCamera);
	
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
			if (CurrentMaterial != Material) {
				// Check which texture maps are available
				bool bHasNormalMap = Material->GetNormalTexture() != nullptr;
				bool bHasHeightMap = Material->GetHeightTexture() != nullptr;

				// Initialize material constants
				FMaterialConstants MaterialConstants = {};
				FVector AmbientColor = Material->GetAmbientColor(); MaterialConstants.Ka = FVector4(AmbientColor.X, AmbientColor.Y, AmbientColor.Z, 1.0f);
				FVector DiffuseColor = Material->GetDiffuseColor(); MaterialConstants.Kd = FVector4(DiffuseColor.X, DiffuseColor.Y, DiffuseColor.Z, 1.0f);
				FVector SpecularColor = Material->GetSpecularColor(); MaterialConstants.Ks = FVector4(SpecularColor.X, SpecularColor.Y, SpecularColor.Z, 1.0f);
				MaterialConstants.Ns = Material->GetSpecularExponent();
				MaterialConstants.Ni = Material->GetRefractionIndex();
				MaterialConstants.D = Material->GetDissolveFactor();
				MaterialConstants.MaterialFlags = 0;
				if (Material->GetDiffuseTexture())  { MaterialConstants.MaterialFlags |= HAS_DIFFUSE_MAP; }
				if (Material->GetAmbientTexture())  { MaterialConstants.MaterialFlags |= HAS_AMBIENT_MAP; }
				if (Material->GetSpecularTexture()) { MaterialConstants.MaterialFlags |= HAS_SPECULAR_MAP; }
				if (Material->GetAlphaTexture())    { MaterialConstants.MaterialFlags |= HAS_ALPHA_MAP; }

				// Set normal map and height map flags
				// The appropriate shader variant will be selected based on these flags
				if (bHasNormalMap)
				{
					MaterialConstants.MaterialFlags |= HAS_NORMAL_MAP;
				}
				if (bHasHeightMap)
				{
					MaterialConstants.MaterialFlags |= HAS_HEIGHT_MAP;
				}
				MaterialConstants.Time = MeshComp->GetElapsedTime();
				MaterialConstants.HeightScale = 0.2f;  // Default Parallax scale

				// Select appropriate pixel shader variant based on texture maps
				ID3D11PixelShader* SelectedPS = PS;  // Variant 0: Base (default)

				if (bHasNormalMap && bHasHeightMap)
				{
					SelectedPS = PSWithNormalAndHeightMap;  // Variant 2: Normal + Height
				}
				else if (bHasNormalMap)
				{
					SelectedPS = PSWithNormalMap;  // Variant 1: Normal only
				}
				// else: Use base PS (Variant 0)

				FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, SelectedPS, nullptr, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
				Pipeline->UpdatePipeline(PipelineInfo);

				FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferMaterial, MaterialConstants);
				Pipeline->SetConstantBuffer(2, false, ConstantBufferMaterial);

				if (UTexture* DiffuseTexture = Material->GetDiffuseTexture())
				{
					Pipeline->SetTexture(0, false, DiffuseTexture->GetTextureSRV());
					Pipeline->SetSamplerState(0, false, DiffuseTexture->GetTextureSampler());
				}
				if (UTexture* AmbientTexture = Material->GetAmbientTexture())
				{
					Pipeline->SetTexture(1, false, AmbientTexture->GetTextureSRV());
				}
				if (UTexture* SpecularTexture = Material->GetSpecularTexture())
				{
					Pipeline->SetTexture(2, false, SpecularTexture->GetTextureSRV());
				}
				if (UTexture* NormalTexture = Material->GetNormalTexture())
				{
					Pipeline->SetTexture(3, false, NormalTexture->GetTextureSRV());
				}
				if (UTexture* AlphaTexture = Material->GetAlphaTexture())
				{
					Pipeline->SetTexture(4, false, AlphaTexture->GetTextureSRV());
				}
				if (UTexture* HeightTexture = Material->GetHeightTexture())
				{
					Pipeline->SetTexture(5, false, HeightTexture->GetTextureSRV());
				}

				CurrentMaterial = Material;
			}
			Pipeline->DrawIndexed(Section.IndexCount, Section.StartIndex, 0);
		}
	}
}

void FStaticMeshPass::PostExecute(FRenderingContext& Context)
{
	Pipeline->SetTexture(0, false, nullptr);
	Pipeline->SetTexture(1, false, nullptr);
	Pipeline->SetTexture(2, false, nullptr);
	Pipeline->SetTexture(3, false, nullptr);
	Pipeline->SetTexture(4, false, nullptr);
	Pipeline->SetTexture(5, false, nullptr);
}

void FStaticMeshPass::Release()
{
	SafeRelease(ConstantBufferMaterial);
}
