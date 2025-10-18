#include "pch.h"
#include "Editor/Public/PointLightLines.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Global/Matrix.h"
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

UPointLightLines::UPointLightLines()
{
}

UPointLightLines::~UPointLightLines()
{
}

void UPointLightLines::UpdateVertices(UPointLightComponent* InPointLightComponent)
{
	Vertices.clear();

	if (!InPointLightComponent)
	{
		return;
	}

	const FVector4 LineColor(0.0f, 1.0f, 1.0f, 1.0f); // 시안색
	constexpr int NumSegments = 32; // 원을 구성할 선분의 수

	// PointLight 정보 가져오기
	FVector Position = InPointLightComponent->GetWorldLocation();
	FQuaternion Rotation = InPointLightComponent->GetWorldRotationAsQuaternion();
	float Radius = InPointLightComponent->GetAttenuationRadius();

	// 로컬 축 계산 (회전 적용)
	FVector LocalX = Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
	FVector LocalY = Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f));
	FVector LocalZ = Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));

	// 1. XY 평면 원 (Z축 기준)
	for (int i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Point1 = Position + (LocalX * cosf(Angle1) * Radius) + (LocalY * sinf(Angle1) * Radius);
		FVector Point2 = Position + (LocalX * cosf(Angle2) * Radius) + (LocalY * sinf(Angle2) * Radius);

		Vertices.push_back({ Point1, LineColor });
		Vertices.push_back({ Point2, LineColor });
	}

	// 2. YZ 평면 원 (X축 기준)
	for (int i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Point1 = Position + (LocalY * cosf(Angle1) * Radius) + (LocalZ * sinf(Angle1) * Radius);
		FVector Point2 = Position + (LocalY * cosf(Angle2) * Radius) + (LocalZ * sinf(Angle2) * Radius);

		Vertices.push_back({ Point1, LineColor });
		Vertices.push_back({ Point2, LineColor });
	}

	// 3. XZ 평면 원 (Y축 기준)
	for (int i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (static_cast<float>(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (static_cast<float>((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;

		FVector Point1 = Position + (LocalX * cosf(Angle1) * Radius) + (LocalZ * sinf(Angle1) * Radius);
		FVector Point2 = Position + (LocalX * cosf(Angle2) * Radius) + (LocalZ * sinf(Angle2) * Radius);

		Vertices.push_back({ Point1, LineColor });
		Vertices.push_back({ Point2, LineColor });
	}
}
