#include "pch.h"
#include "Render/Renderer/Public/Pipeline.h"

/// @brief 그래픽 파이프라인을 관리하는 클래스
UPipeline::UPipeline(ID3D11DeviceContext* InDeviceContext)
	: DeviceContext(InDeviceContext)
{
    // LastPipelineInfo를 유효하지 않은 값으로 초기화하여 첫 UpdatePipeline 호출 시 모든 상태를 강제로 설정
    LastPipelineInfo.InputLayout = (ID3D11InputLayout*)-1;
    LastPipelineInfo.VertexShader = (ID3D11VertexShader*)-1;
    LastPipelineInfo.RasterizerState = (ID3D11RasterizerState*)-1;
    LastPipelineInfo.DepthStencilState = (ID3D11DepthStencilState*)-1;
    LastPipelineInfo.PixelShader = (ID3D11PixelShader*)-1;
    LastPipelineInfo.BlendState = (ID3D11BlendState*)-1;
    LastPipelineInfo.Topology = (D3D11_PRIMITIVE_TOPOLOGY)-1;

    // 캐시된 RTV/DSV 상태 초기화
    for (uint32 i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        LastBoundRTVs[i] = (ID3D11RenderTargetView*)-1;
    }
    LastBoundDSV = (ID3D11DepthStencilView*)-1;
    LastBoundNumRTVs = (uint32)-1; // 첫 설정을 강제하기 위해 유효하지 않은 값 사용
}

UPipeline::~UPipeline()
{
	// Device Context는 Device Resource에서 제거
}


/// @brief 파이프라인 상태를 업데이트
void UPipeline::UpdatePipeline(FPipelineInfo Info)
{
	if (LastPipelineInfo.Topology != Info.Topology) {
		DeviceContext->IASetPrimitiveTopology(Info.Topology);
		LastPipelineInfo.Topology = Info.Topology;
	}
	if (LastPipelineInfo.InputLayout != Info.InputLayout) {
		DeviceContext->IASetInputLayout(Info.InputLayout);
		LastPipelineInfo.InputLayout = Info.InputLayout;
	}
	if (LastPipelineInfo.VertexShader != Info.VertexShader) {
		DeviceContext->VSSetShader(Info.VertexShader, nullptr, 0);
		LastPipelineInfo.VertexShader = Info.VertexShader;
	}
	if (LastPipelineInfo.RasterizerState != Info.RasterizerState) {
		DeviceContext->RSSetState(Info.RasterizerState);
		LastPipelineInfo.RasterizerState = Info.RasterizerState;
	}
	if (Info.DepthStencilState) {
		DeviceContext->OMSetDepthStencilState(Info.DepthStencilState, 0);
	}
	if (LastPipelineInfo.PixelShader != Info.PixelShader) {
		DeviceContext->PSSetShader(Info.PixelShader, nullptr, 0);
		LastPipelineInfo.PixelShader = Info.PixelShader;
	}
	if (LastPipelineInfo.BlendState != Info.BlendState) {
		DeviceContext->OMSetBlendState(Info.BlendState, nullptr, 0xffffffff);
		LastPipelineInfo.BlendState = Info.BlendState;
	}
}

void UPipeline::SetIndexBuffer(ID3D11Buffer* indexBuffer, uint32 stride)
{
	DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
}

/// @brief 정점 버퍼를 바인딩
void UPipeline::SetVertexBuffer(ID3D11Buffer* VertexBuffer, uint32 Stride)
{
	uint32 Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
}

/// @brief 상수 버퍼를 설정
void UPipeline::SetConstantBuffer(uint32 Slot, bool bIsVS, ID3D11Buffer* ConstantBuffer)
{
	if (bIsVS)
		DeviceContext->VSSetConstantBuffers(Slot, 1, &ConstantBuffer);
	else
		DeviceContext->PSSetConstantBuffers(Slot, 1, &ConstantBuffer);
}

/// @brief 텍스처를 설정
void UPipeline::SetTexture(uint32 Slot, bool bIsVS, ID3D11ShaderResourceView* Srv)
{
	if (bIsVS)
		DeviceContext->VSSetShaderResources(Slot, 1, &Srv);
	else
		DeviceContext->PSSetShaderResources(Slot, 1, &Srv);
}

/// @brief 샘플러 상태를 설정
void UPipeline::SetSamplerState(uint32 Slot, bool bIsVS, ID3D11SamplerState* SamplerState)
{
	if (bIsVS)
		DeviceContext->VSSetSamplers(Slot, 1, &SamplerState);
	else
		DeviceContext->PSSetSamplers(Slot, 1, &SamplerState);
}

void UPipeline::SetRenderTargets(uint32 NumViews, ID3D11RenderTargetView* const* RenderTargetViews,
	ID3D11DepthStencilView* DepthStencilView)
{
    bool changed = false;

    // NumViews 변경 여부 확인
    if (LastBoundNumRTVs != NumViews)
    {
        changed = true;
    }
    else
    {
        // RTV 변경 여부 확인
        for (uint32 i = 0; i < NumViews; ++i)
        {
            if (LastBoundRTVs[i] != RenderTargetViews[i])
            {
                changed = true;
                break;
            }
        }
        // DSV 변경 여부 확인
        if (LastBoundDSV != DepthStencilView)
        {
            changed = true;
        }
    }

    if (changed)
    {
	    DeviceContext->OMSetRenderTargets(NumViews, RenderTargetViews, DepthStencilView);

        // 캐시된 상태 업데이트
        for (uint32 i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            LastBoundRTVs[i] = (i < NumViews) ? RenderTargetViews[i] : nullptr;
        }
        LastBoundDSV = DepthStencilView;
        LastBoundNumRTVs = NumViews;
    }
}

/// @brief 정점 개수를 기반으로 드로우 호출
void UPipeline::Draw(uint32 VertexCount, uint32 StartLocation)
{
	DeviceContext->Draw(VertexCount, StartLocation);
}

void UPipeline::DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation, int32 BaseVertexLocation)
{
	DeviceContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}
