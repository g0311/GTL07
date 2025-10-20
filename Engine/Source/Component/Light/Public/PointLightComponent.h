#pragma once
#include "Component/Light/Public/LightComponent.h"

UCLASS()
class UPointLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UPointLightComponent, ULightComponent)

public:
    UPointLightComponent();
    virtual ~UPointLightComponent() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	UObject* Duplicate() override;

	virtual UClass* GetSpecificWidgetClass() const override;

    float GetAttenuationRadius() const { return AttenuationRadius; }
    float GetLightFalloffExponent() const { return LightFalloffExponent; }
    void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }
    void SetLightFalloffExponent(float InExponent) { LightFalloffExponent = InExponent; }

private:
	float AttenuationRadius;
	float LightFalloffExponent;
};
