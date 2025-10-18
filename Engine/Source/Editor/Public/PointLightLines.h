#pragma once
#include "Global/Types.h"

class UPointLightComponent;

class UPointLightLines
{
public:
	UPointLightLines();
	~UPointLightLines();

	void UpdateVertices(UPointLightComponent* InPointLightComponent);

	const TArray<FVertex>& GetVertices() const { return Vertices; }

	void ClearVertices() { Vertices.clear(); }

private:
	void GenerateCircleVertices(const FVector& Center, const FVector& Axis1, const FVector& Axis2,
								float Radius, int NumSegments, const FVector4& Color);
	
	TArray<FVertex> Vertices;
};
