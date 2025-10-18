#pragma once
#include "Component/Light/Public/LightComponent.h"

UCLASS()
class UClass;
class USpotLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(USpotLightComponent, ULightComponent)

public:
	UClass* GetSpecificWidgetClass() const override;
	
	void SetRange(float InRange) { Range = InRange; }
	void SetOuterAngle(float InOutAngle) { OuterConeAngle = InOutAngle; }
	void SetInnerAngle(float InInnerAngle) { InnerConeAngle = InInnerAngle; }
	void SetFalloff(float InFalloff) { Light.Falloff = InFalloff; }

	float GetRange() { return Range; }
	float GetInnerConeAngle() { return InnerConeAngle; }
	float GetOuterConeAngle() { return OuterConeAngle; }
	
	/* Todo : rename & 통일 Method name && set Dirty Bit*/
	FSpotLight GetSpotInfo()
	{
		Light.Position = GetRelativeLocation();
		Light.InvRange2 = 1/(Range*Range);
		Light.Direction = GetWorldRotationAsQuaternion().RotateVector(FVector(1,0,0));
		Light.CosInner = cos(InnerConeAngle);
		Light.CosOuter = cos(OuterConeAngle);
		return Light;
	}
	
private:
	float InnerConeAngle;
	float OuterConeAngle;
	float Range;

	FSpotLight Light;
};