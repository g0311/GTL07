#pragma once
#include "Component/Light/Public/LightComponent.h"

/**
 * USpotLightComponent
 *
 * Represents a spotlight source that emits light within a conical shape.
 * a spotlight combines both direction and range.
 *
 * Key Property:
 *		- Inner/Outer cone angles control the intensity gradient of the light cone.
 *      - The light intensity decreases with distance (attenuation) and falloff.
 *		- Direction is determined by the light’s world rotation (forward vector).
 */
UCLASS()
class UClass;
class USpotLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(USpotLightComponent, ULightComponent)

public:
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	UObject* Duplicate() override;

	UClass* GetSpecificWidgetClass() const override;
	
	void SetRange(float InRange) { Range = InRange; }
	void SetOuterAngleRad(float InOutAngleRad) { OuterConeAngleRad = InOutAngleRad; }
	void SetInnerAngleRad(float InInnerAngleRad) { InnerConeAngleRad = InInnerAngleRad; }
	void SetFalloff(float InFalloff) { Light.Falloff = InFalloff; }

	float GetRange() { return Range; }
	float GetInnerConeAngleRad() { return InnerConeAngleRad; }
	float GetOuterConeAngleRad() { return OuterConeAngleRad; }
	float GetFallOff() {return Light.Falloff;}
	
	/* Todo : rename & 통일 Method name && set Dirty Bit*/
	FSpotLightData GetSpotInfo()
	{
		// Use World coordinates for shader calculations
		Light.Position = GetWorldLocation();
		Light.InvRange2 = 1/(Range*Range);
		Light.Direction = GetWorldRotationAsQuaternion().RotateVector(FVector(1,0,0));
		
		Light.CosInner = cos(InnerConeAngleRad);
		Light.CosOuter = cos(OuterConeAngleRad);
		
		if (Light.CosOuter > Light.CosInner)
		{
			float temp = Light.CosOuter;
			Light.CosOuter = Light.CosInner;
			Light.CosInner = temp;

			float temp2 = InnerConeAngleRad;
			InnerConeAngleRad = OuterConeAngleRad;
			OuterConeAngleRad = temp2;
		}

		Light.Color = Color;
		Light.Intensity = Intensity;
		return Light;
	}

private:
	float InnerConeAngleRad = 0.0f; // Stored in radians
	float OuterConeAngleRad = 0.785398f; // 45 degrees default, stored in radians
	float Range = 50.0f;

	FSpotLightData Light;
};