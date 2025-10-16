#include "pch.h"
#include "Component/Public/SceneComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Utility/Public/JsonSerializer.h"

#include "Component/Public/PrimitiveComponent.h"
#include "Level/Public/Level.h"

#include <json.hpp>

IMPLEMENT_CLASS(USceneComponent, UActorComponent)

USceneComponent::USceneComponent()
{
}

void USceneComponent::BeginPlay()
{

}

void USceneComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
}
void USceneComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 불러오기
	if (bInIsLoading)
	{
		FJsonSerializer::ReadVector(InOutHandle, "Location", RelativeLocation, FVector::ZeroVector());
		FVector RotationEuler;
		FJsonSerializer::ReadVector(InOutHandle, "Rotation", RotationEuler, FVector::ZeroVector());
		RelativeRotation = FQuaternion::FromEuler(RotationEuler);

		FJsonSerializer::ReadVector(InOutHandle, "Scale", RelativeScale3D, FVector::OneVector());
	}
	// 저장
	else
	{
		InOutHandle["Location"] = FJsonSerializer::VectorToJson(RelativeLocation);
		InOutHandle["Rotation"] = FJsonSerializer::VectorToJson(RelativeRotation.ToEuler());
		InOutHandle["Scale"] = FJsonSerializer::VectorToJson(RelativeScale3D);
	}
}

void USceneComponent::AttachToComponent(USceneComponent* Parent, bool bRemainTransform)
{
	if (!Parent || Parent == this || GetOwner() != Parent->GetOwner()) { return; }
	if (AttachParent)
	{
		AttachParent->DetachChild(this);
	}

	AttachParent = Parent;
	Parent->AttachChildren.push_back(this);

	MarkAsDirty();
}

void USceneComponent::DetachFromComponent()
{
	if (AttachParent)
	{
		AttachParent->DetachChild(this);
		AttachParent = nullptr;
	}
}

void USceneComponent::DetachChild(USceneComponent* ChildToDetach)
{
	AttachChildren.erase(std::remove(AttachChildren.begin(), AttachChildren.end(), ChildToDetach), AttachChildren.end());
}

UObject* USceneComponent::Duplicate()
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(Super::Duplicate());
	SceneComponent->RelativeLocation = RelativeLocation;
	SceneComponent->RelativeRotation = RelativeRotation;
	SceneComponent->RelativeScale3D = RelativeScale3D;
	SceneComponent->MarkAsDirty();
	return SceneComponent;
}

void USceneComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

void USceneComponent::MarkAsDirty()
{
	bIsTransformDirty = true;
	bIsTransformDirtyInverse = true;

	for (USceneComponent* Child : AttachChildren)
	{
		Child->MarkAsDirty();
	}
}

void USceneComponent::SetRelativeLocation(const FVector& Location)
{
	RelativeLocation = Location;
	MarkAsDirty();

	if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(this))
	{
		GWorld->GetLevel()->UpdatePrimitiveInOctree(PrimitiveComponent);
	}
}

void USceneComponent::SetRelativeRotation(const FQuaternion& Rotation)
{
	RelativeRotation = Rotation;
	MarkAsDirty();

	if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(this))
	{
		GWorld->GetLevel()->UpdatePrimitiveInOctree(PrimitiveComponent);
	}
}

void USceneComponent::SetRelativeScale3D(const FVector& Scale)
{
	RelativeScale3D = Scale;
	MarkAsDirty();

	if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(this))
	{
		GWorld->GetLevel()->UpdatePrimitiveInOctree(PrimitiveComponent);
	}
}

const FMatrix& USceneComponent::GetWorldTransformMatrix() const
{
	if (bIsTransformDirty)
	{
		WorldTransformMatrix = FMatrix::GetModelMatrix(RelativeLocation, RelativeRotation, RelativeScale3D);

		if (AttachParent)
		{
			WorldTransformMatrix *= AttachParent->GetWorldTransformMatrix();
		}

		bIsTransformDirty = false;
	}

	return WorldTransformMatrix;
}

const FMatrix& USceneComponent::GetWorldTransformMatrixInverse() const
{
	if (bIsTransformDirtyInverse)
	{
		WorldTransformMatrixInverse = FMatrix::Identity();

		if (AttachParent)
		{
			WorldTransformMatrixInverse *= AttachParent->GetWorldTransformMatrixInverse();
		}

		WorldTransformMatrixInverse *= FMatrix::GetModelMatrixInverse(RelativeLocation, RelativeRotation, RelativeScale3D);

		bIsTransformDirtyInverse = false;
	}

	return WorldTransformMatrixInverse;
}

FVector USceneComponent::GetWorldLocation() const
{
    return GetWorldTransformMatrix().GetLocation();
}

FQuaternion USceneComponent::GetWorldRotationAsQuaternion() const
{
    if (AttachParent)
    {
        return AttachParent->GetWorldRotationAsQuaternion() * RelativeRotation;
    }
    return RelativeRotation;
}

FVector USceneComponent::GetWorldRotation() const
{
    return GetWorldRotationAsQuaternion().ToEuler();
}

FVector USceneComponent::GetWorldScale3D() const
{
    return GetWorldTransformMatrix().GetScale();
}

void USceneComponent::SetWorldLocation(const FVector& NewLocation)
{
    if (AttachParent)
    {
        const FMatrix ParentWorldMatrixInverse = AttachParent->GetWorldTransformMatrixInverse();
        SetRelativeLocation(ParentWorldMatrixInverse.TransformPosition(NewLocation));
    }
    else
    {
        SetRelativeLocation(NewLocation);
    }
}

void USceneComponent::SetWorldRotation(const FVector& NewRotation)
{
    FQuaternion NewWorldRotationQuat = FQuaternion::FromEuler(NewRotation);
    if (AttachParent)
    {
        FQuaternion ParentWorldRotationQuat = AttachParent->GetWorldRotationAsQuaternion();
        SetRelativeRotation(ParentWorldRotationQuat.Inverse() * NewWorldRotationQuat);
    }
    else
    {
        SetRelativeRotation(NewWorldRotationQuat);
    }
}

void USceneComponent::SetWorldRotation(const FQuaternion& NewRotation)
{
	if (AttachParent)
	{
		FQuaternion ParentWorldRotationQuat = AttachParent->GetWorldRotationAsQuaternion();
       SetRelativeRotation(ParentWorldRotationQuat.Inverse() * NewRotation); 
	}
	else
	{
		SetRelativeRotation(NewRotation);
	}
}

void USceneComponent::SetWorldScale3D(const FVector& NewScale)
{
    if (AttachParent)
    {
        const FVector ParentWorldScale = AttachParent->GetWorldScale3D();
        SetRelativeScale3D(FVector(NewScale.X / ParentWorldScale.X, NewScale.Y / ParentWorldScale.Y, NewScale.Z / ParentWorldScale.Z));
    }
    else
    {
        SetRelativeScale3D(NewScale);
    }
}
