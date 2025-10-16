#pragma once
#include "DeviceResources.h"
#include "Core/Public/Object.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/RenderPass/Public/FXAAPass.h"

class FViewport;
class UCamera;
class UPipeline;
class FViewportClient;
class FFXAAPass;

/**
 * @brief Rendering Pipeline 전반을 처리하는 클래스
 */
UCLASS()
class URenderer : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(URenderer, UObject)

public:
	void Init(HWND InWindowHandle);
	void Release();

	// Initialize
	void CreateDepthStencilState();
	void CreateBlendState();
	void CreateSamplerState();
	void CreateDefaultShader();
	void CreateTextureShader();
	void CreateDecalShader();
	void CreatePointLightShader();
	void CreateFogShader();
	void CreateConstantBuffers();
	void CreateFXAAShader();

	
	// Release
	void ReleaseConstantBuffers();
	void ReleaseDefaultShader();
	void ReleaseDepthStencilState();
	void ReleaseBlendState();
	void ReleaseSamplerState();
	
	// Render
	void Update();
	void RenderBegin() const;
	void RenderLevel(FViewportClient& InViewportClient);
	void RenderEnd() const;
	void RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride = 0, uint32 InIndexBufferStride = 0);

	void OnResize(uint32 Inwidth = 0, uint32 InHeight = 0) const;

	// Getter & Setter
	ID3D11Device* GetDevice() const { return DeviceResources->GetDevice(); }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceResources->GetDeviceContext(); }
	IDXGISwapChain* GetSwapChain() const { return DeviceResources->GetSwapChain(); }
	
	ID3D11SamplerState* GetDefaultSampler() const { return DefaultSampler; }
	ID3D11ShaderResourceView* GetDepthSRV() const { return DeviceResources->GetDepthStencilSRV(); }
	
	ID3D11RenderTargetView* GetRenderTargetView() const { return DeviceResources->GetRenderTargetView(); }
	ID3D11RenderTargetView* GetSceneColorRenderTargetView()const {return DeviceResources->GetSceneColorRenderTargetView(); }
	
	UDeviceResources* GetDeviceResources() const { return DeviceResources; }
	FViewport* GetViewportClient() const { return ViewportClient; }
	UPipeline* GetPipeline() const { return Pipeline; }
	bool GetIsResizing() const { return bIsResizing; }
	bool GetFXAA() const { return bFXAAEnabled; }

	ID3D11DepthStencilState* GetDefaultDepthStencilState() const { return DefaultDepthStencilState; }
	ID3D11DepthStencilState* GetDisabledDepthStencilState() const { return DisabledDepthStencilState; }
	ID3D11BlendState* GetAlphaBlendState() const { return AlphaBlendState; }
	ID3D11Buffer* GetConstantBufferModels() const { return ConstantBufferModels; }
	ID3D11Buffer* GetConstantBufferViewProj() const { return ConstantBufferViewProj; }

	void SetIsResizing(bool isResizing) { bIsResizing = isResizing; }

private:
	UPipeline* Pipeline = nullptr;
	UDeviceResources* DeviceResources = nullptr;
	TArray<UPrimitiveComponent*> PrimitiveComponents;

	// States
	ID3D11DepthStencilState* DefaultDepthStencilState = nullptr;
	ID3D11DepthStencilState* DecalDepthStencilState = nullptr;
	ID3D11DepthStencilState* DisabledDepthStencilState = nullptr;
	ID3D11BlendState* AlphaBlendState = nullptr;
	ID3D11BlendState* AdditiveBlendState = nullptr;
	
	// Constant Buffers
	ID3D11Buffer* ConstantBufferModels = nullptr;
	ID3D11Buffer* ConstantBufferViewProj = nullptr;
	ID3D11Buffer* ConstantBufferColor = nullptr;
	
	FLOAT ClearColor[4] = {0.025f, 0.025f, 0.025f, 1.0f};

	// Default Shaders
	ID3D11VertexShader* DefaultVertexShader = nullptr;
	ID3D11PixelShader* DefaultPixelShader = nullptr;
	ID3D11InputLayout* DefaultInputLayout = nullptr;

	// FXAA Shaders
	ID3D11VertexShader* FXAAVertexShader = nullptr;
	ID3D11PixelShader* FXAAPixelShader = nullptr;
	ID3D11InputLayout* FXAAInputLayout = nullptr;
	ID3D11SamplerState* FXAASamplerState = nullptr;
	
	// Texture Shaders
	ID3D11VertexShader* TextureVertexShader = nullptr;
	ID3D11PixelShader* TexturePixelShader = nullptr;
	ID3D11InputLayout* TextureInputLayout = nullptr;

	// Decal Shaders
	ID3D11VertexShader* DecalVertexShader = nullptr;
	ID3D11PixelShader* DecalPixelShader = nullptr;
	ID3D11InputLayout* DecalInputLayout = nullptr;

	// Point Light Shaders
	ID3D11VertexShader* PointLightVertexShader = nullptr;
	ID3D11PixelShader* PointLightPixelShader = nullptr;
	ID3D11InputLayout* PointLightInputLayout = nullptr;
	
	// Fog Shaders
	ID3D11VertexShader* FogVertexShader = nullptr;
	ID3D11PixelShader* FogPixelShader = nullptr;
	ID3D11InputLayout* FogInputLayout = nullptr;
	ID3D11SamplerState* DefaultSampler = nullptr;
	
	uint32 Stride = 0;

	FViewport* ViewportClient = nullptr;
	
	bool bIsResizing = false;
	bool bFXAAEnabled = true;

	TArray<class FRenderPass*> RenderPasses;

	FFXAAPass* FXAAPass = nullptr;
};