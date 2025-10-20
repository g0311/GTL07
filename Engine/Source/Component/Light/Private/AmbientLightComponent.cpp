#include "pch.h"
#include "Component/Light/Public/AmbientLightComponent.h"
#include "Render/UI/Widget/Light/Public/AmbientComponentWidget.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent)

UAmbientLightComponent::UAmbientLightComponent()
{
	Intensity = 0.1f;
}

UClass* UAmbientLightComponent::GetSpecificWidgetClass() const
{
    return UAmbientComponentWidget::StaticClass();
}
