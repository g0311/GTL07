#include "pch.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Render/UI/Widget/Light/Public/SpotLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(USpotLightComponent, ULightComponent)

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "InnerConeAngleRad", InnerConeAngleRad, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "OuterConeAngleRad", OuterConeAngleRad, 0.785398f);
		FJsonSerializer::ReadFloat(InOutHandle, "Range", Range, 50.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "Falloff", Light.Falloff, 1.0f);
	}
	else
	{
		InOutHandle["InnerConeAngleRad"] = InnerConeAngleRad;
		InOutHandle["OuterConeAngleRad"] = OuterConeAngleRad;
		InOutHandle["Range"] = Range;
		InOutHandle["Falloff"] = Light.Falloff;
	}
}

UObject* USpotLightComponent::Duplicate()
{
	USpotLightComponent* SpotLightComp = Cast<USpotLightComponent>(Super::Duplicate());
	SpotLightComp->InnerConeAngleRad = InnerConeAngleRad;
	SpotLightComp->OuterConeAngleRad = OuterConeAngleRad;
	SpotLightComp->Range = Range;
	SpotLightComp->Light.Falloff = Light.Falloff;
	return SpotLightComp;
}

UClass* USpotLightComponent::GetSpecificWidgetClass() const
{
    return USpotlightComponentWidget::StaticClass();
}