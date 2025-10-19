#pragma once
#include "Component/Light/Public/LightComponent.h"

UCLASS()
class UDirectionalLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	FVector GetForwardVector() const;
};