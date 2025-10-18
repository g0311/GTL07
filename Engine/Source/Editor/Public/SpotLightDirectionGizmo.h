#pragma once

#include "Core/Public/Object.h"
#include "Editor/Public/EditorPrimitive.h"

class USpotLightComponent;

class USpotLightDirectionGizmo : public UObject
{
public:
	USpotLightDirectionGizmo();
	~USpotLightDirectionGizmo() override = default;

	void UpdateFromSpotLight(USpotLightComponent* InSpotLightComponent);
	void Clear();
	void Render();

private:
	FEditorPrimitive Primitive;
	bool bIsVisible = false;
};
