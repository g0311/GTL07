#include "pch.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Utility/Public/JsonSerializer.h"
#include "Render/UI/Widget/Light/Public/PointLightComponentWidget.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        FJsonSerializer::ReadFloat(InOutHandle, "AttenuationRadius", AttenuationRadius, AttenuationRadius, false);
        FJsonSerializer::ReadFloat(InOutHandle, "LightFalloffExponent", LightFalloffExponent, LightFalloffExponent, false);
    }
    else
    {
        InOutHandle["AttenuationRadius"] = AttenuationRadius;
        InOutHandle["LightFalloffExponent"] = LightFalloffExponent;
    }
}

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

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
	return UPointLightComponentWidget::StaticClass();
}
