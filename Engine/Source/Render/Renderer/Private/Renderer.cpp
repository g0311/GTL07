#include "pch.h"
#include "Component/Mesh/Public/StaticMesh.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Component/Public/HeightFogComponent.h"
#include "Component/Public/FakePointLightComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Viewport.h"
#include "Editor/Public/ViewportClient.h"
#include "Level/Public/Level.h"
#include "Manager/UI/Public/UIManager.h"
#include "Optimization/Public/OcclusionCuller.h"
#include "Render/RenderPass/Public/BillboardPass.h"
#include "Render/RenderPass/Public/DecalPass.h"
#include "Render/RenderPass/Public/CopyPass.h"
#include "Render/RenderPass/Public/FogPass.h"
#include "Render/RenderPass/Public/RenderPass.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"
#include "Render/RenderPass/Public/TextPass.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/RenderPass/Public/FXAAPass.h"
#include "Render/RenderPass/Public/LightCullingPass.h"
#include "Render/RenderPass/Public/LightCullingDebugPass.h"

#include "Render/RenderPass/Public/SceneDepthPass.h"
#include "Render/RenderPass/Public/WorldNormalPass.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"

IMPLEMENT_SINGLETON_CLASS(URenderer, UObject)

URenderer::URenderer() = default;

URenderer::~URenderer() = default;


void URenderer::Init(HWND InWindowHandle)
{
	DeviceResources = new UDeviceResources(InWindowHandle);
	Pipeline = new UPipeline(GetDeviceContext());
	ViewportClient = new FViewport();
	
	
	// 렌더링 상태 및 리소스 생성
	CreateDepthStencilState();
	CreateBlendState();
	CreateSamplerState();
	
	CreateDefaultShader();
	CreateTextureShader();
	CreateDecalShader();
	CreatePointLightShader();
	CreateFogShader();
	CreateCopyShader();
	CreateFXAAShader();
	CreateBillboardShader();

	CreateConstantBuffers();
	CreateLightBuffers();
	CreateLightCullBuffers();
	
	ViewportClient->InitializeLayout(DeviceResources->GetViewportInfo());

	FLightCullingPass* LightCullPass = new FLightCullingPass(Pipeline, DeviceResources);
	RenderPasses.push_back(LightCullPass);
	
	FStaticMeshPass* StaticMeshPass = new FStaticMeshPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		TextureVertexShader, TexturePixelShader, TexturePixelShaderWithNormalMap, TextureInputLayout, DefaultDepthStencilState);
	RenderPasses.push_back(StaticMeshPass);

	FDecalPass* DecalPass = new FDecalPass(Pipeline, ConstantBufferViewProj,
		DecalVertexShader, DecalPixelShader, DecalInputLayout, DecalDepthStencilState, AlphaBlendState);
	RenderPasses.push_back(DecalPass);
	
	FBillboardPass* BillboardPass = new FBillboardPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		BillboardVertexShader, BillboardPixelShader, BillboardInputLayout, DefaultDepthStencilState, AlphaBlendState);
	RenderPasses.push_back(BillboardPass);

	FTextPass* TextPass = new FTextPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels);
	RenderPasses.push_back(TextPass);

	FFogPass* FogPass = new FFogPass(Pipeline, ConstantBufferViewProj,
		FogVertexShader, FogPixelShader, FogInputLayout, DefaultDepthStencilState, AlphaBlendState);
	RenderPasses.push_back(FogPass);

	FSceneDepthPass* SceneDepthPass = new FSceneDepthPass(Pipeline, ConstantBufferViewProj, DisabledDepthStencilState);
	RenderPasses.push_back(SceneDepthPass);

	FWorldNormalPass* WorldNormalPass = new FWorldNormalPass(Pipeline, ConstantBufferViewProj, DisabledDepthStencilState);
	RenderPasses.push_back(WorldNormalPass);

	// Create final passes
	{
		CopyPass = new FCopyPass(Pipeline, DeviceResources, CopyVertexShader, CopyPixelShader, CopyInputLayout, CopySamplerState);
		RenderPasses.push_back(CopyPass);
	
		FXAAPass = new FFXAAPass(Pipeline, DeviceResources, FXAAVertexShader, FXAAPixelShader, FXAAInputLayout, FXAASamplerState);
		RenderPasses.push_back(FXAAPass);

		LightCullingDebugPass = new FLightCullingDebugPass(Pipeline, DeviceResources);
		RenderPasses.push_back(LightCullingDebugPass);
	}
}

