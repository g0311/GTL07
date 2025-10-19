#include "pch.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Utility/Public/JsonSerializer.h"
#include "Render/UI/Widget/Light/Public/SpotLightComponentWidget.h"

IMPLEMENT_CLASS(USpotLightComponent, ULightComponent)

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        FJsonSerializer::ReadFloat(InOutHandle, "InnerConeAngleRad", InnerConeAngleRad, InnerConeAngleRad, false);
        FJsonSerializer::ReadFloat(InOutHandle, "OuterConeAngleRad", OuterConeAngleRad, OuterConeAngleRad, false);
        FJsonSerializer::ReadFloat(InOutHandle, "Range", Range, Range, false);
    }
    else
    {
        InOutHandle["InnerConeAngleRad"] = InnerConeAngleRad;
        InOutHandle["OuterConeAngleRad"] = OuterConeAngleRad;
        InOutHandle["Range"] = Range;
    }
}

UClass* USpotLightComponent::GetSpecificWidgetClass() const
{
    return USpotlightComponentWidget::StaticClass();
}
