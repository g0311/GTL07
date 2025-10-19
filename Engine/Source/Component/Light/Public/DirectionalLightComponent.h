#pragma once
#include "Component/Light/Public/LightComponent.h"

UCLASS()
class UDirectionalLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	FVector GetForwardVector() const;
};
