#pragma once
#include "Actor/Public/Actor.h"

UCLASS()
class ASpotLightActor : public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(ASpotLightActor, AActor)

public:
    ASpotLightActor();

    virtual UClass* GetDefaultRootComponent() override;
    virtual void InitializeComponents() override;
};
