#include "pch.h"

#include "Component/Public/FakeLightComponent.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_ABSTRACT_CLASS(UFakeLightComponent, USceneComponent)

void UFakeLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
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

UObject* UFakeLightComponent::Duplicate()
{
	UFakeLightComponent* LightComponent = Cast<UFakeLightComponent>(Super::Duplicate());
	LightComponent->Intensity = Intensity;
	LightComponent->LightColor = LightColor;

	return LightComponent;
}

void UFakeLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}
