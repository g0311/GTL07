#include "pch.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Render/UI/Widget/Light/Public/PointLightComponentWidget.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

UPointLightComponent::UPointLightComponent()
    : AttenuationRadius(10.0f)
    , LightFalloffExponent(8.0f)
{
    SetColor(FVector4(1.0f, 0.0f, 1.0f, 0.0f));
}

UPointLightComponent::~UPointLightComponent()
{
    // ...delete if something allocated
}

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
	return UPointLightComponentWidget::StaticClass();
}
