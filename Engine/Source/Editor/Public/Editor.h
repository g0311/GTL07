#pragma once
#include "Core/Public/Object.h"
#include "Editor/Public/Gizmo.h"
#include "Editor/Public/Grid.h"
#include "Editor/public/Axis.h"
#include "Editor/Public/ObjectPicker.h"
#include "Editor/Public/BatchLines.h"
#include "Editor/Public/SplitterWindow.h"

class UPrimitiveComponent;
class UUUIDTextComponent;
class FViewportClient;
class UCamera;
class ULevel;
class USplitterWidget;
struct FRay;

enum class EViewportLayoutState
{
	Multi,
	Single,
	Animating,
};

class UEditor : public UObject
{
public:
	UEditor();
	~UEditor();

	void Update();
	void RenderEditor();
	void RenderGizmo(UCamera* InCamera);

	void SetViewMode(EViewModeIndex InNewViewMode) { CurrentViewMode = InNewViewMode; }
	EViewModeIndex GetViewMode() const { return CurrentViewMode; }

	void SetSingleViewportLayout(int InActiveIndex);
	void RestoreMultiViewportLayout();

	void SelectActor(AActor* InActor);
	AActor* GetSelectedActor() const { return SelectedActor; }
	void SelectComponent(UActorComponent* InComponent);
	UActorComponent* GetSelectedComponent() const { return SelectedComponent; }

// Getter
public:
	UBatchLines* GetBatchLines() { return &BatchLines; }
	UAxis* GetAxis() { return &Axis; }
	UGizmo* GetGizmo() { return &Gizmo; }
	SSplitter* GetRootSplitter() { return &RootSplitter; }
	SSplitter* GetLeftSplitter() { return &LeftSplitter; }
	SSplitter* GetRightSplitter() { return &RightSplitter; }
	
private:
	void InitializeLayout();
	void UpdateBatchLines();
	void ProcessMouseInput();
	void UpdateLayout();

	// 모든 기즈모 드래그 함수가 ActiveCamera를 받도록 통일
	FVector GetGizmoDragLocation(UCamera* InActiveCamera, FRay& WorldRay);
	FVector GetGizmoDragRotation(UCamera* InActiveCamera, FRay& WorldRay);
	FVector GetGizmoDragScale(UCamera* InActiveCamera, FRay& WorldRay);

	inline float Lerp(const float A, const float B, const float Alpha)
	{
		return A * (1 - Alpha) + B * Alpha;
	}

	UObjectPicker ObjectPicker;
	AActor* SelectedActor = nullptr; // 선택된 액터
	UActorComponent* SelectedComponent = nullptr; // 선택된 컴포넌트

	const float MinScale = 0.01f;
	float SavedRootRatio = 0.5f;
	float SavedLeftRatio = 0.5f;
	float SavedRightRatio = 0.5f;
	UGizmo Gizmo;
	UAxis Axis;
	UBatchLines BatchLines;

	SSplitterV RootSplitter;
	SSplitterH LeftSplitter;
	SSplitterH RightSplitter;
	SWindow ViewportWindows[4]; // 최종 뷰포트 영역의 정보, 쉽게 참조하도록 선언했습니다.
	SSplitter* DraggedSplitter = nullptr; // 드래그 상태를 추적하는 포인터
	FViewportClient* InteractionViewport = nullptr; // 뷰포트의 상호작용을 고정하는 포인터

	EViewModeIndex CurrentViewMode = EViewModeIndex::VMI_Lit;

	// Animation
	EViewportLayoutState ViewportLayoutState = EViewportLayoutState::Multi;
	EViewportLayoutState TargetViewportLayoutState = EViewportLayoutState::Multi;
	float AnimationStartTime = 0.0f;
	float AnimationDuration = 0.2f; 
	float SourceRootRatio = 0.5f;
	float SourceLeftRatio = 0.5f;
	float SourceRightRatio = 0.5f;
	float TargetRootRatio = 0.5f;
	float TargetLeftRatio = 0.5f;
	float TargetRightRatio = 0.5f;
};
