#pragma once
#include "RenderPass.h"

struct FWorldNormalConstants
{
	FVector2 RenderTargetSize;
};

class FWorldNormalPass : public FRenderPass
{
public:
	FWorldNormalPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11DepthStencilState* InDS);

	void PreExecute(FRenderingContext& Context) override;
	void Execute(FRenderingContext& Context) override;
	void PostExecute(FRenderingContext& Context) override;
	void Release() override;

private:
	ID3D11VertexShader* VertexShader = nullptr;
	ID3D11PixelShader* PixelShader = nullptr;

	ID3D11DepthStencilState* DS = nullptr;
	ID3D11SamplerState* SamplerState = nullptr;
	ID3D11Buffer* ConstantBufferPerFrame = nullptr;
};
