#include "pch.h"
#include "Editor/Public/SpotLightLines.h"
#include "Component/Light/Public/SpotLightComponent.h"
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

void USpotLightLines::UpdateVertices(USpotLightComponent* InSpotLightComponent)
{
	Vertices.clear();

	if (!InSpotLightComponent)
	{
		return;
	}

	const FVector4 LineColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색
	constexpr int NumSegments = 24; // 원을 구성할 선분의 수
	constexpr int NumRadialLines = 8; // 꼭짓점에서 밑면으로 향하는 선의 수

	// SpotLight 정보 가져오기
	FVector Position = InSpotLightComponent->GetWorldLocation();
	FQuaternion Rotation = InSpotLightComponent->GetWorldRotationAsQuaternion();
	float Range = InSpotLightComponent->GetRange();
	float OuterAngle = InSpotLightComponent->GetOuterConeAngle();

	// Spotlight는 로컬 X축 방향으로 빛을 쏜다
	FVector Direction = Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));

	// 원뿔의 꼭짓점 (Light Position)
	FVector Apex = Position;

	// 원뿔의 밑면 중심
	FVector BaseCenter = Apex + Direction * Range;

	// 밑면의 반지름 계산
	float BaseRadius = Range * tan(OuterAngle);

	// 로컬 Y, Z 축 (원을 그리기 위한 기저)
	FVector Up = FVector(0.0f, 0.0f, 1.0f);
	if (fabs(Direction.Dot(Up)) > 0.99f)
	{
		Up = FVector(0.0f, 1.0f, 0.0f);
	}
	FVector Right = Direction.Cross(Up);
	Right.Normalize();
	Up = Right.Cross(Direction);
	Up.Normalize();

	// 밑면의 원 위의 점들 생성
	TArray<FVector> BaseVertices;
	BaseVertices.reserve(NumSegments);

	for (int i = 0; i < NumSegments; ++i)
	{
		float Angle = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		FVector PointOnBase = BaseCenter + (Right * cosf(Angle) * BaseRadius) + (Up * sinf(Angle) * BaseRadius);
		BaseVertices.push_back(PointOnBase);
	}

	// 1. 밑면의 원을 구성하는 선분들
	for (int i = 0; i < NumSegments; ++i)
	{
		Vertices.push_back({ BaseVertices[i], LineColor });
		Vertices.push_back({ BaseVertices[(i + 1) % NumSegments], LineColor });
	}

	// 2. 꼭짓점에서 밑면으로 향하는 방사형 선분들
	for (int i = 0; i < NumRadialLines; ++i)
	{
		int idx = (i * NumSegments) / NumRadialLines;
		Vertices.push_back({ Apex, LineColor });
		Vertices.push_back({ BaseVertices[idx], LineColor });
	}
}