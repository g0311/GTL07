#include "pch.h"
#include "Actor/Public/PointLightActor.h"
#include "Component/Public/BillBoardComponent.h"
#include "Component/Public/PointLightComponent.h"
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
