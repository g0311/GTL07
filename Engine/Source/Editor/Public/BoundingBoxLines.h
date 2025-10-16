#pragma once
#include "Core/Public/Object.h"
#include "Physics/Public/AABB.h"

class UBoundingBoxLines : UObject
{
public:
	UBoundingBoxLines();
	~UBoundingBoxLines() = default;

	void MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex);
	void UpdateVertices(const IBoundingVolume* NewBoundingVolume);
	int32* GetIndices(EBoundingVolumeType BoundingVolumeType);
	uint32 GetNumVertices() const
	{
		return CurrentNumVertices;
	}
	uint32 GetNumIndices(EBoundingVolumeType BoundingVolumeType) const;
	EBoundingVolumeType GetCurrentType() const
	{
		return CurrentType;
	}

	FAABB* GetDisabledBoundingBox() { return &DisabledBoundingBox; }

private:
	TArray<FVector> Vertices;
	uint32 NumVertices = 8;
	uint32 SpotLightVeitices = 61;
	uint32 CurrentNumVertices = NumVertices;
	EBoundingVolumeType CurrentType = EBoundingVolumeType::AABB;
	FAABB DisabledBoundingBox = FAABB(FVector(0, 0, 0), FVector(0, 0, 0));

	int32 BoundingBoxLineIdx[24];
	int32 SpotLightLineIdx[240];
};

