#pragma once

#include "SceneComponent.h"

UENUM()
enum class ELightComponentType
{
    LightType_Directional = 0,
    LightType_Point       = 1,
    LightType_Spot        = 2,
    LightType_Rect        = 3,
    LightType_Max         = 4
};
DECLARE_ENUM_REFLECTION(ELightComponentType)

UCLASS()
class ULightComponent : public USceneComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(ULightComponent, USceneComponent)

public:
    ULightComponent() = default;

    virtual ~ULightComponent() = default;
    
    /*-----------------------------------------------------------------------------
        UObject Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    virtual UObject* Duplicate() override;
        
    virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

    /*-----------------------------------------------------------------------------
        UActorComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual void BeginPlay() override { Super::BeginPlay(); }
    
    virtual void TickComponent(float DeltaTime) override { Super::TickComponent(DeltaTime); }

    virtual void EndPlay() override { Super::EndPlay(); }

    /*-----------------------------------------------------------------------------
        ULightComponent Features
     -----------------------------------------------------------------------------*/
public:
    // --- Getters & Setters ---

    float GetIntensity() const { return Intensity; }

    FVector GetLightColor() const { return LightColor;}

    virtual ELightComponentType GetLightType() const { return ELightComponentType::LightType_Max; }

    // --- [UE Style] ---

    // virtual FBox GetBoundingBox() const;

    // virtual FSphere GetBoundingSphere() const;
    
    /** @note Sets the light intensity and clamps it to the same range as Unreal Engine (0.0 - 20.0). */
    void SetIntensity(float InIntensity) { Intensity = std::clamp(InIntensity, 0.0f, 20.0f); }

    void SetLightColor(FVector InLightColor) { LightColor = InLightColor; }

private:
    /** Total energy that the light emits. */
    float Intensity = 5.0f;

    /**
     * Filter color of the light.
     * @todo Change type of this variable into FLinearColor
     */
    FVector LightColor = { 1.0f, 1.0f, 1.0f };
};