void URenderer::Release()
{
	ReleaseConstantBuffers();
	ReleaseLightBuffers();
	ReleaseLightCullBuffers();
	
	ReleaseDefaultShader();
	ReleaseDepthStencilState();
	ReleaseBlendState();
	ReleaseSamplerState();
	FRenderResourceFactory::ReleaseRasterizerState();
	for (auto& RenderPass : RenderPasses)
	{
		RenderPass->Release();
		SafeDelete(RenderPass);
	}
	
	SafeDelete(ViewportClient);
	SafeDelete(Pipeline);
	SafeDelete(DeviceResources);
}

void URenderer::CreateDepthStencilState()
{
	// 3D Default Depth Stencil (Depth O, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DefaultDescription = {};
	DefaultDescription.DepthEnable = TRUE;
	DefaultDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DefaultDescription.DepthFunc = D3D11_COMPARISON_LESS;
	DefaultDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DefaultDescription, &DefaultDepthStencilState);

	// Decal Depth Stencil (Depth Read, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DecalDescription = {};
	DecalDescription.DepthEnable = TRUE;
	DecalDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DecalDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DecalDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DecalDescription, &DecalDepthStencilState);


	// Disabled Depth Stencil (Depth X, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DisabledDescription = {};
	DisabledDescription.DepthEnable = FALSE;
	DisabledDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DisabledDescription, &DisabledDepthStencilState);
}

void URenderer::CreateBlendState()
{
    // Alpha Blending
    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    GetDevice()->CreateBlendState(&BlendDesc, &AlphaBlendState);

    // Additive Blending
    D3D11_BLEND_DESC AdditiveBlendDesc = {};
    AdditiveBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    AdditiveBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    AdditiveBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    AdditiveBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    AdditiveBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    AdditiveBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    AdditiveBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    AdditiveBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    GetDevice()->CreateBlendState(&AdditiveBlendDesc, &AdditiveBlendState);
}

void URenderer::CreateSamplerState()
{
	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	GetDevice()->CreateSamplerState(&samplerDesc, &DefaultSampler);
}

void URenderer::CreateDefaultShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DefaultLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/SampleShader.hlsl", DefaultLayout, &DefaultVertexShader, &DefaultInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/SampleShader.hlsl", &DefaultPixelShader);
	Stride = sizeof(FNormalVertex);
}

void URenderer::CreateTextureShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> TextureLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Tangent), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Bitangent), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/UberLit.hlsl", TextureLayout, &TextureVertexShader, &TextureInputLayout);

	// Compile pixel shader without normal map (nullptr = no defines)
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", &TexturePixelShader, nullptr);

	// Compile pixel shader with normal map
	D3D_SHADER_MACRO NormalMapDefines[] =
	{
		{ "HAS_NORMAL_MAP", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", &TexturePixelShaderWithNormalMap, NormalMapDefines);
}

void URenderer::CreateDecalShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DecalLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/DecalShader.hlsl", DecalLayout, &DecalVertexShader, &DecalInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/DecalShader.hlsl", &DecalPixelShader);
}

void URenderer::CreatePointLightShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> PointLightLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/PointLightShader.hlsl", PointLightLayout, &PointLightVertexShader, &PointLightInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/PointLightShader.hlsl", &PointLightPixelShader);
}

void URenderer::CreateFogShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> FogLayout = {};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/HeightFogShader.hlsl", FogLayout, &FogVertexShader, &FogInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/HeightFogShader.hlsl", &FogPixelShader);
}

void URenderer::CreateCopyShader()
{
    // Shaders
    TArray<D3D11_INPUT_ELEMENT_DESC> CopyLayout = {};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/Copy.hlsl", CopyLayout, &CopyVertexShader, &CopyInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/Copy.hlsl", &CopyPixelShader);

    // Sampler
    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    GetDevice()->CreateSamplerState(&SamplerDesc, &CopySamplerState);
}

void URenderer::CreateFXAAShader()
{
    // Shaders
    TArray<D3D11_INPUT_ELEMENT_DESC> FXAALayout = {};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/FXAAShader.hlsl", FXAALayout, &FXAAVertexShader, &FXAAInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/FXAAShader.hlsl", &FXAAPixelShader);

    // Sampler
    FXAASamplerState = FRenderResourceFactory::CreateFXAASamplerState();
}

