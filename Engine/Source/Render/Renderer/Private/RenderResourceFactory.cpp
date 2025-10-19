#include "pch.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/Renderer.h"
ID3D11Buffer* FRenderResourceFactory::CreateVertexBuffer(FNormalVertex* InVertices, uint32 InByteWidth)
{
	D3D11_BUFFER_DESC Desc = { InByteWidth, D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
	D3D11_SUBRESOURCE_DATA InitData = { InVertices, 0, 0 };
	ID3D11Buffer* VertexBuffer = nullptr;
	URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, &InitData, &VertexBuffer);
	return VertexBuffer;
}

ID3D11Buffer* FRenderResourceFactory::CreateVertexBuffer(FVector* InVertices, uint32 InByteWidth, bool bCpuAccess)
{
	D3D11_BUFFER_DESC Desc = { InByteWidth, D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
	if (bCpuAccess)
	{
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	D3D11_SUBRESOURCE_DATA InitData = { InVertices, 0, 0 };
	ID3D11Buffer* VertexBuffer = nullptr;
	URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, &InitData, &VertexBuffer);
	return VertexBuffer;
}

ID3D11Buffer* FRenderResourceFactory::CreateIndexBuffer(const void* InIndices, uint32 InByteWidth)
{
	D3D11_BUFFER_DESC Desc = { InByteWidth, D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
	D3D11_SUBRESOURCE_DATA InitData = { InIndices, 0, 0 };
	ID3D11Buffer* IndexBuffer = nullptr;
	URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, &InitData, &IndexBuffer);
	return IndexBuffer;
}

void FRenderResourceFactory::CreateVertexShaderAndInputLayout(const wstring& InFilePath,
	const TArray<D3D11_INPUT_ELEMENT_DESC>& InInputLayoutDescs, ID3D11VertexShader** OutVertexShader,
	ID3D11InputLayout** OutInputLayout, const D3D_SHADER_MACRO* InDefines)
{
	ID3DBlob* VertexShaderBlob = nullptr;
	ID3DBlob* ErrorBlob = nullptr;

	// Debug flags for shader compilation
	UINT CompileFlags = 0;
#if defined(_DEBUG)
	CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT Result = D3DCompileFromFile(InFilePath.data(), InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainVS", "vs_5_0", CompileFlags, 0, &VertexShaderBlob, &ErrorBlob);
	if (FAILED(Result))
	{
		if (ErrorBlob) { OutputDebugStringA(static_cast<char*>(ErrorBlob->GetBufferPointer())); SafeRelease(ErrorBlob); }
		SafeRelease(VertexShaderBlob);
		return;
	}

	URenderer::GetInstance().GetDevice()->CreateVertexShader(VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), nullptr, OutVertexShader);
	if (InInputLayoutDescs.size() > 0)
		URenderer::GetInstance().GetDevice()->CreateInputLayout(InInputLayoutDescs.data(), static_cast<uint32>(InInputLayoutDescs.size()), VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), OutInputLayout);

	SafeRelease(VertexShaderBlob);	
}

void FRenderResourceFactory::CreatePixelShader(const wstring& InFilePath, ID3D11PixelShader** OutPixelShader, const D3D_SHADER_MACRO* InDefines)
{
	ID3DBlob* PixelShaderBlob = nullptr;
	ID3DBlob* ErrorBlob = nullptr;

	// Debug flags fo r shader compilation
	UINT CompileFlags = 0;
#if defined(_DEBUG)
	CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT Result = D3DCompileFromFile(InFilePath.data(), InDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainPS", "ps_5_0", CompileFlags, 0, &PixelShaderBlob, &ErrorBlob);
	if (FAILED(Result))
	{
		if (ErrorBlob) { OutputDebugStringA(static_cast<char*>(ErrorBlob->GetBufferPointer())); SafeRelease(ErrorBlob); }
		SafeRelease(PixelShaderBlob);
		return;
	}

	URenderer::GetInstance().GetDevice()->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), nullptr, OutPixelShader);
	SafeRelease(PixelShaderBlob);
}

ID3D11SamplerState* FRenderResourceFactory::CreateSamplerState(D3D11_FILTER InFilter, D3D11_TEXTURE_ADDRESS_MODE InAddressMode)
{
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = InFilter;
	SamplerDesc.AddressU = InAddressMode;
	SamplerDesc.AddressV = InAddressMode;
	SamplerDesc.AddressW = InAddressMode;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	ID3D11SamplerState* SamplerState = nullptr;
	if (FAILED(URenderer::GetInstance().GetDevice()->CreateSamplerState(&SamplerDesc, &SamplerState)))
	{
		UE_LOG_ERROR("Renderer: ?�플???�테?�트 ?�성 ?�패");
		return nullptr;
	}
	return SamplerState;
}

