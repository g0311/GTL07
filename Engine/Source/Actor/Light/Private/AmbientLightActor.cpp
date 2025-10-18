#include "pch.h"
#include "Actor/Light/Public/AmbientLightActor.h"
#include "Component/Light/Public/AmbientLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(AAmbientLightActor, AActor)

AAmbientLightActor::AAmbientLightActor()
{
}

UClass* AAmbientLightActor::GetDefaultRootComponent()
{
    return UAmbientLightComponent::StaticClass();
}

void AAmbientLightActor::InitializeComponents()
{
    Super::InitializeComponents();

    UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
    Billboard->AttachToComponent(GetRootComponent());
    Billboard->SetIsVisualizationComponent(true);
    Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/AmbientLight_64x.png"));
    Billboard->SetScreenSizeScaled(true);
}
