#include "pch.h"

#include "Component/Public/LightComponent.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_ABSTRACT_CLASS(ULightComponent, USceneComponent)

void ULightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity);
		FJsonSerializer::ReadVector(InOutHandle, "LightColor", LightColor);
	}
	else
	{
		InOutHandle["Intensity"] = Intensity;
		InOutHandle["LightColor"] = FJsonSerializer::VectorToJson(LightColor);
	}
}

UObject* ULightComponent::Duplicate()
{
	ULightComponent* LightComponent = Cast<ULightComponent>(Super::Duplicate());
	LightComponent->Intensity = Intensity;
	LightComponent->LightColor = LightColor;

	return LightComponent;
}

void ULightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}
