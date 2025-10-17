#include "pch.h"
#include "Render/RenderPass/Public/LightCullingPass.h"

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

    if (FAILED(hr))
    {
        assert(false && "LightCulling.hlsl 컴퓨트 셰이더 컴파일 실패!");
        SafeRelease(pBlobCS);
        return;
    }

    // 컴퓨트 셰이더 생성
    DeviceResources->GetDevice()->CreateComputeShader(
        pBlobCS->GetBufferPointer(),
        pBlobCS->GetBufferSize(),
        nullptr,
        &CullingCS);

    SafeRelease(pBlobCS);

    // 컵링 파라미터 상수 버퍼 생성
    CullingParamsCB = FRenderResourceFactory::CreateConstantBuffer<FCullingParams>();

    // 광원 리스트를 위한 구조화 버퍼 생성 (전체 화면 기준)
    // 씨내 최대 광원 수 (설정 가능해야 함)
    const uint32 MAX_SCENE_LIGHTS = 1024;
    // 타일 크기 (셰이더와 일치해야 함)
    const uint32 TILE_SIZE = 16;
    const uint32 screenWidth = DeviceResources->GetWidth();
    const uint32 screenHeight = DeviceResources->GetHeight();
    const uint32 numTilesX = (screenWidth + TILE_SIZE - 1) / TILE_SIZE;
    const uint32 numTilesY = (screenHeight + TILE_SIZE - 1) / TILE_SIZE;
    const uint32 MAX_TILES = numTilesX * numTilesY;

    // LightIndexBuffer: 타일별 가시 광원 인덱스를 저장
    const uint32 MAX_TOTAL_LIGHT_INDICES = MAX_SCENE_LIGHTS * 32; // 타일당 평균 32개 광원 가정

    D3D11_BUFFER_DESC lightIndexBufferDesc = {};
    lightIndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    lightIndexBufferDesc.ByteWidth = sizeof(uint32) * (MAX_TOTAL_LIGHT_INDICES + 1);
    lightIndexBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    lightIndexBufferDesc.StructureByteStride = sizeof(uint32);
    lightIndexBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    hr = DeviceResources->GetDevice()->CreateBuffer(&lightIndexBufferDesc, nullptr, &LightIndexBuffer);
    assert(SUCCEEDED(hr) && "LightIndexBuffer 생성 실패");

    D3D11_UNORDERED_ACCESS_VIEW_DESC lightIndexUAVDesc = {};
    lightIndexUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    lightIndexUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    lightIndexUAVDesc.Buffer.FirstElement = 0;
    lightIndexUAVDesc.Buffer.NumElements = MAX_TOTAL_LIGHT_INDICES + 1;
    lightIndexUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;

    hr = DeviceResources->GetDevice()->CreateUnorderedAccessView(LightIndexBuffer, &lightIndexUAVDesc, &LightIndexBufferUAV);
    assert(SUCCEEDED(hr) && "LightIndexBufferUAV 생성 실패");

    D3D11_SHADER_RESOURCE_VIEW_DESC lightIndexSRVDesc = {};
    lightIndexSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    lightIndexSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    lightIndexSRVDesc.Buffer.FirstElement = 1;
    lightIndexSRVDesc.Buffer.NumElements = MAX_TOTAL_LIGHT_INDICES;

    hr = DeviceResources->GetDevice()->CreateShaderResourceView(LightIndexBuffer, &lightIndexSRVDesc, &LightIndexBufferSRV);
    assert(SUCCEEDED(hr) && "LightIndexBufferSRV 생성 실패");

    // TileLightInfoBuffer
    D3D11_BUFFER_DESC tileLightInfoBufferDesc = {};
    tileLightInfoBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    tileLightInfoBufferDesc.ByteWidth = sizeof(uint32) * 2 * MAX_TILES;
    tileLightInfoBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    tileLightInfoBufferDesc.StructureByteStride = sizeof(uint32) * 2;
    tileLightInfoBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    hr = DeviceResources->GetDevice()->CreateBuffer(&tileLightInfoBufferDesc, nullptr, &TileLightInfoBuffer);
    assert(SUCCEEDED(hr) && "TileLightInfoBuffer 생성 실패");

    D3D11_UNORDERED_ACCESS_VIEW_DESC tileLightInfoUAVDesc = {};
    tileLightInfoUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    tileLightInfoUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    tileLightInfoUAVDesc.Buffer.FirstElement = 0;
    tileLightInfoUAVDesc.Buffer.NumElements = MAX_TILES;

    hr = DeviceResources->GetDevice()->CreateUnorderedAccessView(TileLightInfoBuffer, &tileLightInfoUAVDesc, &TileLightInfoUAV);
    assert(SUCCEEDED(hr) && "TileLightInfoUAV 생성 실패");

    D3D11_SHADER_RESOURCE_VIEW_DESC tileLightInfoSRVDesc = {};
    tileLightInfoSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    tileLightInfoSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    tileLightInfoSRVDesc.Buffer.FirstElement = 0;
    tileLightInfoSRVDesc.Buffer.NumElements = MAX_TILES;

    hr = DeviceResources->GetDevice()->CreateShaderResourceView(TileLightInfoBuffer, &tileLightInfoSRVDesc, &TileLightInfoSRV);
    assert(SUCCEEDED(hr) && "TileLightInfoSRV 생성 실패");

    // 현재 화면 크기 저장
    LastScreenWidth = screenWidth;
    LastScreenHeight = screenHeight;
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
        const uint32 TILE_SIZE = 16;
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

void FLightCullingPass::Execute(FRenderingContext& Context)
{
    // 리사이즈 시 리소스 재생성 확인
    RecreateResourcesIfNeeded();
    
    ID3D11DeviceContext* pContext = DeviceResources->GetDeviceContext();

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
    // TODO: 실제 씨의 광원 개수를 가져 와야 함
    cullingParams.NumLights = 0; // 임시 값

    FRenderResourceFactory::UpdateConstantBufferData(CullingParamsCB, cullingParams);

    // 2. 리소스 바인딩
    pContext->CSSetShader(CullingCS, nullptr, 0);
    pContext->CSSetConstantBuffers(0, 1, &CullingParamsCB);

    // TODO: 씬의 모든 광원 데이터를 담은 StructuredBuffer를 SRV로 바인딩해야 함
    // pContext->CSSetShaderResources(0, 1, &AllLightsSRV);

    // UAVs 바인딩
    pContext->CSSetUnorderedAccessViews(0, 1, &LightIndexBufferUAV, nullptr);
    pContext->CSSetUnorderedAccessViews(1, 1, &TileLightInfoUAV, nullptr);

    // 3. 컴퓨트 셰이더 디스패치 (뷰포트 크기 기반)
    const uint32 TILE_SIZE = 16; // 셰이더와 일치해야 함
    const uint32 viewportWidth = static_cast<uint32>(Context.Viewport.Width);
    const uint32 viewportHeight = static_cast<uint32>(Context.Viewport.Height);
    const uint32 numTilesX = (viewportWidth + TILE_SIZE - 1) / TILE_SIZE;
    const uint32 numTilesY = (viewportHeight + TILE_SIZE - 1) / TILE_SIZE;

    pContext->Dispatch(numTilesX, numTilesY, 1);

    // 4. 리소스 언바인딩
    ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
    pContext->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    pContext->CSSetShaderResources(0, 1, &nullSRV);
    ID3D11Buffer* nullCB = nullptr;
    pContext->CSSetConstantBuffers(0, 1, &nullCB);
    pContext->CSSetShader(nullptr, nullptr, 0);
}
