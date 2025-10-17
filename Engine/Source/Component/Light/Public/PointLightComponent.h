#pragma once
#include "Component/Light/Public/LightComponent.h"

UCLASS()
class UPointLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UPointLightComponent, ULightComponent)

private:
	float AttenuationRadius;
	float LightFalloffExponent;
};