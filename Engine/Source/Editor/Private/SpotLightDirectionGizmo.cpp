#include "pch.h"
#include "Editor/Public/SpotLightDirectionGizmo.h"

#include <algorithm>

#include "Component/Light/Public/SpotLightComponent.h"
#include "Global/Quaternion.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
namespace
{
	constexpr float GizmoScaleFactor = 1.0f;
	constexpr float GizmoLocateOffset = 1.0f;
}


USpotLightDirectionGizmo::USpotLightDirectionGizmo()
{
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	Primitive.VertexBuffer = AssetManager.GetVertexbuffer(EPrimitiveType::Arrow);
	Primitive.NumVertices = AssetManager.GetNumVertices(EPrimitiveType::Arrow);
	Primitive.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitive.Color = FVector4(1.0f, 0.9f, 0.25f, 1.0f);
	Primitive.Location = FVector::ZeroVector();
	Primitive.Rotation = FQuaternion::Identity();
	Primitive.Scale = FVector(GizmoScaleFactor, GizmoScaleFactor, GizmoScaleFactor);
	Primitive.RenderState.CullMode = ECullMode::None;
	Primitive.RenderState.FillMode = EFillMode::Solid;
	Primitive.bShouldAlwaysVisible = true;
}

void USpotLightDirectionGizmo::UpdateFromSpotLight(USpotLightComponent* InSpotLightComponent)
{
	if (!InSpotLightComponent)
	{
		Clear();
		return;
	}

	FVector Direction = InSpotLightComponent->GetWorldRotationAsQuaternion().RotateVector(FVector::ForwardVector());
	if (!Direction.IsZero())
	{
		Direction.Normalize();
		Primitive.Rotation = FQuaternion::MakeFromDirection(Direction);
	}
	else
	{
		Primitive.Rotation = FQuaternion::Identity();
	}
	Primitive.Location = InSpotLightComponent->GetWorldLocation() + Direction * GizmoLocateOffset;
	const float LightRange = std::max(InSpotLightComponent->GetRange(), 1.0f);
	const float ArrowLength = std::clamp(LightRange, 10.0f, 15.0f);
	const float ThicknessScale = std::clamp(ArrowLength * 0.10f, 5.0f, ArrowLength);
	Primitive.Scale = FVector(ArrowLength * GizmoScaleFactor, ThicknessScale * GizmoScaleFactor, ThicknessScale * GizmoScaleFactor);

	bIsVisible = true;
}

void USpotLightDirectionGizmo::Clear()
{
	bIsVisible = false;
}

void USpotLightDirectionGizmo::Render()
{
	if (!bIsVisible)
	{
		return;
	}

	URenderer::GetInstance().RenderEditorPrimitive(Primitive, Primitive.RenderState);
}
