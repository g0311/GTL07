#pragma once
#include "Component/Public/SceneComponent.h"

UCLASS(abstract)
class ULightComponentBase : public USceneComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(ULightComponentBase, USceneComponent)

public:
	ULightComponentBase() {};
	~ULightComponentBase() override {};

	void SetIntensity(float InIntensity) { Intensity = InIntensity;}
	float GetIntensity() { return Intensity;}

	void SetColor(FVector4 InColor) { Color = InColor;}
	FVector4 GetColor() { return Color;}

	
private:
	float Intensity;
	FVector4 Color;
};