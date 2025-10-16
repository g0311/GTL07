#pragma once

#include "Component/Public/PrimitiveComponent.h"
#include "Physics/Public/AABB.h"

class FOctree;

enum class EBoundCheckResult
{
	Outside,
	Intersect,
	Inside
};

struct FFrustum
{
    FVector4 Planes[6];

    EBoundCheckResult CheckIntersection(const FAABB& BBox) const
    {
        EBoundCheckResult Result = EBoundCheckResult::Inside;

        for (int i = 0; i < 6; ++i)
        {
            const FVector4& P = Planes[i];

            FVector Closest(
                P.X > 0 ? BBox.Min.X : BBox.Max.X,
                P.Y > 0 ? BBox.Min.Y : BBox.Max.Y,
                P.Z > 0 ? BBox.Min.Z : BBox.Max.Z
            );

            if (P.Dot3(Closest) + P.W > 0)
            {
                return EBoundCheckResult::Outside; 
            }

            FVector Farthest(
                P.X > 0 ? BBox.Max.X : BBox.Min.X,
                P.Y > 0 ? BBox.Max.Y : BBox.Min.Y,
                P.Z > 0 ? BBox.Max.Z : BBox.Min.Z
            );

            if (P.Dot3(Farthest) + P.W < 0)
            {
                continue; 
            }

            Result = EBoundCheckResult::Intersect;
        }

        return Result;
    }

    void Clear() { for (int i = 0; i < 6; ++i) { Planes[i] = FVector4::Zero(); }; }
};

class ViewVolumeCuller
{
public:
	ViewVolumeCuller() = default;
	~ViewVolumeCuller() = default;
	ViewVolumeCuller(const ViewVolumeCuller& Other) = default;
	ViewVolumeCuller& operator=(const ViewVolumeCuller& Other) = default;

	void Cull(
        FOctree* StaticOctree,
        TArray<UPrimitiveComponent*>& DynamicPrimitives,
		const FCameraConstants& ViewProjConstants
	);

	const TArray<UPrimitiveComponent*>& GetRenderableObjects();
private:
    void CullOctree(FOctree* Octree);

    FFrustum CurrentFrustum{};
    TArray<UPrimitiveComponent*> RenderableObjects{};
};