#include "pch.h"

#include "Component/Public/FakePointLightComponent.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UFakePointLightComponent, UFakeLightComponent)

void UFakePointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
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

UObject* UFakePointLightComponent::Duplicate()
{
	UFakePointLightComponent* PointLightComponent = Cast<UFakePointLightComponent>(Super::Duplicate());
	PointLightComponent->LightFalloffExtent = LightFalloffExtent;
	PointLightComponent->SourceRadius = SourceRadius;

	return PointLightComponent;
}

void UFakePointLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* UFakePointLightComponent::GetSpecificWidgetClass() const
{
    return UPointLightComponentWidget::StaticClass();
}
