#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

class FStaticMeshPass : public FRenderPass
{
public:
    FStaticMeshPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel,
        ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11PixelShader* InPSWithNormalMap, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS);
    void PreExecute(FRenderingContext& Context) override;
    void Execute(FRenderingContext& Context) override;
    void PostExecute(FRenderingContext& Context) override;
    void Release() override;

    FMaterialConstants CreateMaterialConstants(UMaterial* Material, UStaticMeshComponent* MeshComp);
    void BindMaterialTextures(UMaterial* Material);
    void SetUpTiledLighting(const FRenderingContext& Context); // Tiled Lighting cbuffer 설정
    void BindTiledLightingBuffers(); // Tiled Lighting을 위한 Structured Buffer SRV 바인딩

private:
    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11PixelShader* PSWithNormalMap = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS = nullptr;

    ID3D11Buffer* ConstantBufferMaterial = nullptr;
	ID3D11Buffer* ConstantBufferTiledLighting = nullptr; // Tiled Lighting 용 cbuffer
};
