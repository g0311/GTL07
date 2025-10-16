#pragma once
#include "Component/Light/Public/LightComponentBase.h"

UCLASS()
class ULightComponent : public ULightComponentBase
{
	GENERATED_BODY()
	DECLARE_CLASS(ULightComponent, ULightComponentBase)

public:
	ULightComponent() {};
	~ULightComponent() override {};
};