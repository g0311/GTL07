#include "pch.h"
#include "Actor/Light/Public/PointLightActor.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(APointLightActor, AActor)

APointLightActor::APointLightActor()
{
}

UClass* APointLightActor::GetDefaultRootComponent()
{
    return UPointLightComponent::StaticClass();
}

void APointLightActor::InitializeComponents()
{
    Super::InitializeComponents();
	
    UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
    Billboard->AttachToComponent(GetRootComponent());
    Billboard->SetIsVisualizationComponent(true);
    Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/PointLight_64x.png"));
    Billboard->SetScreenSizeScaled(true);
}
