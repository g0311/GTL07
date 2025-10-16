#pragma once
#include "Widget.h"

class AActor;
class UActorComponent;
class USceneComponent;

/**
 * @brief 선택된 Actor의 이름과 컴포넌트 트리를 표시하는 Widget
 * Rename 기능이 추가되어 있음
 */
class UActorDetailWidget : public UWidget
{
	DECLARE_CLASS(UActorDetailWidget, UObject);
public:
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	UActorComponent* GetSelectedComponent() const { return SelectedComponent; }
	void SetSelectedComponent(UActorComponent* InComponent);

	// Special Member Function
	UActorDetailWidget();
	~UActorDetailWidget() override;

private:
	bool bIsRenamingActor = false;
	char ActorNameBuffer[256] = {};

	UActorComponent*  SelectedComponent = nullptr;
	AActor* CachedSelectedActor = nullptr;

	// Helper functions
	void RenderActorHeader(AActor* InSelectedActor);
	void RenderComponents(AActor* InSelectedActor);
	void RenderSceneComponents(USceneComponent* InSceneComponent);
	void RenderActorComponent(UActorComponent* InActorComponent);
	void RenderAddComponentButton(AActor* InSelectedActor);
	bool CenteredSelectable(const char* label);
	void AddComponentByName(AActor* InSelectedActor, const FString& InComponentName);
	void RenderTransformEdit();
	void SwapComponents(UActorComponent* A, UActorComponent* B);

	void DecomposeMatrix(const FMatrix& InMatrix, FVector& OutLocation, FVector& OutRotation, FVector& OutScale);

	// 이름 변경 함수
	void StartRenamingActor(AActor* InActor);
	void FinishRenamingActor(AActor* InActor);
	void CancelRenamingActor();

	// Actor Class
	void LoadComponentClasses();
	
	TMap<FString, UClass*> ComponentClasses;
};
