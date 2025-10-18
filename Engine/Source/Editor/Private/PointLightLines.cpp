#include "pch.h"
#include "Editor/Public/PointLightLines.h"
#include "Component/Light/Public/PointLightComponent.h"

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

    const FVector4 LineColor(0.0f, 1.0f, 1.0f, 1.0f);
    constexpr int NumSegments = 32;
    
    const FVector Position = InPointLightComponent->GetWorldLocation();
    const FQuaternion Rotation = InPointLightComponent->GetWorldRotationAsQuaternion();
    const float Radius = InPointLightComponent->GetAttenuationRadius();

    // 로컬 축 벡터
    const FVector LocalAxes[3] = {
        Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)), // X
        Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)), // Y
        Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f))  // Z
    };

    // 3개 평면의 원 생성 (XY, YZ, XZ)
    struct CirclePlane
    {
        int Axis1Index;
        int Axis2Index;
    };

    constexpr CirclePlane Planes[3] = {
        {0, 1}, // XY 평면 (Z축 기준)
        {1, 2}, // YZ 평면 (X축 기준)
        {0, 2}  // XZ 평면 (Y축 기준)
    };

    for (const auto& Plane : Planes)
    {
        GenerateCircleVertices(
            Position,
            LocalAxes[Plane.Axis1Index],
            LocalAxes[Plane.Axis2Index],
            Radius,
            NumSegments,
            LineColor
        );
    }
}

void UPointLightLines::GenerateCircleVertices(
    const FVector& Center,
    const FVector& Axis1,
    const FVector& Axis2,
    float Radius,
    int NumSegments,
    const FVector4& Color)
{
    const float AngleStep = (2.0f * PI) / static_cast<float>(NumSegments);

    for (int i = 0; i < NumSegments; ++i)
    {
        const float Angle1 = i * AngleStep;
        const float Angle2 = ((i + 1) % NumSegments) * AngleStep;

        const FVector Point1 = Center + 
            (Axis1 * cosf(Angle1) * Radius) + 
            (Axis2 * sinf(Angle1) * Radius);
            
        const FVector Point2 = Center + 
            (Axis1 * cosf(Angle2) * Radius) + 
            (Axis2 * sinf(Angle2) * Radius);

        Vertices.push_back({ Point1, Color });
        Vertices.push_back({ Point2, Color });
    }
}