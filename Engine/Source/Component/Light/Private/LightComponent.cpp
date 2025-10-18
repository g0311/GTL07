#include "pch.h"
#include "Component/Light/Public/LightComponent.h"
#include "Render/UI/Widget/Light/Public/LightComponentWidget.h"

IMPLEMENT_CLASS(ULightComponent, ULightComponentBase)

UClass* ULightComponent::GetSpecificWidgetClass() const
{
    return ULightComponentWidget::StaticClass();
}