#include "pch.h"
#include "Render/UI/Widget/Light/Public/SpotlightComponentWidget.h"
#include "Component/Light/Public/SpotLightComponent.h"

IMPLEMENT_CLASS(USpotlightComponentWidget, ULightComponentWidget)

void USpotlightComponentWidget::Initialize()
{
}

void USpotlightComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (SpotlightComponent != NewSelectedComponent)
        {
            SpotlightComponent = Cast<USpotLightComponent>(NewSelectedComponent);
        }
    }
}

void USpotlightComponentWidget::RenderWidget()
{
    if (!SpotlightComponent)
    {
        return;
    }

    ImGui::Separator();

    // Light Color
    FVector4 LightColor = SpotlightComponent->GetColor();
    if (ImGui::ColorEdit4("Light Color", &LightColor.X))
    {
        SpotlightComponent->SetColor(LightColor);
    }

    // Intensity
    float Intensity = SpotlightComponent->GetIntensity();
    if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
    {
        SpotlightComponent->SetIntensity(Intensity);
    }

    FSpotLight Spot = SpotlightComponent->GetSpotInfo();
    
    // Recover current values
    float Range   = SpotlightComponent->GetRange();
    float InnerRad   = SpotlightComponent->GetInnerConeAngle();
    float OuterRad   = SpotlightComponent->GetOuterConeAngle();
    float Falloff  = Spot.Falloff;
    
    if (ImGui::DragFloat("Range", &Range, 0.1f, 0.01f, 100000.0f, "%.3f"))
    {
        Range = std::max(Range, 0.01f);
        SpotlightComponent->SetRange(Range);
    }

    // Inner/Outer를 함께 편집 (항상 Inner < Outer)
    if (ImGui::DragFloatRange2("Cone Angle (deg)", &InnerRad, &OuterRad, 0.1f, 0.0f, 179.9f, "Inner: %.1f", "Outer: %.1f"))
    {
        // 강제 제약: 0 <= Inner < Outer <= 179.9
        InnerRad = std::max(0.0f, std::min(InnerRad, 4.0f));
        OuterRad = std::max(InnerRad + 0.1f, std::min(OuterRad, 4.0f));

        SpotlightComponent->SetInnerAngle(InnerRad);
        SpotlightComponent->SetOuterAngle(OuterRad);
    }

    if (ImGui::DragFloat("Falloff", &Falloff, 0.1f, 0.0f, 128.0f, "%.2f"))
    {
        Falloff = std::max(0.0f, Falloff);
        SpotlightComponent->SetFalloff(Falloff);
    }

    // 디버그 읽기용: Direction/Angles 표시 (읽기 전용)
    if (ImGui::TreeNode("Debug (read-only)"))
    {
        ImGui::Text("Dir: (%.3f, %.3f, %.3f)", Spot.Direction.X, Spot.Direction.Y, Spot.Direction.Z);
        ImGui::Text("CosInner: %.6f  CosOuter: %.6f", Spot.CosInner, Spot.CosOuter);
        ImGui::Text("Inner: %.2f deg  Outer: %.2f deg", InnerRad, OuterRad);
        ImGui::Text("InvRange2: %.6e  Range: %.3f", Spot.InvRange2, Range);
        ImGui::TreePop();
    }
}

