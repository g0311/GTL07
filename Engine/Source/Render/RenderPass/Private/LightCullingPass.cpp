#include "pch.h"
#include "Render/RenderPass/Public/LightCullingPass.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Component/Public/FakePointLightComponent.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/DeviceResources.h"
#include "Render/RenderPass/Public/FogPass.h"

FLightCullingPass::FLightCullingPass(UPipeline* InPipeline, UDeviceResources* InDeviceResources)
    : FRenderPass(InPipeline, nullptr, nullptr)
    , DeviceResources(InDeviceResources)
{
    CreateResources();
}

FLightCullingPass::~FLightCullingPass()
{
    ReleaseResources();
}

void FLightCullingPass::CreateResources()
{
    HRESULT hr;
    ID3DBlob* pBlobCS = nullptr;
    
    // 셰이더 컴파일
    hr = D3DCompileFromFile(
        L"Asset/Shader/LightCulling.hlsl",
        nullptr,
        nullptr,
        "CSMain",
        "cs_5_0",
        0,
        0,
        &pBlobCS,
        nullptr);
    assert(SUCCEEDED(hr) && "LightCulling.hlsl 컴퓨트 셰이더 컴파일 실패");

    // 컴퓨트 셰이더 생성
    hr = DeviceResources->GetDevice()->CreateComputeShader(
        pBlobCS->GetBufferPointer(),
        pBlobCS->GetBufferSize(),
        nullptr,
        &CullingCS);
    assert(SUCCEEDED(hr) && "LightCulling.hlsl 컴퓨트 셰이더 생성 실패");

    SafeRelease(pBlobCS);

    // 컵링 파라미터 상수 버퍼 생성
    CullingParamsCB = FRenderResourceFactory::CreateConstantBuffer<FCullingParams>();
}

void FLightCullingPass::ReleaseResources()
{
    SafeRelease(CullingCS);
    SafeRelease(CullingParamsCB);
}

void FLightCullingPass::PreExecute(FRenderingContext& Context)
{
    URenderer& Renderer = URenderer::GetInstance();
    
    LightIndexBufferUAV = Renderer.GetLightIndexBufferUAV();
    TileLightInfoUAV = Renderer.GetTileLightInfoUAV();
    
    AllLightsBuffer = Renderer.GetAllLightsBuffer();
    AllLightsSRV = Renderer.GetAllLightsSRV();
}

