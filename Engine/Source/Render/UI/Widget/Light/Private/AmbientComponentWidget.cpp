#include "pch.h"
#include "Render/UI/Widget/Light/Public/AmbientComponentWidget.h"
#include "Component/Light/Public/AmbientLightComponent.h"
#include "Component/Public/ActorComponent.h"

IMPLEMENT_CLASS(UAmbientComponentWidget, ULightComponentWidget)

void UAmbientComponentWidget::Initialize()
{
    
}

void UAmbientComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (AmbientLightComponent != NewSelectedComponent)
        {
            AmbientLightComponent = Cast<UAmbientLightComponent>(NewSelectedComponent);
        }
    }
}

void UAmbientComponentWidget::RenderWidget()
{
    
}