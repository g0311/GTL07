#include "pch.h"
#include "Component/Public/TextComponent.h"
#include "Editor/Public/Editor.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/UI/Widget/Public/SetTextComponentWidget.h"

IMPLEMENT_CLASS(UTextComponent, UPrimitiveComponent)

UTextComponent::UTextComponent()
{
	Vertices = &PickingAreaVertex;
	NumVertices = PickingAreaVertex.size();

	Indices = &PickingAreaIndex;
	NumIndices = PickingAreaIndex.size();

	RegulatePickingAreaByTextLength();
}

UTextComponent::~UTextComponent()
{
}

void UTextComponent::UpdateRotationMatrix(const FVector& InCameraLocation) {}
FMatrix UTextComponent::GetRTMatrix() const { return FMatrix(); }

const FString& UTextComponent::GetText() { return Text; }
void UTextComponent::SetText(const FString& InText)
{
	Text = InText;
	RegulatePickingAreaByTextLength();
}

UClass* UTextComponent::GetSpecificWidgetClass() const
{
	return USetTextComponentWidget::StaticClass();
}

UObject* UTextComponent::Duplicate()
{
	UTextComponent* TextComponent = Cast<UTextComponent>(Super::Duplicate());
	TextComponent->Text = Text;
	return TextComponent;
}

void UTextComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

void UTextComponent::RegulatePickingAreaByTextLength()
{
	PickingAreaVertex.clear();
	int32 NewStrLen = Text.size();

	const static TPair<int32, int32> Offset[] =
	{
		{-1, 1},
		{1, 1},
		{-1, -1},
		{1, -1}
	};

	for (int i = 0; i < 4; i++)
	{
		FNormalVertex NewVertex = {
			{0.0f, 0.5f * NewStrLen * Offset[i].first, 0.5f * Offset[i].second},
			{}, {}, {}
		};
		PickingAreaVertex.push_back(NewVertex);
	}

	PickingAreaBoundingBox =
		FAABB(
			FVector(0.0f, -0.5f * NewStrLen, -0.5f),
			FVector(0.0f, 0.5f * NewStrLen, 0.5f)
		);
	BoundingBox = &PickingAreaBoundingBox;
}
