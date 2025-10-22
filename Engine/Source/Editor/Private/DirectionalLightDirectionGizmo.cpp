#include "pch.h"
#include "Editor/Public/DirectionalLightDirectionGizmo.h"

#include <algorithm>

#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Global/Quaternion.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"

namespace
{
	constexpr float GizmoScaleFactor = 1.0f / 3.0f;
	constexpr float BaseArrowLength = 18.0f;
	constexpr float BaseArrowThickness = 15.0f;
}

UDirectionalLightDirectionGizmo::UDirectionalLightDirectionGizmo()
{
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	Primitive.InputLayout = URenderer::GetInstance().GetGizmoInputLayout();
	Primitive.VertexShader = URenderer::GetInstance().GetGizmoVertexShader();
	Primitive.PixelShader = URenderer::GetInstance().GetGizmoPixelShader();
	Primitive.VertexBuffer = AssetManager.GetVertexbuffer(EPrimitiveType::Arrow);
	Primitive.NumVertices = AssetManager.GetNumVertices(EPrimitiveType::Arrow);
	Primitive.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitive.Color = FVector4(0.8f, 0.8f, 0.8f, 1.0f);
	Primitive.Location = FVector::ZeroVector();
	Primitive.Rotation = FQuaternion::Identity();
	Primitive.Scale = FVector(BaseArrowLength * GizmoScaleFactor, BaseArrowThickness * GizmoScaleFactor, BaseArrowThickness * GizmoScaleFactor);
	Primitive.RenderState.CullMode = ECullMode::None;
	Primitive.RenderState.FillMode = EFillMode::Solid;
	Primitive.bShouldAlwaysVisible = true;
}

void UDirectionalLightDirectionGizmo::UpdateFromLight(UDirectionalLightComponent* InLightComponent)
{
	if (!InLightComponent)
	{
		Clear();
		return;
	}

	Primitive.Location = InLightComponent->GetWorldLocation();

	FVector Direction = InLightComponent->GetForwardVector();
	if (!Direction.IsZero())
	{
		Direction.Normalize();
		Primitive.Rotation = FQuaternion::MakeFromDirection(Direction);
	}
	else
	{
		Primitive.Rotation = FQuaternion::Identity();
	}

	bIsVisible = true;
}

void UDirectionalLightDirectionGizmo::Clear()
{
	bIsVisible = false;
}

void UDirectionalLightDirectionGizmo::Render()
{
	if (!bIsVisible)
	{
		return;
	}

	URenderer::GetInstance().RenderEditorPrimitive(Primitive, Primitive.RenderState);
}
