#include "pch.h"
#include "Component/Light/Public/AmbientLightComponent.h"
#include "Render/UI/Widget/Light/Public/AmbientLightComponentWidget.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent)

UClass* UAmbientLightComponent::GetSpecificWidgetClass() const
{
    return UAmbientLightComponentWidget::StaticClass();
}
