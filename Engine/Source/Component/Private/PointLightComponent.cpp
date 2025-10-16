#include "pch.h"

#include "Component/Public/PointLightComponent.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "LightFalloffExtent", LightFalloffExtent);
		FJsonSerializer::ReadFloat(InOutHandle, "SourceRadius", SourceRadius);
	}
	else
	{
		InOutHandle["LightFalloffExtent"] = LightFalloffExtent;
		InOutHandle["SourceRadius"] = SourceRadius;
	}
}

UObject* UPointLightComponent::Duplicate()
{
	UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(Super::Duplicate());
	PointLightComponent->LightFalloffExtent = LightFalloffExtent;
	PointLightComponent->SourceRadius = SourceRadius;

	return PointLightComponent;
}

void UPointLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
    return UPointLightComponentWidget::StaticClass();
}
