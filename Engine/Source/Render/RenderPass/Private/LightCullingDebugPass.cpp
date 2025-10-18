
#include "pch.h"
#include "Render/RenderPass/Public/LightCullingDebugPass.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/DeviceResources.h"
#include "Global/Enum.h"

FLightCullingDebugPass::FLightCullingDebugPass(UPipeline* InPipeline, UDeviceResources* InDeviceResources)
    : FRenderPass(InPipeline, nullptr, nullptr), DeviceResources(InDeviceResources)
{
    CreateResources();
}

FLightCullingDebugPass::~FLightCullingDebugPass()
{
    ReleaseResources();
}

void FLightCullingDebugPass::CreateResources()
{
    // Create Staging Buffer
    D3D11_BUFFER_DESC desc;
    URenderer::GetInstance().GetTileLightInfoBuffer()->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    HRESULT hr = DeviceResources->GetDevice()->CreateBuffer(&desc, nullptr, &TileLightInfoStagingBuffer);
    assert(SUCCEEDED(hr));

    // Create D2D/DWrite resources
    DWriteFactory = DeviceResources->GetDWriteFactory();
    if (DWriteFactory)
    {
        DWriteFactory->CreateTextFormat(
            L"Consolas",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f,
            L"en-us",
            &TextFormat
        );
    }

    D2D1_FACTORY_OPTIONS opts{};
#ifdef _DEBUG
    opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &opts, (void**)&D2DFactory);
    assert(SUCCEEDED(hr));

    IDXGIDevice* dxgiDevice = nullptr;
    hr = DeviceResources->GetDevice()->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    assert(SUCCEEDED(hr));

    hr = D2DFactory->CreateDevice(dxgiDevice, &D2DDevice);
    SafeRelease(dxgiDevice);
    assert(SUCCEEDED(hr));

    hr = D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2DContext);
    assert(SUCCEEDED(hr));

    hr = D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &TextBrush);
    assert(SUCCEEDED(hr));
}

void FLightCullingDebugPass::ReleaseResources()
{
    SafeRelease(TileLightInfoStagingBuffer);
    SafeRelease(TextFormat);
    SafeRelease(D2DFactory);
    SafeRelease(D2DDevice);
    SafeRelease(D2DContext);
    SafeRelease(TextBrush);
}

void FLightCullingDebugPass::PreExecute(FRenderingContext& Context)
{
    
}

void FLightCullingDebugPass::Execute(FRenderingContext& Context)
{
    if (!(Context.ShowFlags & EEngineShowFlags::SF_LightCullingDebug))
    {
        return;
    }

    RenderDebugInfo(Context);
}

void FLightCullingDebugPass::PostExecute(FRenderingContext& Context)
{
    
}

void FLightCullingDebugPass::Release()
{
    ReleaseResources();
}

void FLightCullingDebugPass::RenderDebugInfo(FRenderingContext& Context)
{
    ID3D11DeviceContext* Ctx = DeviceResources->GetDeviceContext();
    URenderer& renderer = URenderer::GetInstance();

    Ctx->CopyResource(TileLightInfoStagingBuffer, renderer.GetTileLightInfoBuffer());

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = Ctx->Map(TileLightInfoStagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr))
    {
        return;
    }

    const uint32 TILE_SIZE = 32;
    const uint32 globalNumTilesX = (DeviceResources->GetWidth() + TILE_SIZE - 1) / TILE_SIZE;

    struct TileInfo { uint32_t LightOffset; uint32_t LightCount; };
    const TileInfo* tileInfo = static_cast<const TileInfo*>(mappedResource.pData);

    IDXGISurface* surface = nullptr;
    hr = renderer.GetSwapChain()->GetBuffer(0, __uuidof(IDXGISurface), (void**)&surface);
    if (FAILED(hr))
    {
        Ctx->Unmap(TileLightInfoStagingBuffer, 0);
        return;
    }

    D2D1_BITMAP_PROPERTIES1 bmpProps = {};
    bmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    bmpProps.dpiX = 96.0f;
    bmpProps.dpiY = 96.0f;
    bmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    ID2D1Bitmap1* targetBmp = nullptr;
    hr = D2DContext->CreateBitmapFromDxgiSurface(surface, &bmpProps, &targetBmp);
    SafeRelease(surface);

    if (FAILED(hr))
    {
        Ctx->Unmap(TileLightInfoStagingBuffer, 0);
        return;
    }

    D2DContext->SetTarget(targetBmp);
    D2DContext->BeginDraw();

    // Set clipping rectangle to the viewport
    const D2D1_RECT_F viewportRect = { Context.Viewport.TopLeftX, Context.Viewport.TopLeftY, Context.Viewport.TopLeftX + Context.Viewport.Width, Context.Viewport.TopLeftY + Context.Viewport.Height };
    D2DContext->PushAxisAlignedClip(viewportRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    const uint32 numTilesX = (Context.Viewport.Width + TILE_SIZE - 1) / TILE_SIZE;
    const uint32 numTilesY = (Context.Viewport.Height + TILE_SIZE - 1) / TILE_SIZE;
    const uint32 startTileX = (UINT)Context.Viewport.TopLeftX / TILE_SIZE;
    const uint32 startTileY = (UINT)Context.Viewport.TopLeftY / TILE_SIZE;

    for (uint32 y = 0; y < numTilesY; ++y)
    {
        for (uint32 x = 0; x < numTilesX; ++x)
        {
            uint32 globalTileX = startTileX + x;
            uint32 globalTileY = startTileY + y;
            uint32_t lightCount = tileInfo[globalTileY * globalNumTilesX + globalTileX].LightCount;
            
            // 뷰포트 내 상대 좌표로 렌더링 (뷰포트 오프셋 적용)
            float tileScreenX = Context.Viewport.TopLeftX + x * TILE_SIZE;
            float tileScreenY = Context.Viewport.TopLeftY + y * TILE_SIZE;
            D2D1_RECT_F rect = D2D1::RectF(tileScreenX, tileScreenY, tileScreenX + TILE_SIZE, tileScreenY + TILE_SIZE);
            
            // Draw grid
            TextBrush->SetColor(D2D1::ColorF(0.3f, 0.3f, 0.3f, 0.5f));
            D2DContext->DrawRectangle(&rect, TextBrush, 0.5f);

            if (lightCount > 0)
            {
                TextBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            }
            else
            {
                TextBrush->SetColor(D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.8f));
            }
            std::wstring wtext = std::to_wstring(lightCount);
            D2DContext->DrawTextW(wtext.c_str(), wtext.length(), TextFormat, &rect, TextBrush);
        }
    }

    D2DContext->PopAxisAlignedClip();
    D2DContext->EndDraw();
    D2DContext->SetTarget(nullptr);
    SafeRelease(targetBmp);

    Ctx->Unmap(TileLightInfoStagingBuffer, 0);
}

std::wstring FLightCullingDebugPass::ToWString(const FString& InStr)
{
    if (InStr.empty()) return std::wstring();
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), (int)InStr.size(), NULL, 0);
    std::wstring wStr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), (int)InStr.size(), &wStr[0], sizeNeeded);
    return wStr;
}
