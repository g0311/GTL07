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
	TArray<FVertex> Vertices;
};
