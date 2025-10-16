#include "pch.h"
#include "Editor/Public/BoundingBoxLines.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/OBB.h"

UBoundingBoxLines::UBoundingBoxLines()
	: Vertices(TArray<FVector>()),
	BoundingBoxLineIdx{
		// 앞면
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		// 뒷면
		4, 5,
		5, 6,
		6, 7,
		7, 4,

		// 옆면 연결
		0, 4,
		1, 5,
		2, 6,
		3, 7
	}
{
	Vertices.reserve(NumVertices);
	UpdateVertices(GetDisabledBoundingBox());

}

void UBoundingBoxLines::MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex)
{
	// 인덱스 범위 보정
	InsertStartIndex = std::min(InsertStartIndex, DestVertices.size());

	// 미리 메모리 확보
	DestVertices.reserve(DestVertices.size() + std::distance(Vertices.begin(), Vertices.end()));

	// 덮어쓸 수 있는 개수 계산
	size_t OverwriteCount = std::min(
		Vertices.size(),
		DestVertices.size() - InsertStartIndex
	);

	// 기존 요소 덮어쓰기
	std::copy(
		Vertices.begin(),
		Vertices.begin() + OverwriteCount,
		DestVertices.begin() + InsertStartIndex
	);
}

void UBoundingBoxLines::UpdateVertices(const IBoundingVolume* NewBoundingVolume)
{
	switch (NewBoundingVolume->GetType())
	{
	case EBoundingVolumeType::AABB:
		{
			const FAABB* AABB = static_cast<const FAABB*>(NewBoundingVolume);

			float MinX = AABB->Min.X, MinY = AABB->Min.Y, MinZ = AABB->Min.Z;
			float MaxX = AABB->Max.X, MaxY = AABB->Max.Y, MaxZ = AABB->Max.Z;

			CurrentType = EBoundingVolumeType::AABB;
			CurrentNumVertices = NumVertices;
			Vertices.resize(NumVertices);

			uint32 Idx = 0;
			Vertices[Idx++] = {MinX, MinY, MinZ}; // Front-Bottom-Left
			Vertices[Idx++] = {MaxX, MinY, MinZ}; // Front-Bottom-Right
			Vertices[Idx++] = {MaxX, MaxY, MinZ}; // Front-Top-Right
			Vertices[Idx++] = {MinX, MaxY, MinZ}; // Front-Top-Left
			Vertices[Idx++] = {MinX, MinY, MaxZ}; // Back-Bottom-Left
			Vertices[Idx++] = {MaxX, MinY, MaxZ}; // Back-Bottom-Right
			Vertices[Idx++] = {MaxX, MaxY, MaxZ}; // Back-Top-Right
			Vertices[Idx] = {MinX, MaxY, MaxZ}; // Back-Top-Left
			break;
		}
	case EBoundingVolumeType::OBB:
		{
			const FOBB* OBB = static_cast<const FOBB*>(NewBoundingVolume);
			const FVector& Extents = OBB->Extents;

			FMatrix OBBToWorld = OBB->ScaleRotation;
			OBBToWorld *= FMatrix::TranslationMatrix(OBB->Center);

			FVector LocalCorners[8] =
			{
				FVector(-Extents.X, -Extents.Y, -Extents.Z), // 0: FBL
				FVector(+Extents.X, -Extents.Y, -Extents.Z), // 1: FBR
				FVector(+Extents.X, +Extents.Y, -Extents.Z), // 2: FTR
				FVector(-Extents.X, +Extents.Y, -Extents.Z), // 3: FTL

				FVector(-Extents.X, -Extents.Y, +Extents.Z), // 4: BBL
				FVector(+Extents.X, -Extents.Y, +Extents.Z), // 5: BBR
				FVector(+Extents.X, +Extents.Y, +Extents.Z), // 6: BTR
				FVector(-Extents.X, +Extents.Y, +Extents.Z)  // 7: BTL
			};

			CurrentType = EBoundingVolumeType::OBB;
			CurrentNumVertices = NumVertices;
			Vertices.resize(NumVertices);

			for (uint32 Idx = 0; Idx < 8; ++Idx)
			{
				FVector WorldCorner = OBBToWorld.TransformPosition(LocalCorners[Idx]);

				Vertices[Idx] = {WorldCorner.X, WorldCorner.Y, WorldCorner.Z};
			}
			break;
		}
	case EBoundingVolumeType::SpotLight:
	{
		const FOBB* OBB = static_cast<const FOBB*>(NewBoundingVolume);
		const FVector& Extents = OBB->Extents;

		FMatrix OBBToWorld = OBB->ScaleRotation;
		OBBToWorld *= FMatrix::TranslationMatrix(OBB->Center);

		// 61개의 점들을 로컬에서 찍는다.
		FVector LocalSpotLight[61];
		LocalSpotLight[0] = FVector(-Extents.X, 0.0f, 0.0f); // 0: Center

		for (int32 i = 0; i < 60;++i)
		{
			LocalSpotLight[i + 1] = FVector(Extents.X, cosf((6.0f * i) * (PI / 180.0f)) * 0.5f, sinf((6.0f * i) * (PI / 180.0f)) * 0.5f); 
		}

		CurrentType = EBoundingVolumeType::SpotLight;
		CurrentNumVertices = SpotLightVeitices;
		Vertices.resize(SpotLightVeitices);

		// 61개의 점을 월드로 변환하고 Vertices에 넣는다.
		// 인덱스에도 넣는다.

		// 0번인덱스는 미리 처리
		FVector WorldCorner = OBBToWorld.TransformPosition(LocalSpotLight[0]);
		Vertices[0] = { WorldCorner.X, WorldCorner.Y, WorldCorner.Z };

		int32 LineIdx = 0;


		// 꼭지점에서 원으로 뻗는 60개의 선을 월드로 바꾸고 인덱스 번호를 지정한다
		for (uint32 Idx = 1; Idx < 61; ++Idx)
		{
			FVector WorldCorner = OBBToWorld.TransformPosition(LocalSpotLight[Idx]);

			Vertices[Idx] = { WorldCorner.X, WorldCorner.Y, WorldCorner.Z };
			SpotLightLineIdx[LineIdx++] = 0;
			SpotLightLineIdx[LineIdx++] = Idx;
		}

		// 원에서 각 점을 잇는 선의 인덱스 번호를 지정한다
		SpotLightLineIdx[LineIdx++] = 60;
		SpotLightLineIdx[LineIdx++] = 1;
		for (uint32 Idx = 1; Idx < 60; ++Idx)
		{
			SpotLightLineIdx[LineIdx++] = Idx;
			SpotLightLineIdx[LineIdx++] = Idx + 1;
		}
		break;

	}
	default:
		break;
	}
}

int32* UBoundingBoxLines::GetIndices(EBoundingVolumeType BoundingVolumeType)
{
	switch (BoundingVolumeType)
	{
	case EBoundingVolumeType::AABB:
	{
		return BoundingBoxLineIdx;
	}
	case EBoundingVolumeType::OBB:
	{
		return BoundingBoxLineIdx;
	}
	case EBoundingVolumeType::SpotLight:
	{
		return SpotLightLineIdx;
	}
	default:
		break;
	}

	return nullptr;
}


uint32 UBoundingBoxLines::GetNumIndices(EBoundingVolumeType BoundingVolumeType) const
{
	switch (BoundingVolumeType)
	{
	case EBoundingVolumeType::AABB:
	case EBoundingVolumeType::OBB:
		return 24;
	case EBoundingVolumeType::SpotLight:
		return 240;
	default:
		break;
	}

	return 0;
}
