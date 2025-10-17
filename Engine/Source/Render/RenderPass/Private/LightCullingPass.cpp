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

    // 고정 크기 라이트 버퍼 생성 (DYNAMIC 사용)
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth = sizeof(FLightParams) * MAX_LIGHTS;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.StructureByteStride = sizeof(FLightParams);
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    
    hr = DeviceResources->GetDevice()->CreateBuffer(&bufferDesc, nullptr, &AllLightsBuffer);
    assert(SUCCEEDED(hr) && "AllLightsBuffer 생성 실패");
    
    // SRV 생성 (최대 크기로)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = MAX_LIGHTS;
    
    hr = DeviceResources->GetDevice()->CreateShaderResourceView(AllLightsBuffer, &srvDesc, &AllLightsSRV);
    assert(SUCCEEDED(hr) && "AllLightsSRV 생성 실패");

    // 타일 기반 버퍼들 생성
    RecreateResourcesIfNeeded();
}

void FLightCullingPass::ReleaseResources()
{
    SafeRelease(CullingCS);
    SafeRelease(CullingParamsCB);
    SafeRelease(LightIndexBuffer);
    SafeRelease(LightIndexBufferUAV);
    SafeRelease(LightIndexBufferSRV);
    SafeRelease(TileLightInfoBuffer);
    SafeRelease(TileLightInfoUAV);
    SafeRelease(TileLightInfoSRV);
    SafeRelease(AllLightsBuffer);
    SafeRelease(AllLightsSRV);
    
    LastScreenWidth = 0;
    LastScreenHeight = 0;
}

void FLightCullingPass::RecreateResourcesIfNeeded()
{
    const uint32 currentWidth = DeviceResources->GetWidth();
    const uint32 currentHeight = DeviceResources->GetHeight();
    
    // 화면 크기가 변경되었는지 확인
    if (currentWidth != LastScreenWidth || currentHeight != LastScreenHeight)
    {
        // 기존 리소스 해제 (상수버퍼와 셰이더 제외)
        SafeRelease(LightIndexBuffer);
        SafeRelease(LightIndexBufferUAV);
        SafeRelease(LightIndexBufferSRV);
        SafeRelease(TileLightInfoBuffer);
        SafeRelease(TileLightInfoUAV);
        SafeRelease(TileLightInfoSRV);
        
        // 새 크기로 리소스 재생성
        HRESULT hr;
        const uint32 MAX_SCENE_LIGHTS = 1024;
        const uint32 TILE_SIZE = 32;
        const uint32 numTilesX = (currentWidth + TILE_SIZE - 1) / TILE_SIZE;
        const uint32 numTilesY = (currentHeight + TILE_SIZE - 1) / TILE_SIZE;
        const uint32 MAX_TILES = numTilesX * numTilesY;
        const uint32 MAX_TOTAL_LIGHT_INDICES = MAX_SCENE_LIGHTS * 32;
        
        // LightIndexBuffer 재생성
        D3D11_BUFFER_DESC lightIndexBufferDesc = {};
        lightIndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        lightIndexBufferDesc.ByteWidth = sizeof(uint32) * (MAX_TOTAL_LIGHT_INDICES + 1);
        lightIndexBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        lightIndexBufferDesc.StructureByteStride = sizeof(uint32);
        lightIndexBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        
        hr = DeviceResources->GetDevice()->CreateBuffer(&lightIndexBufferDesc, nullptr, &LightIndexBuffer);
        assert(SUCCEEDED(hr) && "LightIndexBuffer 재생성 실패");
        
        D3D11_UNORDERED_ACCESS_VIEW_DESC lightIndexUAVDesc = {};
        lightIndexUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        lightIndexUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        lightIndexUAVDesc.Buffer.FirstElement = 0;
        lightIndexUAVDesc.Buffer.NumElements = MAX_TOTAL_LIGHT_INDICES + 1;
        lightIndexUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
        
        hr = DeviceResources->GetDevice()->CreateUnorderedAccessView(LightIndexBuffer, &lightIndexUAVDesc, &LightIndexBufferUAV);
        assert(SUCCEEDED(hr) && "LightIndexBufferUAV 재생성 실패");
        
        D3D11_SHADER_RESOURCE_VIEW_DESC lightIndexSRVDesc = {};
        lightIndexSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        lightIndexSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        lightIndexSRVDesc.Buffer.FirstElement = 1;
        lightIndexSRVDesc.Buffer.NumElements = MAX_TOTAL_LIGHT_INDICES;
        
        hr = DeviceResources->GetDevice()->CreateShaderResourceView(LightIndexBuffer, &lightIndexSRVDesc, &LightIndexBufferSRV);
        assert(SUCCEEDED(hr) && "LightIndexBufferSRV 재생성 실패");
        
        // TileLightInfoBuffer 재생성
        D3D11_BUFFER_DESC tileLightInfoBufferDesc = {};
        tileLightInfoBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        tileLightInfoBufferDesc.ByteWidth = sizeof(uint32) * 2 * MAX_TILES;
        tileLightInfoBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        tileLightInfoBufferDesc.StructureByteStride = sizeof(uint32) * 2;
        tileLightInfoBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        
        hr = DeviceResources->GetDevice()->CreateBuffer(&tileLightInfoBufferDesc, nullptr, &TileLightInfoBuffer);
        assert(SUCCEEDED(hr) && "TileLightInfoBuffer 재생성 실패");
        
        D3D11_UNORDERED_ACCESS_VIEW_DESC tileLightInfoUAVDesc = {};
        tileLightInfoUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        tileLightInfoUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        tileLightInfoUAVDesc.Buffer.FirstElement = 0;
        tileLightInfoUAVDesc.Buffer.NumElements = MAX_TILES;
        
        hr = DeviceResources->GetDevice()->CreateUnorderedAccessView(TileLightInfoBuffer, &tileLightInfoUAVDesc, &TileLightInfoUAV);
        assert(SUCCEEDED(hr) && "TileLightInfoUAV 재생성 실패");
        
        D3D11_SHADER_RESOURCE_VIEW_DESC tileLightInfoSRVDesc = {};
        tileLightInfoSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        tileLightInfoSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        tileLightInfoSRVDesc.Buffer.FirstElement = 0;
        tileLightInfoSRVDesc.Buffer.NumElements = MAX_TILES;
        
        hr = DeviceResources->GetDevice()->CreateShaderResourceView(TileLightInfoBuffer, &tileLightInfoSRVDesc, &TileLightInfoSRV);
        assert(SUCCEEDED(hr) && "TileLightInfoSRV 재생성 실패");
        
        // 현재 크기 업데이트
        LastScreenWidth = currentWidth;
        LastScreenHeight = currentHeight;
    }
}

void FLightCullingPass::PreExecute(FRenderingContext& Context)
{
    // 리사이즈 시 리소스 재생성 확인
    RecreateResourcesIfNeeded();
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

    
}
