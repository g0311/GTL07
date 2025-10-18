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
    
    float Range   = SpotlightComponent->GetRange();
    float InnerRad   = SpotlightComponent->GetInnerConeAngleRad();
    float OuterRad   = SpotlightComponent->GetOuterConeAngleRad();
    float Falloff  = Spot.Falloff;
    
    const float RAD_TO_DEG = 180.0f / 3.14159265359f;
    const float DEG_TO_RAD = 3.14159265359f / 180.0f;
    float InnerDeg = InnerRad * RAD_TO_DEG;
    float OuterDeg = OuterRad * RAD_TO_DEG;

    if (ImGui::DragFloat("Range", &Range, 0.1f, 0.01f, 100000.0f, "%.3f"))
    {
        Range = std::max(Range, 0.01f);
        SpotlightComponent->SetRange(Range);
    }
    
    if (ImGui::DragFloatRange2("Cone Angle (deg)", &InnerDeg, &OuterDeg, 0.5f, 0.0f, FLT_MAX, "Inner: %.1f", "Outer: %.1f"))
    {
        // Clamp to valid spotlight range [0, 179.9] degrees
        InnerDeg = std::max(0.0f, std::min(InnerDeg, 179.9f));
        OuterDeg = std::max(0.0f, std::min(OuterDeg, 179.9f));

        // Ensure Inner < Outer
        if (OuterDeg <= InnerDeg)
        {
            OuterDeg = std::min(InnerDeg + 0.1f, 179.9f);
        }
        
        InnerRad = InnerDeg * DEG_TO_RAD;
        OuterRad = OuterDeg * DEG_TO_RAD;

        SpotlightComponent->SetInnerAngleRad(InnerRad);
        SpotlightComponent->SetOuterAngleRad(OuterRad);
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
        ImGui::Text("Inner: %.2f deg (%.4f rad)", InnerDeg, InnerRad);
        ImGui::Text("Outer: %.2f deg (%.4f rad)", OuterDeg, OuterRad);
        ImGui::Text("InvRange2: %.6e  Range: %.3f", Spot.InvRange2, Range);
        ImGui::TreePop();
    }
}

