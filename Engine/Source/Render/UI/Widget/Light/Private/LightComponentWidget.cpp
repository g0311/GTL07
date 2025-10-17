#include "pch.h"
#include "Render/UI/Widget/Light/Public/LightComponentWidget.h"
#include "Component/Light/Public/LightComponent.h"
#include "Component/Public/ActorComponent.h"

IMPLEMENT_CLASS(ULightComponentWidget, UWidget)

void ULightComponentWidget::Initialize()
{
    
}

void ULightComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (LightComponent != NewSelectedComponent)
        {
            LightComponent = Cast<ULightComponent>(NewSelectedComponent);
        }
    }
}

void ULightComponentWidget::RenderWidget()
{
    if (!LightComponent)
    {
        return;
    }

    ImGui::Separator();

    // Light Color
    FVector4 LightColor = LightComponent->GetColor();
    if (ImGui::ColorEdit4("Light Color", &LightColor.X))
    {
        LightComponent->SetColor(LightColor);
    }

    // Intensity
    float Intensity = LightComponent->GetIntensity();
    if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
    {
        LightComponent->SetIntensity(Intensity);
    }
}

