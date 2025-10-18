#include "pch.h"
#include "Render/UI/Widget/Light/Public/SpotlightComponentWidget.h"

void USpotlightComponentWidget::Initialize()
{
    ULightComponentWidget::Initialize();
}

void USpotlightComponentWidget::Update()
{
    Super::RenderWidget();
    if (LightComponent = Cast<USpotlightComponentWidget>(LightComponent));
}

void USpotlightComponentWidget::RenderWidget()
{
    Super::RenderWidget();

    
}

