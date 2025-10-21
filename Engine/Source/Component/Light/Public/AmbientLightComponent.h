#pragma once
#include "Component/Light/Public/LightComponent.h"

/**
 * UAmbientLightComponent
 *
 * Represents a global ambient light source that provides constant base illumination
 * across the entire scene, independent of direction or position.
 *
 * Key Property:
 *		- Omnidirectional and position-independent.
 *      - No attenuation with distance or angle.
 */
UCLASS()
class UAmbientLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)

public:
	UAmbientLightComponent();

	UClass* GetSpecificWidgetClass() const override;
};
