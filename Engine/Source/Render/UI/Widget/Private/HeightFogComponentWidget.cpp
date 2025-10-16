#include "pch.h"
#include "Render/UI/Widget/Public/HeightFogComponentWidget.h"
#include "Component/Public/HeightFogComponent.h"
#include "Component/Public/ActorComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(UHeightFogComponentWidget, UWidget)

void UHeightFogComponentWidget::Initialize()
{
}

void UHeightFogComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (FogComponent != NewSelectedComponent)
        {
            FogComponent = Cast<UHeightFogComponent>(NewSelectedComponent);
        }
    }
}

void UHeightFogComponentWidget::RenderWidget()
{
    if (!FogComponent)
    {
        return;
    }

    ImGui::Separator();
    ImGui::PushItemWidth(150.0f);

    // FogDensity
    float density = FogComponent->GetFogDensity();
    if (ImGui::DragFloat("Fog Density", &density, 0.001f, 0.0f, 1.0f, "%.3f"))
    {
        FogComponent->SetFogDensity(density);
    }

    // FogHeightFalloff
    float heightFalloff = FogComponent->GetFogHeightFalloff();
    if (ImGui::DragFloat("Height Falloff", &heightFalloff, 0.001f, 0.0f, 1.0f, "%.3f"))
    {
        FogComponent->SetFogHeightFalloff(heightFalloff);
    }

    // StartDistance
    float startDistance = FogComponent->GetStartDistance();
    if (ImGui::DragFloat("Start Distance", &startDistance, 0.5f, 0.0f, FLT_MAX, "%.1f"))
    {
        FogComponent->SetStartDistance(startDistance);
    }

    // FogCutoffDistance
    float cutoffDistance = FogComponent->GetFogCutoffDistance();
    if (ImGui::DragFloat("Cutoff Distance", &cutoffDistance, 10.0f, 0.0f, FLT_MAX, "%.1f"))
    {
        FogComponent->SetFogCutoffDistance(cutoffDistance);
    }

    // FogMaxOpacity
    float maxOpacity = FogComponent->GetFogMaxOpacity();
    if (ImGui::DragFloat("Max Opacity", &maxOpacity, 0.01f, 0.0f, 1.0f, "%.2f"))
    {
        FogComponent->SetFogMaxOpacity(maxOpacity);
    }

    // FogInscatteringColor
    FVector color = FogComponent->GetFogInscatteringColor();
    float colorArr[3] = { color.X, color.Y, color.Z };
    if (ImGui::ColorEdit3("Inscattering Color", colorArr))
    {
        FogComponent->SetFogInscatteringColor(FVector(colorArr[0], colorArr[1], colorArr[2]));
    }

    ImGui::PopItemWidth();
    ImGui::Separator();
}