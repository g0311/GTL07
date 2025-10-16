#include "pch.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"
#include "Component/Public/PointLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(UPointLightComponentWidget, UWidget)

void UPointLightComponentWidget::Initialize()
{
}

void UPointLightComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (PointLightComponent != NewSelectedComponent)
        {
            PointLightComponent = Cast<UPointLightComponent>(NewSelectedComponent);
        }
    }
}

void UPointLightComponentWidget::RenderWidget()
{
    if (!PointLightComponent)
    {
        return;
    }

    ImGui::Separator();

    // Light Color
    FVector LightColor = PointLightComponent->GetLightColor();
    if (ImGui::ColorEdit3("Light Color", &LightColor.X))
    {
        PointLightComponent->SetLightColor(LightColor);
    }

    // Intensity
    float Intensity = PointLightComponent->GetIntensity();
    if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
    {
        PointLightComponent->SetIntensity(Intensity);
    }

    // Source Radius
    float SourceRadius = PointLightComponent->GetSourceRadius();
    if (ImGui::DragFloat("Source Radius", &SourceRadius, 0.1f, 0.0f, 1000.0f))
    {
        PointLightComponent->SetSourceRadius(SourceRadius);
    }

    // Light Falloff Extent
    float LightFalloffExtent = PointLightComponent->GetLightFalloffExtent();
    if (ImGui::DragFloat("Light Falloff Extent", &LightFalloffExtent, 0.1f, 2.0f, 16.0f))
    {
        PointLightComponent->SetLightFalloffExtent(LightFalloffExtent);
    }

    ImGui::Separator();
}

