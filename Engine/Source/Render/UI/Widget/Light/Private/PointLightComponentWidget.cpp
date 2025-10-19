#include "pch.h"
#include "Render/UI/Widget/Light/Public/PointLightComponentWidget.h"
#include "Component/Light/Public/PointLightComponent.h"

IMPLEMENT_CLASS(UPointLightComponentWidget, ULightComponentWidget)

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

    FVector4 LightColor = PointLightComponent->GetColor();
    if (ImGui::ColorEdit4("Light Color", &LightColor.X))
    {
        PointLightComponent->SetColor(LightColor);
    }

    float Intensity = PointLightComponent->GetIntensity();
    if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
    {
        PointLightComponent->SetIntensity(Intensity);
    }

    float AttenuationRadius = PointLightComponent->GetAttenuationRadius();
    if (ImGui::DragFloat("Attenuation Radius", &AttenuationRadius, 0.1f, 0.0f, 100000.0f, "%.3f"))
    {
        AttenuationRadius = std::max(AttenuationRadius, 0.0f);
        PointLightComponent->SetAttenuationRadius(AttenuationRadius);
    }

    float Falloff = PointLightComponent->GetLightFalloffExponent();
    if (ImGui::DragFloat("Falloff Exponent", &Falloff, 0.1f, 0.0f, 128.0f, "%.2f"))
    {
        Falloff = std::max(Falloff, 0.0f);
        PointLightComponent->SetLightFalloffExponent(Falloff);
    }
}