ID3D11SamplerState* FRenderResourceFactory::CreateFXAASamplerState()
{
	// FXAA?�플???�태 ?�성
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	ID3D11SamplerState* FXAASamplerState = nullptr;
	if (FAILED(URenderer::GetInstance().GetDevice()->CreateSamplerState(&SamplerDesc, &FXAASamplerState)))
	{
		UE_LOG_ERROR("Renderer: ?�플???�테?�트 ?�성 ?�패");
		return nullptr;
	}
	return FXAASamplerState;
}

ID3D11RasterizerState* FRenderResourceFactory::GetRasterizerState(const FRenderState& InRenderState)
{
	const FRasterKey Key{ ToD3D11(InRenderState.FillMode), ToD3D11(InRenderState.CullMode) };
	if (auto Iter = RasterCache.find(Key); Iter != RasterCache.end())
	{
		return Iter->second;
	}

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = Key.FillMode;
	RasterizerDesc.CullMode = Key.CullMode;
	RasterizerDesc.FrontCounterClockwise = TRUE;
	RasterizerDesc.DepthClipEnable = TRUE;

	ID3D11RasterizerState* RasterizerState = nullptr;
	if (FAILED(URenderer::GetInstance().GetDevice()->CreateRasterizerState(&RasterizerDesc, &RasterizerState)))
	{
		return nullptr;
	}

	RasterCache.emplace(Key, RasterizerState);
	return RasterizerState;
}

void FRenderResourceFactory::ReleaseRasterizerState()
{
	for (auto& Cache : RasterCache)
	{
		SafeRelease(Cache.second);
	}
	RasterCache.clear();
}

ID3D11BlendState* FRenderResourceFactory::GetBlendState(const FRenderState& InRenderState)
{
	const D3D11_BLEND_DESC BlendDesc = ToD3D11(InRenderState.BlendMode);

	FBlendKey Key = {};
	Key.AlphaToCoverageEnable = BlendDesc.AlphaToCoverageEnable;
	Key.IndependentBlendEnable = BlendDesc.IndependentBlendEnable;
	memcpy(Key.RenderTarget, BlendDesc.RenderTarget, sizeof(Key.RenderTarget));

	if (auto Iter = BlendCache.find(Key); Iter != BlendCache.end())
	{
		return Iter->second;
	}

	ID3D11BlendState* BlendState = nullptr;
	if (FAILED(URenderer::GetInstance().GetDevice()->CreateBlendState(&BlendDesc, &BlendState)))
	{
		return nullptr;
	}

	BlendCache.emplace(Key, BlendState);
	return BlendState;
}

void FRenderResourceFactory::ReleaseBlendState()
{
	for (auto& Cache : BlendCache)
	{
		SafeRelease(Cache.second);
	}
	BlendCache.clear();
}

D3D11_CULL_MODE FRenderResourceFactory::ToD3D11(ECullMode InCull)
{
	switch (InCull)
	{
	case ECullMode::Back: return D3D11_CULL_BACK;
	case ECullMode::Front: return D3D11_CULL_FRONT;
	case ECullMode::None: return D3D11_CULL_NONE;
	default: return D3D11_CULL_BACK;
	}
}

D3D11_FILL_MODE FRenderResourceFactory::ToD3D11(EFillMode InFill)
{
	switch (InFill)
	{
	case EFillMode::Solid: return D3D11_FILL_SOLID;
	case EFillMode::WireFrame: return D3D11_FILL_WIREFRAME;
	default: return D3D11_FILL_SOLID;
	}
}

D3D11_BLEND_DESC FRenderResourceFactory::ToD3D11(EBlendMode InBlend)
{
	D3D11_BLEND_DESC BlendDesc = {};
	BlendDesc.AlphaToCoverageEnable = FALSE;
	BlendDesc.IndependentBlendEnable = FALSE;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	switch (InBlend)
	{
	case EBlendMode::Opaque:
		BlendDesc.RenderTarget[0].BlendEnable = FALSE;
		break;

	case EBlendMode::Transparent:
		BlendDesc.RenderTarget[0].BlendEnable = TRUE;
		BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		break;
	}
	return BlendDesc;
}

TMap<FRenderResourceFactory::FRasterKey, ID3D11RasterizerState*, FRenderResourceFactory::FRasterKeyHasher> FRenderResourceFactory::RasterCache;
TMap<FRenderResourceFactory::FBlendKey, ID3D11BlendState*, FRenderResourceFactory::FBlendKeyHasher> FRenderResourceFactory::BlendCache;