void URenderer::CreateBillboardShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> BillboardLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Tangent), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Bitangent), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/BillboardShader.hlsl", BillboardLayout, &BillboardVertexShader, &BillboardInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/BillboardShader.hlsl", &BillboardPixelShader);
}

void URenderer::ReleaseDefaultShader()
{
	SafeRelease(DefaultInputLayout);
	SafeRelease(DefaultPixelShader);
	SafeRelease(DefaultVertexShader);
	
	SafeRelease(TextureInputLayout);
	SafeRelease(TexturePixelShader);
	SafeRelease(TexturePixelShaderWithNormalMap);
	SafeRelease(TextureVertexShader);
	
	SafeRelease(DecalVertexShader);
	SafeRelease(DecalPixelShader);
	SafeRelease(DecalInputLayout);
	
	SafeRelease(PointLightVertexShader);
	SafeRelease(PointLightPixelShader);
	SafeRelease(PointLightInputLayout);
	
	SafeRelease(FogVertexShader);
	SafeRelease(FogPixelShader);
	SafeRelease(FogInputLayout);

	SafeRelease(CopyVertexShader);
	SafeRelease(CopyPixelShader);
	SafeRelease(CopyInputLayout);

	SafeRelease(FXAAVertexShader);
	SafeRelease(FXAAPixelShader);
	SafeRelease(FXAAInputLayout);

	SafeRelease(BillboardVertexShader);
	SafeRelease(BillboardPixelShader);
	SafeRelease(BillboardInputLayout);
}

void URenderer::ReleaseDepthStencilState()
{
	SafeRelease(DefaultDepthStencilState);
	SafeRelease(DecalDepthStencilState);
	SafeRelease(DisabledDepthStencilState);
	if (GetDeviceContext())
	{
		GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
	}
}

void URenderer::ReleaseBlendState()
{
    SafeRelease(AlphaBlendState);
	SafeRelease(AdditiveBlendState);
}

void URenderer::ReleaseSamplerState()
{
	SafeRelease(DefaultSampler);
	SafeRelease(CopySamplerState);
	SafeRelease(FXAASamplerState);
}

void URenderer::Update()
{
    RenderBegin();

    for (FViewportClient& ViewportClient : ViewportClient->GetViewports())
    {
        if (ViewportClient.GetViewportInfo().Width < 1.0f || ViewportClient.GetViewportInfo().Height < 1.0f) { continue; }

        ViewportClient.Apply(GetDeviceContext());

        UCamera* CurrentCamera = &ViewportClient.Camera;
        CurrentCamera->Update(ViewportClient.GetViewportInfo());
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, CurrentCamera->GetFViewProjConstants());
        Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
	    
	    {
        	TIME_PROFILE(RenderLevel)
			RenderLevel(ViewportClient);
	    }
	    {
        	TIME_PROFILE(RenderEditor)
			GEditor->GetEditorModule()->RenderEditor();
	    }
    	
        // Gizmo는 최종적으로 렌더
        GEditor->GetEditorModule()->RenderGizmo(CurrentCamera);
    }

    {
        TIME_PROFILE(UUIManager)
        UUIManager::GetInstance().Render();
    }
    {
        TIME_PROFILE(UStatOverlay)
        UStatOverlay::GetInstance().Render();
    }

    RenderEnd();
}

void URenderer::RenderBegin() const
{
	// Clear sRGB RTV (for normal rendering and UI)
	auto* RenderTargetView = DeviceResources->GetFrameBufferRTV();
	GetDeviceContext()->ClearRenderTargetView(RenderTargetView, ClearColor);

	// Clear Linear RTV (for WorldNormal mode)
	// Both RTVs point to the same backbuffer but with different format interpretations
	auto* LinearRenderTargetView = DeviceResources->GetFrameBufferLinearRTV();
	GetDeviceContext()->ClearRenderTargetView(LinearRenderTargetView, ClearColor);

	// Clear Normal buffer to (0.5, 0.5, 0.5, 0) - when decoded becomes (0,0,0) which represents invalid/no normals
	// Alpha = 0 to indicate no geometry rendered here
	float NormalClearColor[4] = { 0.5f, 0.5f, 0.5f, 0.0f };
	auto* NormalRenderTargetView = DeviceResources->GetNormalRenderTargetView();
	GetDeviceContext()->ClearRenderTargetView(NormalRenderTargetView, NormalClearColor);

	auto* DepthStencilView = DeviceResources->GetDepthStencilView();
	GetDeviceContext()->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	auto* SceneColorRenderTargetView = DeviceResources->GetSceneColorRenderTargetView();
	GetDeviceContext()->ClearRenderTargetView(SceneColorRenderTargetView, ClearColor);


    // Clear UAVs
    const UINT clearValues[4] = { 0, 0, 0, 0 };
    GetDeviceContext()->ClearUnorderedAccessViewUint(TileLightInfoUAV, clearValues);
    GetDeviceContext()->ClearUnorderedAccessViewUint(LightIndexBufferUAV, clearValues);

    DeviceResources->UpdateViewport();
}

