#include "pch.h"
#include "Editor/Public/SpotLightLines.h"
#include "Physics/Public/OBB.h"
#include "Global/Matrix.h"
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

USpotLightLines::USpotLightLines()
{
}

USpotLightLines::~USpotLightLines()
{
}

void USpotLightLines::UpdateVertices(UDecalSpotLightComponent* InSpotLightComponent)
{
	//Vertices.clear();
	//
	//if (!InSpotLightComponent)
	//{
	//	return;
	//}
	//
	//const FOBB* Fobb = static_cast<const FOBB*>(InSpotLightComponent->GetBoundingBox());
	//if (!Fobb)
	//{
	//	return;
	//}
	//
	//const FMatrix& WorldTransform = InSpotLightComponent->GetWorldTransformMatrix();
	//const FVector4 LineColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색
	//constexpr int NumSegments = 24; // 원을 구성할 선분의 수
	//
	//// OBB 데이터로부터 원뿔의 속성을 계산
	//
	//FVector Scale = InSpotLightComponent->GetWorldScale3D();
	//
	//const float Depth = Scale.X;
	//const float RadiusY = Scale.Y;
	//const float RadiusZ = Fobb->Extents.Z;
	//
	//// 로컬 X축 방향이 원뿔의 방향
	//const FVector LocalDirection(1.f, 0.f, 0.f);
	//const FVector LocalBaseCenter = LocalApex + LocalDirection * Depth;
	//
	//
	//
	//// 월드 좌표계로 변환
	//FVector4 WorldBaseCenter = FVector4(-0.5f, 0.0f, 0.0f, 1.0f) * WorldTransform;
	//float LightLength = Depth * 2;
	//FVector WorldAxisY = WorldTransform.GetUpVector();
	//FVector WorldAxisZ = WorldTransform.GetForwardVector();
	//
	//TArray<FVector> BaseVertices;
	//BaseVertices.reserve(NumSegments);
	//
	//// 원뿔 밑면의 정점들 계산
	//for (int i = 0; i < NumSegments; ++i)
	//{
	//	float Angle = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
	//	FVector PointOnBase = WorldBaseCenter + (WorldAxisY * cosf(Angle) * RadiusY) + (WorldAxisZ * sinf(Angle) * RadiusZ);
	//	BaseVertices.push_back(PointOnBase);
	//}
	//
	//// 선분(라인) 생성
	//for (int i = 0; i < NumSegments; ++i)
	//{
	//	// 밑면의 원을 구성하는 선분
	//	Vertices.push_back({ BaseVertices[i], LineColor });
	//	Vertices.push_back({ BaseVertices[(i + 1) % NumSegments], LineColor });
	//
	//	// 꼭짓점과 밑면을 잇는 선분 (4개만 그려서 간결하게 표현)
	//	if (i % (NumSegments / 4) == 0)
	//	{
	//		Vertices.push_back({ WorldApex, LineColor });
	//		Vertices.push_back({ BaseVertices[i], LineColor });
	//	}
	//}
}