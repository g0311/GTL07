#pragma once
#include "DeviceResources.h"
#include "Core/Public/Object.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Render/Renderer/Public/Pipeline.h"

class FViewport;
class UCamera;
class UPipeline;
class FViewportClient;
class FCopyPass;
class FFXAAPass;
class FRenderingContext;

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
	void CreateCopyShader();
	void CreateFXAAShader();
	void CreateBillboardShader();

	void CreateConstantBuffers();
	void CreateLightBuffers();
	void CreateLightCullBuffers();
	
	// Release
	void ReleaseConstantBuffers();
	void ReleaseLightBuffers();
	void ReleaseLightCullBuffers();
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

	void OnResize(uint32 Inwidth = 0, uint32 InHeight = 0);

	// Getter & Setter
	ID3D11Device* GetDevice() const { return DeviceResources->GetDevice(); }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceResources->GetDeviceContext(); }
	IDXGISwapChain* GetSwapChain() const { return DeviceResources->GetSwapChain(); }
	
	ID3D11SamplerState* GetDefaultSampler() const { return DefaultSampler; }
	ID3D11ShaderResourceView* GetDepthSRV() const { return DeviceResources->GetDepthStencilSRV(); }
	
	ID3D11RenderTargetView* GetRenderTargetView() const { return DeviceResources->GetFrameBufferRTV(); }
	ID3D11RenderTargetView* GetSceneColorRenderTargetView()const {return DeviceResources->GetSceneColorRenderTargetView(); }
	
	UDeviceResources* GetDeviceResources() const { return DeviceResources; }
	FViewport* GetViewportClient() const { return ViewportClient; }
	UPipeline* GetPipeline() const { return Pipeline; }
	bool GetIsResizing() const { return bIsResizing; }

	ID3D11DepthStencilState* GetDefaultDepthStencilState() const { return DefaultDepthStencilState; }
	ID3D11DepthStencilState* GetDisabledDepthStencilState() const { return DisabledDepthStencilState; }
	ID3D11BlendState* GetAlphaBlendState() const { return AlphaBlendState; }
	ID3D11Buffer* GetConstantBufferModels() const { return ConstantBufferModels; }
	ID3D11Buffer* GetConstantBufferViewProj() const { return ConstantBufferViewProj; }

	ID3D11Buffer* GetLightIndexBuffer() const { return LightIndexBuffer; }
	ID3D11UnorderedAccessView* GetLightIndexBufferUAV() const { return LightIndexBufferUAV; }
	ID3D11ShaderResourceView* GetLightIndexBufferSRV() const { return LightIndexBufferSRV; }
	ID3D11Buffer* GetTileLightInfoBuffer() const { return TileLightInfoBuffer; }
	ID3D11UnorderedAccessView* GetTileLightInfoUAV() const { return TileLightInfoUAV; }
	ID3D11ShaderResourceView* GetTileLightInfoSRV() const { return TileLightInfoSRV; }
	ID3D11Buffer* GetAllLightsBuffer() const { return AllLightsBuffer; }
	ID3D11ShaderResourceView* GetAllLightsSRV() const { return AllLightsSRV; }
	
	void SetIsResizing(bool isResizing) { bIsResizing = isResizing; }

private:
	void SetUpLightingForAllPasses(const FRenderingContext& Context);
	
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
	ID3D11Buffer* ConstantBufferLighting = nullptr;

	// Light Structured Buffers
	
	// UAV Buffers for light lists
	ID3D11Buffer* LightIndexBuffer = nullptr;
	ID3D11UnorderedAccessView* LightIndexBufferUAV = nullptr;
	ID3D11ShaderResourceView* LightIndexBufferSRV = nullptr;

	ID3D11Buffer* TileLightInfoBuffer = nullptr;
	ID3D11UnorderedAccessView* TileLightInfoUAV = nullptr;
	ID3D11ShaderResourceView* TileLightInfoSRV = nullptr;
    
	// 라이트 데이터 버퍼 (고정 크기)
	ID3D11Buffer* AllLightsBuffer = nullptr;
	ID3D11ShaderResourceView* AllLightsSRV = nullptr;
	static constexpr uint32 MAX_LIGHTS = 1024; // 최대 라이트 개수

	
	FLOAT ClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	// Default Shaders
	ID3D11VertexShader* DefaultVertexShader = nullptr;
	ID3D11PixelShader* DefaultPixelShader = nullptr;
	ID3D11InputLayout* DefaultInputLayout = nullptr;

	// Copy Shaders
	ID3D11VertexShader* CopyVertexShader = nullptr;
	ID3D11PixelShader* CopyPixelShader = nullptr;
	ID3D11InputLayout* CopyInputLayout = nullptr;
	ID3D11SamplerState* CopySamplerState = nullptr;

	// FXAA Shaders
	ID3D11VertexShader* FXAAVertexShader = nullptr;
	ID3D11PixelShader* FXAAPixelShader = nullptr;
	ID3D11InputLayout* FXAAInputLayout = nullptr;
	ID3D11SamplerState* FXAASamplerState = nullptr;

	// Billboard Shaders
	ID3D11VertexShader* BillboardVertexShader = nullptr;
	ID3D11PixelShader* BillboardPixelShader = nullptr;
	ID3D11InputLayout* BillboardInputLayout = nullptr;
	
	// Texture Shaders
	ID3D11VertexShader* TextureVertexShader = nullptr;
	ID3D11PixelShader* TexturePixelShader = nullptr;
	ID3D11PixelShader* TexturePixelShaderWithNormalMap = nullptr;
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

	// Uber Shaders
	ID3D11VertexShader* UberVertexShader = nullptr;
	ID3D11PixelShader*  UberPixelShader = nullptr;
	ID3D11InputLayout*  UberInputLayout = nullptr;
	ID3D11SamplerState* UberSampler = nullptr;
	
	uint32 Stride = 0;

	FViewport* ViewportClient = nullptr;
	
	bool bIsResizing = false;

	TArray<class FRenderPass*> RenderPasses;

	FCopyPass* CopyPass = nullptr;
	FFXAAPass* FXAAPass = nullptr;
	class FLightCullingDebugPass* LightCullingDebugPass = nullptr;
};