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

	const FVector4 OuterLineColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색 (Outer Cone)
	const FVector4 InnerLineColor(1.0f, 0.5f, 0.0f, 1.0f); // 주황색 (Inner Cone)
	constexpr int NumSegments = 24; // 원을 구성할 선분의 수
	constexpr int NumRadialLines = 8; // 꼭짓점에서 밑면으로 향하는 선의 수

	// SpotLight 정보 가져오기
	FVector Position = InSpotLightComponent->GetWorldLocation();
	FQuaternion Rotation = InSpotLightComponent->GetWorldRotationAsQuaternion();
	float Range = InSpotLightComponent->GetRange();
	float OuterAngle = InSpotLightComponent->GetOuterConeAngleRad();
	float InnerAngle = InSpotLightComponent->GetInnerConeAngleRad();

	// Spotlight는 로컬 X축 방향으로 빛을 쏜다
	FVector Direction = Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));

	// 원뿔의 꼭짓점 (Light Position)
	FVector Apex = Position;

	// 원뿔의 밑면 중심
	FVector BaseCenter = Apex + Direction * Range;

	// 밑면의 반지름 계산
	float OuterBaseRadius = Range * tan(OuterAngle);
	float InnerBaseRadius = Range * tan(InnerAngle);

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

	// Outer Cone: 밑면의 원 위의 점들 생성
	TArray<FVector> OuterBaseVertices;
	OuterBaseVertices.reserve(NumSegments);

	for (int i = 0; i < NumSegments; ++i)
	{
		float Angle = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		FVector PointOnBase = BaseCenter + (Right * cosf(Angle) * OuterBaseRadius) + (Up * sinf(Angle) * OuterBaseRadius);
		OuterBaseVertices.push_back(PointOnBase);
	}

	// 1. Outer Cone: 밑면의 원을 구성하는 선분들
	for (int i = 0; i < NumSegments; ++i)
	{
		Vertices.push_back({ OuterBaseVertices[i], OuterLineColor });
		Vertices.push_back({ OuterBaseVertices[(i + 1) % NumSegments], OuterLineColor });
	}

	// 2. Outer Cone: 꼭짓점에서 밑면으로 향하는 방사형 선분들
	for (int i = 0; i < NumRadialLines; ++i)
	{
		int idx = (i * NumSegments) / NumRadialLines;
		Vertices.push_back({ Apex, OuterLineColor });
		Vertices.push_back({ OuterBaseVertices[idx], OuterLineColor });
	}

	// Inner Cone: 밑면의 원 위의 점들 생성
	TArray<FVector> InnerBaseVertices;
	InnerBaseVertices.reserve(NumSegments);

	for (int i = 0; i < NumSegments; ++i)
	{
		float Angle = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		FVector PointOnBase = BaseCenter + (Right * cosf(Angle) * InnerBaseRadius) + (Up * sinf(Angle) * InnerBaseRadius);
		InnerBaseVertices.push_back(PointOnBase);
	}

	// 3. Inner Cone: 밑면의 원을 구성하는 선분들
	for (int i = 0; i < NumSegments; ++i)
	{
		Vertices.push_back({ InnerBaseVertices[i], InnerLineColor });
		Vertices.push_back({ InnerBaseVertices[(i + 1) % NumSegments], InnerLineColor });
	}

	// 4. Inner Cone: 꼭짓점에서 밑면으로 향하는 방사형 선분들
	for (int i = 0; i < NumRadialLines; ++i)
	{
		int idx = (i * NumSegments) / NumRadialLines;
		Vertices.push_back({ Apex, InnerLineColor });
		Vertices.push_back({ InnerBaseVertices[idx], InnerLineColor });
	}
}