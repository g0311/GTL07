#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

// Matches the layout in DecalShader.hlsl
struct FModelConstants
{
    FMatrix World;
    FMatrix WorldInverseTranspose;
};

struct FDecalConstants
{
    FMatrix DecalWorld;
    FMatrix DecalViewProjection;
    float FadeProgress;
    float Padding[3];
};

class FDecalPass : public FRenderPass
{
public:
    FDecalPass(
        UPipeline* InPipeline,
        ID3D11Buffer* InConstantBufferViewProj,
        ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS_Read, ID3D11BlendState* InBlendState
);
    
    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    // --- Octree Optimization ---
    void Query(FOctree* InOctree, UDecalComponent* InDecal, TArray<UPrimitiveComponent*>& OutPrimitives);

    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS_Read = nullptr;
    ID3D11BlendState* BlendState = nullptr;

    ID3D11Buffer* ConstantBufferDecal = nullptr;
    ID3D11Buffer* ConstantBufferPrim = nullptr;
};