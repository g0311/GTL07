#pragma once
#include "Component/Light/Public/LightComponent.h"

UCLASS()
class UAmbientLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)

public:
	UAmbientLightComponent();

	UClass* GetSpecificWidgetClass() const override;
};
