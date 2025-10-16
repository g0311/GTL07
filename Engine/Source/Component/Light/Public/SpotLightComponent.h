#pragma once
#include "Component/Light/Public/LightComponent.h"

UCLASS()
class USpotLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(USpotLightComponent, ULightComponent)

private:
	float InnerConeAngle;
	float OuterConeAngle;
};