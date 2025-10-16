#include "pch.h"
#include "Render/UI/Widget/Public/DecalTextureSelectionWidget.h"
#include "Component/Public/DecalComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"
#include "Level/Public/Level.h"
#include <filesystem>

IMPLEMENT_CLASS(UDecalTextureSelectionWidget, UWidget)

void UDecalTextureSelectionWidget::Initialize()
{
}

void UDecalTextureSelectionWidget::Update()
{
    // �� ������ Level�� ���õ� Actor�� Ȯ���ؼ� ���� �ݿ�
    ULevel* CurrentLevel = GWorld->GetLevel();

    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();

        // Update Current Selected Actor
        if (DecalComponent != NewSelectedComponent)
        {
            DecalComponent = Cast<UDecalComponent>(NewSelectedComponent);

        }
    }
}

void UDecalTextureSelectionWidget::RenderWidget()
{
    if (!DecalComponent)
    {
        return;
    }

    ImGui::Separator();

    // 좌측 미니 프리뷰 (정사각형)
    const float PreviewSize = 72.0f;
    ID3D11ShaderResourceView* SRV = nullptr;
    UTexture* CurrentTexture = DecalComponent->GetTexture();
    if (CurrentTexture)
    {
        SRV = CurrentTexture->GetTextureSRV();
    }

    if (SRV)
    {
        ImGui::Image((ImTextureID)SRV, ImVec2(PreviewSize, PreviewSize), ImVec2(0, 0), ImVec2(1, 1));
    }
    else
    {
        ImGui::Dummy(ImVec2(PreviewSize, PreviewSize));
    }
    ImGui::SameLine();

    // 현재 스프라이트 경로 표시 (파일명만)
    FString CurrentPath;
    if (CurrentTexture)
    {
        CurrentPath = CurrentTexture->GetFilePath().ToString();
    }

    FString Preview = "<None>";
    if (!CurrentPath.empty())
    {
        Preview = std::filesystem::path(CurrentPath).stem().string();
    }

    if (ImGui::BeginCombo("Texture (png)##Combo", Preview.c_str()))
    {
        const TMap<FName, UTexture*>& TextureCache = UAssetManager::GetInstance().GetTextureCache();
        for (auto const& [Path, Texture] : TextureCache)
        {
            const FString PathStr = Path.ToString();
            const FString DisplayName = std::filesystem::path(PathStr).stem().string();
            bool bSelected = (PathStr == CurrentPath);
            if (ImGui::Selectable(DisplayName.c_str(), bSelected))
            {
                if (!bSelected)
                {
                    DecalComponent->SetTexture(Texture);
                }
            }
            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Fade Controls"))
    {
        if (ImGui::Button("Fade In"))
        {
            DecalComponent->BeginFadeIn();
        }
        ImGui::SameLine();
        if (ImGui::Button("Fade Out"))
        {
            DecalComponent->BeginFade();
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause"))
        {
            DecalComponent->PauseFade();
        }
        ImGui::SameLine();
        if (ImGui::Button("Resume"))
        {
            DecalComponent->ResumeFade();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            DecalComponent->StopFade();
        }

        float FadeProgress = DecalComponent->GetFadeProgress();
        ImGui::ProgressBar(1.0f - FadeProgress, ImVec2(-1.0f, 0.0f));

        ImGui::Separator();

        // Fade Texture Selection
        const float PreviewSize = 72.0f;
        ID3D11ShaderResourceView* FadeSRV = nullptr;
        UTexture* CurrentFadeTexture = DecalComponent->GetFadeTexture();
        if (CurrentFadeTexture)
        {
            FadeSRV = CurrentFadeTexture->GetTextureSRV();
        }

        if (FadeSRV)
        {
            ImGui::Image((ImTextureID)FadeSRV, ImVec2(PreviewSize, PreviewSize), ImVec2(0, 0), ImVec2(1, 1));
        }
        else
        {
            ImGui::Dummy(ImVec2(PreviewSize, PreviewSize));
        }
        ImGui::SameLine();

        FString CurrentFadeTexturePath;
        if (CurrentFadeTexture)
        {
            CurrentFadeTexturePath = CurrentFadeTexture->GetFilePath().ToString();
        }

        FString FadePreview = "<None>";
        if (!CurrentFadeTexturePath.empty())
        {
            FadePreview = std::filesystem::path(CurrentFadeTexturePath).stem().string();
        }

        if (ImGui::BeginCombo("Fade Texture##FadeCombo", FadePreview.c_str()))
        {
            const TMap<FName, UTexture*>& TextureCache = UAssetManager::GetInstance().GetTextureCache();
            for (auto const& [Path, Texture] : TextureCache)
            {
                const FString PathStr = Path.ToString();
                const FString DisplayName = std::filesystem::path(PathStr).stem().string();
                bool bSelected = (PathStr == CurrentFadeTexturePath);
                if (ImGui::Selectable(DisplayName.c_str(), bSelected))
                {
                    if (!bSelected)
                    {
                        DecalComponent->SetFadeTexture(Texture);
                    }
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        ImGui::PushItemWidth(120.0f);

        float FadeInStartDelay = DecalComponent->GetFadeInStartDelay();
        if (ImGui::InputFloat("Fade In Start Delay", &FadeInStartDelay))
        {
            DecalComponent->SetFadeInStartDelay(FadeInStartDelay);
        }

        float FadeInDuration = DecalComponent->GetFadeInDuration();
        if (ImGui::InputFloat("Fade In Duration", &FadeInDuration))
        {
            DecalComponent->SetFadeInDuration(FadeInDuration);
        }

        float FadeStartDelay = DecalComponent->GetFadeStartDelay();
        if (ImGui::InputFloat("Fade Out Start Delay", &FadeStartDelay))
        {
            DecalComponent->SetFadeStartDelay(FadeStartDelay);
        }

        float FadeDuration = DecalComponent->GetFadeDuration();
        if (ImGui::InputFloat("Fade Out Duration", &FadeDuration))
        {
            DecalComponent->SetFadeDuration(FadeDuration);
        }
        ImGui::PopItemWidth();

        bool bDestroyOwner = DecalComponent->GetDestroyOwnerAfterFade();
        if (ImGui::Checkbox("Destroy Owner After Fade", &bDestroyOwner))
        {
            DecalComponent->SetDestroyOwnerAfterFade(bDestroyOwner);
        }
    }

    ImGui::Separator();
    // 1. Get the current state from the component
    bool bPerspective = DecalComponent->IsPerspective();

    // 2. Draw the checkbox. The 'if' statement is true only when the user clicks it.
    if (ImGui::Checkbox("Perspective", &bPerspective))
    {
        // 3. If changed, update the component's state
        DecalComponent->SetPerspective(bPerspective);
    }

}

UDecalTextureSelectionWidget::UDecalTextureSelectionWidget()
{
}

UDecalTextureSelectionWidget::~UDecalTextureSelectionWidget() = default;

void UDecalTextureSelectionWidget::RenderMaterialSections()
{
}

void UDecalTextureSelectionWidget::RenderAvailableMaterials(int32 TargetSlotIdex)
{
}

void UDecalTextureSelectionWidget::RenderOptions()
{
}

FString UDecalTextureSelectionWidget::GetMaterialDisplayName(UMaterial* Material) const
{
    return FString();
}
