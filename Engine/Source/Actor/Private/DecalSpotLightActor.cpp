#include "pch.h"
#include "Actor/Public/DecalSpotLightActor.h"
#include "Component/Public/BillBoardComponent.h"
#include "Component/Public/DecalSpotLightComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(ADecalSpotLightActor, AActor)

ADecalSpotLightActor::ADecalSpotLightActor()
{
}

UClass* ADecalSpotLightActor::GetDefaultRootComponent()
{
	return UDecalSpotLightComponent::StaticClass();
}

void ADecalSpotLightActor::InitializeComponents()
{
	Super::InitializeComponents();
	
	UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
	Billboard->AttachToComponent(GetRootComponent());
	Billboard->SetIsVisualizationComponent(true);
	Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/SpotLight_64x.png"));
	Billboard->SetScreenSizeScaled(true);
}
