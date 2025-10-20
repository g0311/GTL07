#pragma once
#include "Component/Light/Public/LightComponent.h"

/**
 * UDirectionalLightComponent
 *
 * Represents a directional light source (like sunlight) that emits parallel rays
 * across the entire scene. It has no position, only a direction and color intensity.
 *
 * Key Property:
 *		- Constant illumination direction for all fragments.
 *      - No attenuation with distance.
 */
UCLASS()
class UDirectionalLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	FVector GetForwardVector() const;
};