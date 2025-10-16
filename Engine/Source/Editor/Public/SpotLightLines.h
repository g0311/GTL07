#pragma once
#include "Global/Types.h"
#include "Component/Public/DecalSpotLightComponent.h"

class USpotLightLines
{
public:
	USpotLightLines();
	~USpotLightLines();

	void UpdateVertices(UDecalSpotLightComponent* InSpotLightComponent);

	const TArray<FVertex>& GetVertices() const { return Vertices; }

	void ClearVertices() { Vertices.clear(); }

private:
	TArray<FVertex> Vertices;
};
