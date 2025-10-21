#pragma once
#include "Render/RenderPass/Public/RenderingContext.h"
#include <d3d11.h> // For ID3D11RenderTargetView, ID3D11DepthStencilView

class UPipeline;

/**
 * @brief 특정 Primitive Type별로 달라지는 RenderPass를 관리하고 실행하도록 하는 기본 인터페이스
 */
class FRenderPass
{
public:
    FRenderPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11Buffer* InConstantBufferModel)
        : Pipeline(InPipeline), ConstantBufferCamera(InConstantBufferCamera), ConstantBufferModel(InConstantBufferModel) {}

    virtual ~FRenderPass() = default;

    /**
     * @brief Pass 실행 전 렌더 타겟 및 기타 상태를 설정하는 함수
     * @param Context 프레임 렌더링에 필요한 모든 정보를 담고 있는 객체
     */
    virtual void PreExecute(FRenderingContext& Context) = 0;

    /**
     * @brief 프레임마다 실행할 렌더 함수
     * @param Context 프레임 렌더링에 필요한 모든 정보를 담고 있는 객체
     */
    virtual void Execute(FRenderingContext& Context) = 0;

    /**
     * @brief Pass 실행 후 상태를 정리하는 함수 (선택 사항)
     * @param Context 프레임 렌더링에 필요한 모든 정보를 담고 있는 객체
     */
    virtual void PostExecute(FRenderingContext& Context) = 0;

    /**
     * @brief 생성한 객체들 해제
     */
    virtual void Release() = 0;

protected:
    UPipeline* Pipeline;
    ID3D11Buffer* ConstantBufferCamera;
    ID3D11Buffer* ConstantBufferModel;

    // Pass가 사용할 RTV/DSV를 캐시하는 멤버 변수
    TArray<ID3D11RenderTargetView*> TargetRTVs;
    ID3D11DepthStencilView* TargetDSV = nullptr;
    uint32 NumTargetRTVs = 0;
};