void URenderer::RenderLevel(FViewportClient& InViewportClient)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel) { return; }

	// 오클루전 컬링
	TIME_PROFILE(Occlusion)
	// static COcclusionCuller Culler;
	const FCameraConstants& ViewProj = InViewportClient.Camera.GetFViewProjConstants();
	// Culler.InitializeCuller(ViewProj.View, ViewProj.Projection);
	TArray<UPrimitiveComponent*> FinalVisiblePrims = InViewportClient.Camera.GetViewVolumeCuller().GetRenderableObjects();
	// Culler.PerformCulling(
	// 	InCurrentCamera->GetViewVolumeCuller().GetRenderableObjects(),
	// 	InCurrentCamera->GetLocation()
	// );
	TIME_PROFILE_END(Occlusion)

	FRenderingContext RenderingContext(
		&ViewProj,
		&InViewportClient.Camera,
		GEditor->GetEditorModule()->GetViewMode(),
		CurrentLevel->GetShowFlags(),
		InViewportClient.ViewportInfo,
		{DeviceResources->GetViewportInfo().Width, DeviceResources->GetViewportInfo().Height}
		);
	// 1. Sort visible primitive components
	RenderingContext.AllPrimitives = FinalVisiblePrims;
	for (auto& Prim : FinalVisiblePrims)
	{
		if (auto StaticMesh = Cast<UStaticMeshComponent>(Prim))
		{
			RenderingContext.StaticMeshes.push_back(StaticMesh);
		}
		else if (auto BillBoard = Cast<UBillBoardComponent>(Prim))
		{
			RenderingContext.BillBoards.push_back(BillBoard);
		}
		else if (auto Text = Cast<UTextComponent>(Prim))
		{
			if (!Text->IsExactly(UUUIDTextComponent::StaticClass())) { RenderingContext.Texts.push_back(Text); }
			else { RenderingContext.UUIDs.push_back(Cast<UUUIDTextComponent>(Text)); }
		}
		else if (auto Decal = Cast<UDecalComponent>(Prim))
		{
			RenderingContext.Decals.push_back(Decal);
		}
	}

	for (const auto& Actor : CurrentLevel->GetLevelActors())
	{
		for (const auto& Component : Actor->GetOwnedComponents())
		{
			if (auto Fog = Cast<UHeightFogComponent>(Component))
			{
				RenderingContext.Fogs.push_back(Fog);
			}
			else if (auto Light = Cast<ULightComponent>(Component))
			{
				RenderingContext.Lights.push_back(Light);
			}
		}
	}

	for (auto RenderPass: RenderPasses)
	{
		RenderPass->PreExecute(RenderingContext);
		RenderPass->Execute(RenderingContext);
		RenderPass->PostExecute(RenderingContext);
	}
}

