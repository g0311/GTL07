#include "pch.h"
#include "Actor/Light/Public/SpotLightActor.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(ASpotLightActor, AActor)

ASpotLightActor::ASpotLightActor()
{
}

UClass* ASpotLightActor::GetDefaultRootComponent()
{
    return USpotLightComponent::StaticClass();
}

void ASpotLightActor::InitializeComponents()
{
    Super::InitializeComponents();

    UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
    Billboard->AttachToComponent(GetRootComponent());
    Billboard->SetIsVisualizationComponent(true);
    Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/SpotLight_64x.png"));
    Billboard->SetScreenSizeScaled(true);
}