void FLightCullingPass::Execute(FRenderingContext& Context)
{
    TIME_PROFILE(LightCullingPass)
    
    ID3D11DeviceContext* DeviceContext = DeviceResources->GetDeviceContext();

    // 1. 상수 버퍼 업데이트
    FCullingParams cullingParams;
    cullingParams.View = Context.CurrentCamera->GetFViewProjConstants().View;
    cullingParams.Projection = Context.CurrentCamera->GetFViewProjConstants().Projection;
    // 전체 화면 크기는 리소스 버퍼 크기와 일치
    cullingParams.ScreenDimensions[0] = DeviceResources->GetWidth();
    cullingParams.ScreenDimensions[1] = DeviceResources->GetHeight();
    // 뷰포트 오프셋 및 크기 전달
    cullingParams.ViewportOffset[0] = static_cast<uint32>(Context.Viewport.TopLeftX);
    cullingParams.ViewportOffset[1] = static_cast<uint32>(Context.Viewport.TopLeftY);
    cullingParams.ViewportSize[0] = static_cast<uint32>(Context.Viewport.Width);
    cullingParams.ViewportSize[1] = static_cast<uint32>(Context.Viewport.Height);
    // 리소스 크기 체크 및 라이트 데이터 준비
    const uint32 totalLights = static_cast<uint32>(Context.PointLights.size() + Context.SpotLights.size());
    cullingParams.NumLights = totalLights;
    
    // 라이트 데이터 배열 준비 (AllLights 버퍼용)
    TArray<FLightParams> allLights;
    allLights.reserve(totalLights);
    
    // 포인트 라이트 추가
    for (const auto& pointLight : Context.PointLights)
    {
        FLightParams lightData;
        FVector worldPos = pointLight->GetWorldLocation();
        lightData.Position = FVector4(worldPos.X, worldPos.Y, worldPos.Z, 100.0f); // 기본 반지름
        lightData.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f); // 기본 색상
        lightData.Direction = FVector4(0, 0, 0, static_cast<float>(ELightType::Point));
        lightData.Angles = FVector4(0, 0, 0, 0);
        allLights.push_back(lightData);
    }
    
    // 스포 라이트 추가
    for (const auto& spotLight : Context.SpotLights)
    {
        FLightParams lightData;
        FVector worldPos = spotLight->GetWorldLocation();
        FVector forwardDir = {1,0,0} /*spotLight->GetForwardVector()*/;
        
        lightData.Position = FVector4(worldPos.X, worldPos.Y, worldPos.Z, 100.0f); // 기본 반지름
        lightData.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f); // 기본 색상
        lightData.Direction = FVector4(forwardDir.X, forwardDir.Y, forwardDir.Z, static_cast<float>(ELightType::Spot));
        // 기본 컨 각도 (30도 inner, 45도 outer)
        lightData.Angles = FVector4(cosf(30.0f * 3.14159f / 180.0f), cosf(45.0f * 3.14159f / 180.0f), 0, 0);
        allLights.push_back(lightData);
    }

    FRenderResourceFactory::UpdateConstantBufferData(CullingParamsCB, cullingParams);
    
    // 라이트 데이터 업데이트 (DYNAMIC 버퍼 사용)
    if (totalLights > 0 && totalLights <= MAX_LIGHTS)
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = DeviceContext->Map(AllLightsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            // 전체 버퍼 클리어 (이전 프레임의 데이터 제거)
            memset(mappedResource.pData, 0, sizeof(FLightParams) * MAX_LIGHTS);
            // 실제 라이트 데이터 복사
            if (totalLights > 0)
            {
                memcpy(mappedResource.pData, allLights.data(), sizeof(FLightParams) * totalLights);
            }
            DeviceContext->Unmap(AllLightsBuffer, 0);
        }
        else
        {
            assert(false && "AllLightsBuffer 맵핑 실패");
        }
    }
    else if (totalLights > MAX_LIGHTS)
    {
        // 경고: 최대 라이트 개수 초과
        UE_LOG_WARNING("라이트 개수가 최대치를 초과했습니다: %d > %d", static_cast<int>(totalLights), static_cast<int>(MAX_LIGHTS));
        cullingParams.NumLights = MAX_LIGHTS; // 최대치로 제한
    }

    // 2. 리소스 바인딩
    DeviceContext->CSSetShader(CullingCS, nullptr, 0);
    DeviceContext->CSSetConstantBuffers(0, 1, &CullingParamsCB);

    // 라이트 데이터 SRV 바인딩 (버퍼는 항상 존재)
    DeviceContext->CSSetShaderResources(0, 1, &AllLightsSRV);

    // UAVs 바인딩
    DeviceContext->CSSetUnorderedAccessViews(0, 1, &LightIndexBufferUAV, nullptr);
    DeviceContext->CSSetUnorderedAccessViews(1, 1, &TileLightInfoUAV, nullptr);

    // 3. 컴퓨트 셰이더 디스패치 (뷰포트 크기 기반)
    const uint32 TILE_SIZE = 32;

    const uint32 viewportWidth = static_cast<uint32>(Context.Viewport.Width);
    const uint32 viewportHeight = static_cast<uint32>(Context.Viewport.Height);
    
    const uint32 numTilesX = (viewportWidth + TILE_SIZE - 1) / TILE_SIZE;
    const uint32 numTilesY = (viewportHeight + TILE_SIZE - 1) / TILE_SIZE;
        //소수점 버리기를 제거하기 위해 TILE_SIZE - 1을 더한 후 나눗셈

    DeviceContext->Dispatch(numTilesX, numTilesY, 1);
}

void FLightCullingPass::PostExecute(FRenderingContext& Context)
{
    const auto& Renderer = URenderer::GetInstance();
    const auto& DeviceResources = Renderer.GetDeviceResources();
    ID3D11DeviceContext* DeviceContext = DeviceResources->GetDeviceContext();

    ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
    DeviceContext->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    DeviceContext->CSSetShaderResources(0, 1, nullSRVs);
    ID3D11Buffer* nullCB = nullptr;
    DeviceContext->CSSetConstantBuffers(0, 1, &nullCB);
    DeviceContext->CSSetShader(nullptr, nullptr, 0);
}

void FLightCullingPass::Release()
{
    ReleaseResources();
}
