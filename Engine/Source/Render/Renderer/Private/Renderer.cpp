#include "pch.h"
#include "Component/Mesh/Public/StaticMesh.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Light/Public/AmbientLightComponent.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Component/Public/HeightFogComponent.h"
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
#include "Render/RenderPass/Public/RenderingContext.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/RenderPass/Public/FXAAPass.h"
#include "Render/RenderPass/Public/LightCullingPass.h"
#include "Render/RenderPass/Public/LightCullingDebugPass.h"
#include "Render/RenderPass/Public/SceneDepthPass.h"
#include "Render/RenderPass/Public/WorldNormalPass.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Render/Renderer/Public/ShaderHotReload.h"

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

	// TODO: 셰이더 생성 부분을 전부 패스 안으로 이동
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
		TextureInputLayout, DefaultDepthStencilState);
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

	// Shader Hot Reload 초기화
	InitializeShaderHotReload();
}

void URenderer::Release()
{
	ReleaseConstantBuffers();
	ReleaseLightBuffers();
	ReleaseLightCullBuffers();
	
	ReleaseShader();
	ReleaseDepthStencilState();
	ReleaseBlendState();
	ReleaseSamplerState();
	FRenderResourceFactory::ReleaseRasterizerState();
	for (auto& RenderPass : RenderPasses)
	{
		RenderPass->Release();
		SafeDelete(RenderPass);
	}

	SafeDelete(ShaderHotReload);
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
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Tangent), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Bitangent), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/UberShader.hlsl", TextureLayout, &TextureVertexShader, &TextureInputLayout);
	UberShaderVertexPermutations.Default = TextureVertexShader;

	UE_LOG("URenderer: Compiling UberShader permutations...");

	D3D_SHADER_MACRO UnlitDefines[] = {
		{ "LIGHTING_MODEL_UNLIT", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberShader.hlsl", &UberShaderPermutations.Unlit, UnlitDefines);

	D3D_SHADER_MACRO GouraudDefines[] = {
		{ "LIGHTING_MODEL_GOURAUD", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberShader.hlsl", &UberShaderPermutations.Gouraud, GouraudDefines);
	FRenderResourceFactory::CreateVertexShader(L"Asset/Shader/UberShader.hlsl", &UberShaderVertexPermutations.Gouraud, GouraudDefines);

	D3D_SHADER_MACRO GouraudNormalDefines[] = {
		{ "LIGHTING_MODEL_GOURAUD", "1" },
		{ "HAS_NORMAL_MAP", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberShader.hlsl", &UberShaderPermutations.GouraudWithNormalMap, GouraudNormalDefines);

	D3D_SHADER_MACRO LambertDefines[] = {
		{ "LIGHTING_MODEL_LAMBERT", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberShader.hlsl", &UberShaderPermutations.Lambert, LambertDefines);

	D3D_SHADER_MACRO LambertNormalDefines[] = {
		{ "LIGHTING_MODEL_LAMBERT", "1" },
		{ "HAS_NORMAL_MAP", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberShader.hlsl", &UberShaderPermutations.LambertWithNormalMap, LambertNormalDefines);

	D3D_SHADER_MACRO PhongDefines[] = {
		{ "LIGHTING_MODEL_PHONG", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberShader.hlsl", &UberShaderPermutations.Phong, PhongDefines);

	D3D_SHADER_MACRO PhongNormalDefines[] = {
		{ "LIGHTING_MODEL_PHONG", "1" },
		{ "HAS_NORMAL_MAP", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberShader.hlsl", &UberShaderPermutations.PhongWithNormalMap, PhongNormalDefines);

	D3D_SHADER_MACRO BlinnDefines[] = {
		{ "LIGHTING_MODEL_BLINNPHONG", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberShader.hlsl", &UberShaderPermutations.BlinnPhong, BlinnDefines);

	D3D_SHADER_MACRO BlinnNormalDefines[] = {
		{ "LIGHTING_MODEL_BLINNPHONG", "1" },
		{ "HAS_NORMAL_MAP", "1" },
		{ nullptr, nullptr }
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberShader.hlsl", &UberShaderPermutations.BlinnPhongWithNormalMap, BlinnNormalDefines);

	UE_LOG("URenderer: UberShader permutations compiled successfully!");

	TexturePixelShader = UberShaderPermutations.BlinnPhong;
	TexturePixelShaderWithNormalMap = UberShaderPermutations.BlinnPhongWithNormalMap;
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

void URenderer::ReleaseShader()
{
	SafeRelease(DefaultInputLayout);
	SafeRelease(DefaultPixelShader);
	SafeRelease(DefaultVertexShader);

	SafeRelease(TextureInputLayout);
	SafeRelease(TextureVertexShader);
	UberShaderVertexPermutations.Default = nullptr;
	
	// Release all UberShader permutations
	SafeRelease(UberShaderVertexPermutations.Gouraud);
	
	SafeRelease(UberShaderPermutations.Unlit);
	SafeRelease(UberShaderPermutations.Gouraud);
	SafeRelease(UberShaderPermutations.Lambert);
	SafeRelease(UberShaderPermutations.Phong);
	SafeRelease(UberShaderPermutations.BlinnPhong);
	SafeRelease(UberShaderPermutations.GouraudWithNormalMap);
	SafeRelease(UberShaderPermutations.LambertWithNormalMap);
	SafeRelease(UberShaderPermutations.PhongWithNormalMap);
	SafeRelease(UberShaderPermutations.BlinnPhongWithNormalMap);
	
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

ID3D11VertexShader* URenderer::GetVertexShaderForLightingModel() const
{
	if (CurrentLightingModel == ELightingModel::Gouraud)
		return UberShaderVertexPermutations.Gouraud;
	return UberShaderVertexPermutations.Default;
}

ID3D11PixelShader* URenderer::GetPixelShaderForLightingModel(bool bHasNormalMap) const
{
	if (bHasNormalMap)
	{
		switch (CurrentLightingModel)
		{
		case ELightingModel::Unlit:
			return UberShaderPermutations.Unlit;
		case ELightingModel::Gouraud:
			return UberShaderPermutations.GouraudWithNormalMap;
		case ELightingModel::Lambert:
			return UberShaderPermutations.LambertWithNormalMap;
		case ELightingModel::Phong:
			return UberShaderPermutations.PhongWithNormalMap;
		case ELightingModel::BlinnPhong:
		default:
			return UberShaderPermutations.BlinnPhongWithNormalMap;
		}
	}
	else
	{
		switch (CurrentLightingModel)
		{
		case ELightingModel::Unlit:
			return UberShaderPermutations.Unlit;
		case ELightingModel::Gouraud:
			return UberShaderPermutations.Gouraud;
		case ELightingModel::Lambert:
			return UberShaderPermutations.Lambert;
		case ELightingModel::Phong:
			return UberShaderPermutations.Phong;
		case ELightingModel::BlinnPhong:
		default:
			return UberShaderPermutations.BlinnPhong;
		}	
	}
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
    // 매 프레임 셰이더 핫 리로드 체크
    CheckShaderHotReload();

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
	
    DeviceResources->UpdateViewport();
}

void URenderer::SetUpTiledLighting(const FRenderingContext& Context)
{
	// Tiled Lighting용 cbuffer 설정
	FTiledLightingParams tiledParams = {};
	tiledParams.ViewportOffset[0] = static_cast<uint32>(Context.Viewport.TopLeftX);
	tiledParams.ViewportOffset[1] = static_cast<uint32>(Context.Viewport.TopLeftY);
	tiledParams.ViewportSize[0] = static_cast<uint32>(Context.Viewport.Width);
	tiledParams.ViewportSize[1] = static_cast<uint32>(Context.Viewport.Height);
	tiledParams.NumLights = static_cast<uint32>(Context.Lights.size());  // Gouraud용 전체 라이트 개수
	// Light Culling 활성화 여부 설정 (ShowFlags에 따라 결정)
	tiledParams.EnableCulling = (Context.ShowFlags & EEngineShowFlags::SF_LightCulling) ? 1u : 0u;

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferTiledLighting, tiledParams);
	Pipeline->SetConstantBuffer(3, false, ConstantBufferTiledLighting);
	Pipeline->SetConstantBuffer(3, true, ConstantBufferTiledLighting);
}

void URenderer::BindTiledLightingBuffers()
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
	ConstantBufferTiledLighting = FRenderResourceFactory::CreateConstantBuffer<FTiledLightingParams>();
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
	const uint32 MAX_TOTAL_LIGHT_INDICES = MAX_SCENE_LIGHTS * 64;
        
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
	lightIndexSRVDesc.Buffer.FirstElement = 0;
	lightIndexSRVDesc.Buffer.NumElements = MAX_TOTAL_LIGHT_INDICES + 1;
        
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
	SafeRelease(ConstantBufferTiledLighting);
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

// ========================================
// Shader Hot Reload System
// ========================================

void URenderer::InitializeShaderHotReload()
{
	ShaderHotReload = new FShaderHotReload();

	// 모든 셰이더 파일 등록
	ShaderHotReload->RegisterShader(L"Asset/Shader/UberShader.hlsl", "UberShader");
	ShaderHotReload->RegisterShader(L"Asset/Shader/DecalShader.hlsl", "DecalShader");
	ShaderHotReload->RegisterShader(L"Asset/Shader/PointLightShader.hlsl", "PointLightShader");
	ShaderHotReload->RegisterShader(L"Asset/Shader/HeightFogShader.hlsl", "FogShader");
	ShaderHotReload->RegisterShader(L"Asset/Shader/Copy.hlsl", "CopyShader");
	ShaderHotReload->RegisterShader(L"Asset/Shader/FXAAShader.hlsl", "FXAAShader");
	ShaderHotReload->RegisterShader(L"Asset/Shader/BillboardShader.hlsl", "BillboardShader");
	ShaderHotReload->RegisterShader(L"Asset/Shader/SampleShader.hlsl", "DefaultShader");
	ShaderHotReload->RegisterShader(L"Asset/Shader/LightCulling.hlsl", "LightCullingShader");

	UE_LOG("ShaderHotReload: Initialized and tracking shader files");
}

void URenderer::CheckShaderHotReload()
{
	if (ShaderHotReload)
	{
		ShaderHotReload->CheckForChanges(this);
	}
}

void URenderer::ReloadUberShader()
{
	UE_LOG("ShaderHotReload: Reloading UberShader...");

	// 기존 셰이더 릴리즈
	SafeRelease(TextureVertexShader);
	// SafeRelease(TextureInputLayout);
	UberShaderVertexPermutations.Default = nullptr;
	SafeRelease(UberShaderVertexPermutations.Gouraud);
	SafeRelease(UberShaderPermutations.Unlit);
	SafeRelease(UberShaderPermutations.Gouraud);
	SafeRelease(UberShaderPermutations.Lambert);
	SafeRelease(UberShaderPermutations.Phong);
	SafeRelease(UberShaderPermutations.BlinnPhong);
	SafeRelease(UberShaderPermutations.GouraudWithNormalMap);
	SafeRelease(UberShaderPermutations.LambertWithNormalMap);
	SafeRelease(UberShaderPermutations.PhongWithNormalMap);
	SafeRelease(UberShaderPermutations.BlinnPhongWithNormalMap);

	// 재컴파일
	CreateTextureShader();

	UE_LOG("ShaderHotReload: UberShader reloaded successfully!");
}

void URenderer::ReloadDecalShader()
{
	UE_LOG("ShaderHotReload: Reloading DecalShader...");

	SafeRelease(DecalVertexShader);
	SafeRelease(DecalPixelShader);
	SafeRelease(DecalInputLayout);

	CreateDecalShader();

	UE_LOG("ShaderHotReload: DecalShader reloaded successfully!");
}

void URenderer::ReloadPointLightShader()
{
	UE_LOG("ShaderHotReload: Reloading PointLightShader...");

	SafeRelease(PointLightVertexShader);
	SafeRelease(PointLightPixelShader);
	SafeRelease(PointLightInputLayout);

	CreatePointLightShader();

	UE_LOG("ShaderHotReload: PointLightShader reloaded successfully!");
}

void URenderer::ReloadFogShader()
{
	UE_LOG("ShaderHotReload: Reloading FogShader...");

	SafeRelease(FogVertexShader);
	SafeRelease(FogPixelShader);
	SafeRelease(FogInputLayout);

	CreateFogShader();

	UE_LOG("ShaderHotReload: FogShader reloaded successfully!");
}

void URenderer::ReloadCopyShader()
{
	UE_LOG("ShaderHotReload: Reloading CopyShader...");

	SafeRelease(CopyVertexShader);
	SafeRelease(CopyPixelShader);
	SafeRelease(CopyInputLayout);

	CreateCopyShader();

	UE_LOG("ShaderHotReload: CopyShader reloaded successfully!");
}

void URenderer::ReloadFXAAShader()
{
	UE_LOG("ShaderHotReload: Reloading FXAAShader...");

	SafeRelease(FXAAVertexShader);
	SafeRelease(FXAAPixelShader);
	SafeRelease(FXAAInputLayout);

	CreateFXAAShader();

	UE_LOG("ShaderHotReload: FXAAShader reloaded successfully!");
}

void URenderer::ReloadBillboardShader()
{
	UE_LOG("ShaderHotReload: Reloading BillboardShader...");

	SafeRelease(BillboardVertexShader);
	SafeRelease(BillboardPixelShader);
	SafeRelease(BillboardInputLayout);

	CreateBillboardShader();

	UE_LOG("ShaderHotReload: BillboardShader reloaded successfully!");
}

void URenderer::ReloadDefaultShader()
{
	UE_LOG("ShaderHotReload: Reloading DefaultShader...");

	SafeRelease(DefaultVertexShader);
	SafeRelease(DefaultPixelShader);
	SafeRelease(DefaultInputLayout);

	CreateDefaultShader();

	UE_LOG("ShaderHotReload: DefaultShader reloaded successfully!");
}

void URenderer::ReloadLightCullingShader()
{
	UE_LOG("ShaderHotReload: Reloading LightCullingShader...");

	// LightCullingPass에서 컴파일하므로 해당 패스를 찾아서 재로드
	for (auto RenderPass : RenderPasses)
	{
		FLightCullingPass* LightCullPass = dynamic_cast<FLightCullingPass*>(RenderPass);
		if (LightCullPass)
		{
			// LightCullingPass의 재로드 메서드 호출 필요
			// 현재는 로그만 출력
			UE_LOG("ShaderHotReload: Found LightCullingPass - manual reload required");
			break;
		}
	}

	UE_LOG("ShaderHotReload: LightCullingShader reload requested!");
}