void URenderer::RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride, uint32 InIndexBufferStride)
{
    // Use the global stride if InStride is 0
    const uint32 FinalStride = (InStride == 0) ? Stride : InStride;

	auto* RenderTargetView = DeviceResources->GetFrameBufferRTV();
	ID3D11RenderTargetView* rtvs[] = { RenderTargetView };
	GetDeviceContext()->OMSetRenderTargets(1, rtvs, DeviceResources->GetDepthStencilView());
	
    // Allow for custom shaders, fallback to default
    FPipelineInfo PipelineInfo = {
        InPrimitive.InputLayout ? InPrimitive.InputLayout : DefaultInputLayout,
        InPrimitive.VertexShader ? InPrimitive.VertexShader : DefaultVertexShader,
		FRenderResourceFactory::GetRasterizerState(InRenderState),
        InPrimitive.bShouldAlwaysVisible ? DisabledDepthStencilState : DefaultDepthStencilState,
        InPrimitive.PixelShader ? InPrimitive.PixelShader : DefaultPixelShader,
        nullptr,
        InPrimitive.Topology
    };
    Pipeline->UpdatePipeline(PipelineInfo);

    // Update constant buffers
	FMatrix WorldMatrix = FMatrix::GetModelMatrix(InPrimitive.Location, InPrimitive.Rotation, InPrimitive.Scale);
	FMatrix WorldInverseTranspose = FMatrix::GetModelMatrixInverse(InPrimitive.Location, InPrimitive.Rotation, InPrimitive.Scale).Transpose();
	FModelConstants ModelConstants{ WorldMatrix, WorldInverseTranspose };
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels, ModelConstants);
	Pipeline->SetConstantBuffer(0, true, ConstantBufferModels);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
	
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, InPrimitive.Color);
	Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);
	Pipeline->SetConstantBuffer(2, true, ConstantBufferColor);
	
    Pipeline->SetVertexBuffer(InPrimitive.VertexBuffer, FinalStride);

    // The core logic: check for an index buffer
    if (InPrimitive.IndexBuffer && InPrimitive.NumIndices > 0)
    {
        Pipeline->SetIndexBuffer(InPrimitive.IndexBuffer, InIndexBufferStride);
        Pipeline->DrawIndexed(InPrimitive.NumIndices, 0, 0);
    }
    else
    {
        Pipeline->Draw(InPrimitive.NumVertices, 0);
    }
}

void URenderer::RenderEnd() const
{
	// Copy Pass Call !!
	TIME_PROFILE(DrawCall)
	GetSwapChain()->Present(0, 0);
	TIME_PROFILE_END(DrawCall)
}

void URenderer::OnResize(uint32 InWidth, uint32 InHeight)
{
    if (!DeviceResources || !GetDeviceContext() || !GetSwapChain()) return;

    DeviceResources->ReleaseSceneColorTarget();
	DeviceResources->ReleaseFrameBuffer();
	DeviceResources->ReleaseDepthBuffer();
	DeviceResources->ReleaseNormalBuffer();
	GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
	ReleaseLightCullBuffers();
	
    if (FAILED(GetSwapChain()->ResizeBuffers(2, InWidth, InHeight, DXGI_FORMAT_UNKNOWN, 0)))
    {
        UE_LOG("OnResize Failed");
        return;
    }
	DeviceResources->UpdateViewport();
	
    DeviceResources->CreateSceneColorTarget();
	DeviceResources->CreateFrameBuffer();
	DeviceResources->CreateDepthBuffer();
	DeviceResources->CreateNormalBuffer();
	CreateLightCullBuffers();

    ID3D11RenderTargetView* targetView = DeviceResources->GetSceneColorRenderTargetView();
    ID3D11RenderTargetView* targetViews[] = { targetView };
    GetDeviceContext()->OMSetRenderTargets(1, targetViews, DeviceResources->GetDepthStencilView());
}


void URenderer::CreateConstantBuffers()
{
	ConstantBufferModels = FRenderResourceFactory::CreateConstantBuffer<FModelConstants>();
	ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
	ConstantBufferViewProj = FRenderResourceFactory::CreateConstantBuffer<FCameraConstants>();
}

void URenderer::CreateLightBuffers()
{
	HRESULT hr;

	// 고정 크기 라이트 버퍼 생성 (DYNAMIC 사용)
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(FLightParams) * MAX_LIGHTS;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.StructureByteStride = sizeof(FLightParams);
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    
	hr = DeviceResources->GetDevice()->CreateBuffer(&bufferDesc, nullptr, &AllLightsBuffer);
	assert(SUCCEEDED(hr) && "AllLightsBuffer 생성 실패");
    
	// SRV 생성 (최대 크기로)
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = MAX_LIGHTS;
    
	hr = DeviceResources->GetDevice()->CreateShaderResourceView(AllLightsBuffer, &srvDesc, &AllLightsSRV);
	assert(SUCCEEDED(hr) && "AllLightsSRV 생성 실패");
}

