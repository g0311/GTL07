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
};