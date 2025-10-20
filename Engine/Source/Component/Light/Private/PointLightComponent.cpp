#include "pch.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Render/UI/Widget/Light/Public/PointLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

UPointLightComponent::UPointLightComponent()
    : AttenuationRadius(10.0f)
    , LightFalloffExponent(8.0f)
{
    SetColor(FVector4(1.0f, 0.0f, 1.0f, 0.0f));
}

UPointLightComponent::~UPointLightComponent()
{
    // ...delete if something allocated
}

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "AttenuationRadius", AttenuationRadius, 10.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "LightFalloffExponent", LightFalloffExponent, 8.0f);
	}
	else
	{
		InOutHandle["AttenuationRadius"] = AttenuationRadius;
		InOutHandle["LightFalloffExponent"] = LightFalloffExponent;
	}
}

UObject* UPointLightComponent::Duplicate()
{
	UPointLightComponent* PointLightComp = Cast<UPointLightComponent>(Super::Duplicate());
	PointLightComp->AttenuationRadius = AttenuationRadius;
	PointLightComp->LightFalloffExponent = LightFalloffExponent;
	return PointLightComp;
}

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
	return UPointLightComponentWidget::StaticClass();
}