void URenderer::CreateLightCullBuffers()
{
	HRESULT hr;
	const uint32 MAX_SCENE_LIGHTS = 1024;
	const uint32 TILE_SIZE = 32;
	const uint32 numTilesX = (DeviceResources->GetWidth() + TILE_SIZE - 1) / TILE_SIZE;
	const uint32 numTilesY = (DeviceResources->GetHeight() + TILE_SIZE - 1) / TILE_SIZE;
	const uint32 MAX_TILES = numTilesX * numTilesY;
	const uint32 MAX_TOTAL_LIGHT_INDICES = MAX_SCENE_LIGHTS * 32;
        
	// LightIndexBuffer 생성
	D3D11_BUFFER_DESC lightIndexBufferDesc = {};
	lightIndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	lightIndexBufferDesc.ByteWidth = sizeof(uint32) * (MAX_TOTAL_LIGHT_INDICES + 1);
	lightIndexBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	lightIndexBufferDesc.StructureByteStride = sizeof(uint32);
	lightIndexBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        
	hr = DeviceResources->GetDevice()->CreateBuffer(&lightIndexBufferDesc, nullptr, &LightIndexBuffer);
	assert(SUCCEEDED(hr) && "LightIndexBuffer 생성 실패");
        
	D3D11_UNORDERED_ACCESS_VIEW_DESC lightIndexUAVDesc = {};
	lightIndexUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	lightIndexUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	lightIndexUAVDesc.Buffer.FirstElement = 0;
	lightIndexUAVDesc.Buffer.NumElements = MAX_TOTAL_LIGHT_INDICES + 1;
	lightIndexUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
        
	hr = DeviceResources->GetDevice()->CreateUnorderedAccessView(LightIndexBuffer, &lightIndexUAVDesc, &LightIndexBufferUAV);
	assert(SUCCEEDED(hr) && "LightIndexBufferUAV 생성 실패");
        
	D3D11_SHADER_RESOURCE_VIEW_DESC lightIndexSRVDesc = {};
	lightIndexSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	lightIndexSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	lightIndexSRVDesc.Buffer.FirstElement = 1;
	lightIndexSRVDesc.Buffer.NumElements = MAX_TOTAL_LIGHT_INDICES;
        
	hr = DeviceResources->GetDevice()->CreateShaderResourceView(LightIndexBuffer, &lightIndexSRVDesc, &LightIndexBufferSRV);
	assert(SUCCEEDED(hr) && "LightIndexBufferSRV 생성 실패");
        
	// TileLightInfoBuffer 재생성
	D3D11_BUFFER_DESC tileLightInfoBufferDesc = {};
	tileLightInfoBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	tileLightInfoBufferDesc.ByteWidth = sizeof(uint32) * 2 * MAX_TILES;
	tileLightInfoBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	tileLightInfoBufferDesc.StructureByteStride = sizeof(uint32) * 2;
	tileLightInfoBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        
	hr = DeviceResources->GetDevice()->CreateBuffer(&tileLightInfoBufferDesc, nullptr, &TileLightInfoBuffer);
	assert(SUCCEEDED(hr) && "TileLightInfoBuffer 생성 실패");
        
	D3D11_UNORDERED_ACCESS_VIEW_DESC tileLightInfoUAVDesc = {};
	tileLightInfoUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	tileLightInfoUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	tileLightInfoUAVDesc.Buffer.FirstElement = 0;
	tileLightInfoUAVDesc.Buffer.NumElements = MAX_TILES;
        
	hr = DeviceResources->GetDevice()->CreateUnorderedAccessView(TileLightInfoBuffer, &tileLightInfoUAVDesc, &TileLightInfoUAV);
	assert(SUCCEEDED(hr) && "TileLightInfoUAV 생성 실패");
        
	D3D11_SHADER_RESOURCE_VIEW_DESC tileLightInfoSRVDesc = {};
	tileLightInfoSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	tileLightInfoSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	tileLightInfoSRVDesc.Buffer.FirstElement = 0;
	tileLightInfoSRVDesc.Buffer.NumElements = MAX_TILES;
        
	hr = DeviceResources->GetDevice()->CreateShaderResourceView(TileLightInfoBuffer, &tileLightInfoSRVDesc, &TileLightInfoSRV);
	assert(SUCCEEDED(hr) && "TileLightInfoSRV 생성 실패");
}

void URenderer::ReleaseConstantBuffers()
{
	SafeRelease(ConstantBufferModels);
	SafeRelease(ConstantBufferColor);
	SafeRelease(ConstantBufferViewProj);
}

void URenderer::ReleaseLightBuffers()
{
	SafeRelease(AllLightsBuffer);
	SafeRelease(AllLightsSRV);
}

void URenderer::ReleaseLightCullBuffers()
{
	SafeRelease(LightIndexBuffer);
	SafeRelease(LightIndexBufferUAV);
	SafeRelease(LightIndexBufferSRV);
	SafeRelease(TileLightInfoBuffer);
	SafeRelease(TileLightInfoUAV);
	SafeRelease(TileLightInfoSRV);
}
