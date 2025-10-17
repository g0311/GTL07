#include "pch.h"
#include "Actor/Light/Public/DirectionalLightActor.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(ADirectionalLightActor, AActor)

ADirectionalLightActor::ADirectionalLightActor()
{
}

UClass* ADirectionalLightActor::GetDefaultRootComponent()
{
    return UDirectionalLightComponent::StaticClass();
}

void ADirectionalLightActor::InitializeComponents()
{
    Super::InitializeComponents();

    UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
    Billboard->AttachToComponent(GetRootComponent());
    Billboard->SetIsVisualizationComponent(true);
    Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/DirectionalLight_64x.png"));
    Billboard->SetScreenSizeScaled(true);
}