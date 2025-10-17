#pragma once
#include "Actor/Public/Actor.h"

UCLASS()
class ADirectionalLightActor : public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(ADirectionalLightActor, AActor)

public:
    ADirectionalLightActor();

    virtual UClass* GetDefaultRootComponent() override;
    virtual void InitializeComponents() override;
};