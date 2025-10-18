#pragma once
#include "Render/Renderer/Public/Renderer.h"

class FRenderResourceFactory
{
public:
	static void CreateVertexShaderAndInputLayout(const wstring& InFilePath, const TArray<D3D11_INPUT_ELEMENT_DESC>& InInputLayoutDescriptions,
												 ID3D11VertexShader** OutVertexShader, ID3D11InputLayout** OutInputLayout);
	static ID3D11Buffer* CreateVertexBuffer(FNormalVertex* InVertices, uint32 InByteWidth);
	static ID3D11Buffer* CreateVertexBuffer(FVector* InVertices, uint32 InByteWidth, bool bCpuAccess);
	static ID3D11Buffer* CreateIndexBuffer(const void* InIndices, uint32 InByteWidth);
	static void CreatePixelShader(const wstring& InFilePath, ID3D11PixelShader** InPixelShader);
	static ID3D11SamplerState* CreateSamplerState(D3D11_FILTER InFilter, D3D11_TEXTURE_ADDRESS_MODE InAddressMode);
	static ID3D11SamplerState* CreateFXAASamplerState();
	static ID3D11RasterizerState* GetRasterizerState(const FRenderState& InRenderState);
	static void ReleaseRasterizerState();
	static ID3D11BlendState* GetBlendState(const FRenderState& InRenderState);
	static void ReleaseBlendState();

	// Helper function
	static D3D11_CULL_MODE ToD3D11(ECullMode InCull);
	static D3D11_FILL_MODE ToD3D11(EFillMode InFill);
	static D3D11_BLEND_DESC ToD3D11(EBlendMode InBlend);

	template<typename T>
	static ID3D11Buffer* CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = sizeof(T) + 0xf & 0xfffffff0;
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		ID3D11Buffer* Buffer = nullptr;
		URenderer::GetInstance().GetDevice()->CreateBuffer(&Desc, nullptr, &Buffer);
		return Buffer;
	}

	template<typename T>
	static void UpdateConstantBufferData(ID3D11Buffer* Buffer, const T& Data)
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		URenderer::GetInstance().GetDeviceContext()->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		memcpy(MappedResource.pData, &Data, sizeof(T));
		URenderer::GetInstance().GetDeviceContext()->Unmap(Buffer, 0);
	}

	template<typename T>
	static void UpdateVertexBufferData(ID3D11Buffer* InVertexBuffer, const TArray<T>& InVertices)
	{
		if (!URenderer::GetInstance().GetDeviceContext() || !InVertexBuffer || InVertices.empty()) return;

		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		if (FAILED(URenderer::GetInstance().GetDeviceContext()->Map(InVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource))) return;

		memcpy(MappedResource.pData, InVertices.data(), sizeof(T) * InVertices.size());
		URenderer::GetInstance().GetDeviceContext()->Unmap(InVertexBuffer, 0);
	}

private:
	struct FRasterKey
	{
		D3D11_FILL_MODE FillMode = {};
		D3D11_CULL_MODE CullMode = {};

		bool operator==(const FRasterKey& InKey) const
		{
			return FillMode == InKey.FillMode && CullMode == InKey.CullMode;
		}
	};

	struct FBlendKey
	{
		BOOL AlphaToCoverageEnable = FALSE;
		BOOL IndependentBlendEnable = FALSE;
		D3D11_RENDER_TARGET_BLEND_DESC RenderTarget[8] = {};

		bool operator==(const FBlendKey& InKey) const
		{
			return AlphaToCoverageEnable == InKey.AlphaToCoverageEnable &&
				IndependentBlendEnable == InKey.IndependentBlendEnable &&
				memcmp(RenderTarget, InKey.RenderTarget, sizeof(D3D11_RENDER_TARGET_BLEND_DESC) * 8) == 0;
		}
	};

	struct FRasterKeyHasher
	{
		size_t operator()(const FRasterKey& InKey) const noexcept
		{
			auto Mix = [](size_t& H, size_t V)
			{
				H ^= V + 0x9e3779b97f4a7c15ULL + (H << 6) + (H << 2);
			};

			size_t H = 0;
			Mix(H, (size_t)InKey.FillMode);
			Mix(H, (size_t)InKey.CullMode);

			return H;
		}
	};

	struct FBlendKeyHasher
	{
		size_t operator()(const FBlendKey& InKey) const noexcept
		{
			auto Mix = [](size_t& H, size_t V)
			{
				H ^= V + 0x9e3779b97f4a7c15ULL + (H << 6) + (H << 2);
			};

			size_t H = 0;
			Mix(H, (size_t)InKey.AlphaToCoverageEnable);
			Mix(H, (size_t)InKey.IndependentBlendEnable);
			for (size_t i = 0; i < 8; i++)
			{
				Mix(H, (size_t)InKey.RenderTarget[i].BlendEnable);
				Mix(H, (size_t)InKey.RenderTarget[i].SrcBlend);
				Mix(H, (size_t)InKey.RenderTarget[i].DestBlend);
				Mix(H, (size_t)InKey.RenderTarget[i].BlendOp);
				Mix(H, (size_t)InKey.RenderTarget[i].SrcBlendAlpha);
				Mix(H, (size_t)InKey.RenderTarget[i].DestBlendAlpha);
				Mix(H, (size_t)InKey.RenderTarget[i].BlendOpAlpha);
				Mix(H, (size_t)InKey.RenderTarget[i].RenderTargetWriteMask);
			}
			return H;
		}
	};

	static TMap<FRasterKey, ID3D11RasterizerState*, FRasterKeyHasher> RasterCache;
	static TMap<FBlendKey, ID3D11BlendState*, FBlendKeyHasher> BlendCache;
};
