#pragma once
#include "Component/Public/SceneComponent.h"
#include "Physics/Public/BoundingSphere.h"

UCLASS(Abstract)
class ULightComponentBase : public USceneComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(ULightComponentBase, USceneComponent)

public:
	ULightComponentBase() {};
	~ULightComponentBase() override {};

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	UObject* Duplicate() override;

	void SetIntensity(float InIntensity) { Intensity = InIntensity;}
	float GetIntensity() { return Intensity;}

	void SetColor(FVector4 InColor) { Color = InColor;}
	FVector4 GetColor() { return Color;}
	
	virtual FBoundingSphere GetBoundingSphere() { return FBoundingSphere(); };
protected:
	float Intensity =  1.0f;
	FVector4 Color = FVector4(1,1,1,1);
};