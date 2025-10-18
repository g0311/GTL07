#pragma once
#include "Actor/Public/Actor.h"

UCLASS()
class AAmbientLightActor : public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(AAmbientLightActor, AActor)

public:
    AAmbientLightActor();

    virtual UClass* GetDefaultRootComponent() override;
    virtual void InitializeComponents() override;
};
