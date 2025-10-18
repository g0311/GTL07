#include "pch.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Global/Quaternion.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

FVector UDirectionalLightComponent::GetForwardVector() const
{
    const FQuaternion WorldRotation = GetWorldRotationAsQuaternion();
    // Assuming X-axis is the 'forward' direction for a light by default
    return WorldRotation.RotateVector(FVector::ForwardVector());
}
