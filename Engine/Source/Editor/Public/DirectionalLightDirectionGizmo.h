#pragma once

#include "Core/Public/Object.h"
#include "Editor/Public/EditorPrimitive.h"

class UDirectionalLightComponent;

class UDirectionalLightDirectionGizmo : public UObject
{
public:
	UDirectionalLightDirectionGizmo();
	~UDirectionalLightDirectionGizmo() override = default;

	void UpdateFromLight(UDirectionalLightComponent* InLightComponent);
	void Clear();
	void Render();

private:
	FEditorPrimitive Primitive;
	bool bIsVisible = false;
};
