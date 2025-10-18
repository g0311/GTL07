#pragma once
#include "Global/Types.h"

class USpotLightComponent;

class USpotLightLines
{
public:
	USpotLightLines();
	~USpotLightLines();

	void UpdateVertices(USpotLightComponent* InSpotLightComponent);

	const TArray<FVertex>& GetVertices() const { return Vertices; }

	void ClearVertices() { Vertices.clear(); }

private:
	TArray<FVertex> Vertices;
};
