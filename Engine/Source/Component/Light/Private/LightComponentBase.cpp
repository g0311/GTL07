#include "pch.h"
#include "Component/Light/Public/LightComponentBase.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_ABSTRACT_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity, 1.0f);
		FJsonSerializer::ReadVector4(InOutHandle, "Color", Color, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	else
	{
		InOutHandle["Intensity"] = Intensity;
		InOutHandle["Color"] = FJsonSerializer::Vector4ToJson(Color);
	}
}

UObject* ULightComponentBase::Duplicate()
{
	ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Super::Duplicate());
	LightComponent->Intensity = Intensity;
	LightComponent->Color = Color;
	return LightComponent;
}
