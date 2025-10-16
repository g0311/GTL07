#include "pch.h"
#include "Render/UI/Widget/Public/ActorDetailWidget.h"
#include "Level/Public/Level.h"
#include "Actor/Public/Actor.h"
#include "Component/Public/ActorComponent.h"
#include "Component/Public/SceneComponent.h"
#include "Component/Public/TextComponent.h"
#include "Global/Vector.h"

IMPLEMENT_CLASS(UActorDetailWidget, UWidget)
UActorDetailWidget::UActorDetailWidget()
{
	LoadComponentClasses();
}

UActorDetailWidget::~UActorDetailWidget() = default;

void UActorDetailWidget::Initialize()
{
	UE_LOG("ActorDetailWidget: Initialized");
}

void UActorDetailWidget::Update()
{
	// 특별한 업데이트 로직이 필요하면 여기에 추가
}

void UActorDetailWidget::RenderWidget()
{
	ULevel* CurrentLevel = GWorld->GetLevel();

	if (!CurrentLevel)
	{
		ImGui::TextUnformatted("No Level Loaded");
		return;
	}

	AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
	if (!SelectedActor)
	{
		ImGui::TextUnformatted("No Object Selected");
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Detail 확인을 위해 Object를 선택해주세요");
		SelectedComponent = nullptr; 
		CachedSelectedActor = nullptr;
		return;
	}

	// 선택된 액터가 변경되면, 컴포넌트 선택 상태를 초기화
	if (CachedSelectedActor != SelectedActor)
	{
		CachedSelectedActor = SelectedActor;
		SelectedComponent = SelectedActor->GetRootComponent();
	}

	    // Actor 헤더 렌더링 (이름 + rename 기능)
	    RenderActorHeader(SelectedActor);
	
	    ImGui::Separator();
	
	    if (ImGui::CollapsingHeader("Tick Settings"))
	    {
	        bool bCanEverTick = SelectedActor->CanTick();
	        if (ImGui::Checkbox("Enable Tick", &bCanEverTick))
	        {
	            SelectedActor->SetCanTick(bCanEverTick);
	        }
	
	        bool bTickInEditor = SelectedActor->CanTickInEditor();
	        if (ImGui::Checkbox("Tick in Editor", &bTickInEditor))
	        {
	            SelectedActor->SetTickInEditor(bTickInEditor);
	        }
	    }
	ImGui::Separator();

	// 컴포넌트 트리 렌더링
	RenderComponents(SelectedActor);
	
	// 선택된 컴포넌트의 트랜스폼 정보 렌더링
	RenderTransformEdit();
}

void UActorDetailWidget::RenderActorHeader(AActor* InSelectedActor)
{
	if (!InSelectedActor)
	{
		return;
	}

	FName ActorName = InSelectedActor->GetName();
	FString ActorDisplayName = ActorName.ToString();

	ImGui::Text("[A]");
	ImGui::SameLine();

	if (bIsRenamingActor)
	{
		// Rename 모드
		ImGui::SetKeyboardFocusHere();
		if (ImGui::InputText("##ActorRename", ActorNameBuffer, sizeof(ActorNameBuffer),
		                     ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			FinishRenamingActor(InSelectedActor);
		}

		// ESC로 취소, InputManager보다 일단 내부 API로 입력 받는 것으로 처리
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			CancelRenamingActor();
		}
	}
	else
	{
		// 더블클릭으로 rename 시작
		ImGui::Text("%s", ActorDisplayName.data());

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			StartRenamingActor(InSelectedActor);
		}

		// 툴팁 UI
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Double-Click to Rename");
		}
	}
}

/**
 * @brief 컴포넌트들 렌더(SceneComponent는 트리 형태로, 아닌 것은 일반 형태로
 * @param InSelectedActor 선택된 Actor
 */
void UActorDetailWidget::RenderComponents(AActor* InSelectedActor)
{
	if (!InSelectedActor) { return; }

	const auto& Components = InSelectedActor->GetOwnedComponents();

	ImGui::Text("Components (%d)", static_cast<int>(Components.size()));
	RenderAddComponentButton(InSelectedActor);
	ImGui::Separator();

	if (Components.empty())
	{
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No components");
		return;
	}
	
	if (InSelectedActor->GetRootComponent())
	{
		RenderSceneComponents(InSelectedActor->GetRootComponent());
	}
	ImGui::Separator();
	
	bool bHasActorComponents = false;
	for (UActorComponent* Component : Components)
	{
		if (Component->IsA(USceneComponent::StaticClass())) { continue; }
		bHasActorComponents = true;
		RenderActorComponent(Component);
	}
	if (bHasActorComponents) { ImGui::Separator(); }
}

void UActorDetailWidget::RenderSceneComponents(USceneComponent* InSceneComponent)
{
	if (!InSceneComponent || InSceneComponent->IsVisualizationComponent()) { return; }
	FString ComponentName = InSceneComponent->GetName().ToString();

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!InSceneComponent || InSceneComponent->GetChildren().empty())
		NodeFlags |= ImGuiTreeNodeFlags_Leaf;
	if (SelectedComponent == InSceneComponent)
		NodeFlags |= ImGuiTreeNodeFlags_Selected;

	bool bNodeOpen = ImGui::TreeNodeEx((void*)InSceneComponent, NodeFlags, "%s", ComponentName.c_str());

	// -----------------------------
	// Drag Source
	// -----------------------------
	if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			ImGui::SetDragDropPayload("COMPONENT_PTR", &InSceneComponent, sizeof(USceneComponent*));
			ImGui::Text("Dragging %s", ComponentName.c_str());
			ImGui::EndDragDropSource();
		}
	}

	// -----------------------------
	// Drag Target
	// -----------------------------
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("COMPONENT_PTR"))
		{
			IM_ASSERT(payload->DataSize == sizeof(USceneComponent*));
			USceneComponent* DraggedComp = *(USceneComponent**)payload->Data;

			if (!DraggedComp || DraggedComp == InSceneComponent)
			{
				ImGui::EndDragDropTarget();		ImGui::TreePop();
				return;
			}

			AActor* Owner = DraggedComp->GetOwner();
			if (!Owner)
			{
				ImGui::EndDragDropTarget();		ImGui::TreePop();
				return;
			}
			
			if (DraggedComp->GetAttachParent() == InSceneComponent)
			{
				ImGui::EndDragDropTarget();		ImGui::TreePop();
				return;
			}
			
			// -----------------------------
			// 자기 자신이나 자식에게 Drop 방지
			// -----------------------------
			if (DraggedComp && InSceneComponent)
			{
				USceneComponent* Iter = InSceneComponent;
				while (Iter)
				{
					if (Iter == DraggedComp)
					{
						UE_LOG_WARNING("Cannot drop onto self or own child.");
						ImGui::EndDragDropTarget();
						ImGui::TreePop();

						return;
					}
					Iter = Iter->GetAttachParent();
				}
			}

			// -----------------------------
			// 부모-자식 관계 설정
			// -----------------------------
			if (DraggedComp)
			{
				// 드롭 대상이 유효한 SceneComponent가 아니면, 작업을 진행하지 않습니다.
				if (!InSceneComponent)
				{
					ImGui::EndDragDropTarget();		ImGui::TreePop();
					return;
				}

				// 자기 부모에게 드롭하는 경우, 아무 작업도 하지 않음
				if (InSceneComponent == DraggedComp->GetAttachParent())
				{
					ImGui::EndDragDropTarget();		ImGui::TreePop();
					return;
				}

				// 1. 이전 부모로부터 분리
				if (USceneComponent* OldParent = DraggedComp->GetAttachParent())
				{
					DraggedComp->DetachFromComponent();
				}

				// 2. 새로운 부모에 연결하고 월드 트랜스폼 유지
				const FMatrix OldWorldMatrix = DraggedComp->GetWorldTransformMatrix();
				DraggedComp->AttachToComponent(InSceneComponent);
				const FMatrix NewParentWorldMatrixInverse = InSceneComponent->GetWorldTransformMatrixInverse();
				const FMatrix NewLocalMatrix = OldWorldMatrix * NewParentWorldMatrixInverse;

				FVector NewLocation, NewRotation, NewScale;
				DecomposeMatrix(NewLocalMatrix, NewLocation, NewRotation, NewScale);

				DraggedComp->SetRelativeLocation(NewLocation);
				DraggedComp->SetRelativeRotation(FQuaternion::FromEuler(NewRotation));
				DraggedComp->SetRelativeScale3D(NewScale);
			}
		}
		ImGui::EndDragDropTarget();
	}

	// -----------------------------
	// 클릭 선택 처리
	// -----------------------------
	if (ImGui::IsItemClicked())
	{
		SelectedComponent = InSceneComponent;
		GEditor->GetEditorModule()->SelectComponent(SelectedComponent);
	}

	// -----------------------------
	// 자식 재귀 렌더링
	// -----------------------------
	if (bNodeOpen)
	{
		if (InSceneComponent)
		{
			for (auto& Child : InSceneComponent->GetChildren())
			{
				RenderSceneComponents(Child);
			}
		}
		ImGui::TreePop();
	}
}

/**
 * @brief SceneComponent가 아닌 ActorComponent를 렌더
 * @param InActorComponent 띄울 ActorComponent
 */
void UActorDetailWidget::RenderActorComponent(UActorComponent* InActorComponent)
{
	FString ComponentName = InActorComponent->GetName().ToString();

	// --- 노드 속성 설정 (항상 잎 노드) ---
	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (SelectedComponent == InActorComponent)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	bool bNodeOpen = ImGui::TreeNodeEx((void*)InActorComponent, NodeFlags, "%s", ComponentName.c_str());

	// --- 클릭 선택 처리 ---
	if (ImGui::IsItemClicked())
	{
		SelectedComponent = InActorComponent;
		GEditor->GetEditorModule()->SelectComponent(SelectedComponent);
	}
    
	if (bNodeOpen)
	{
		ImGui::TreePop();
	}
}

void UActorDetailWidget::RenderAddComponentButton(AActor* InSelectedActor)
{
	ImGui::SameLine();

	const char* ButtonText = "[+]";
	float ButtonWidth = ImGui::CalcTextSize(ButtonText).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);

	if (ImGui::Button(ButtonText))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}
    ImGui::SetNextWindowContentSize(ImVec2(200.0f, 0.0f)); 
	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		ImGui::Text("Add Component");
		ImGui::Separator();
		
		for (auto& Pair : ComponentClasses)
		{
			const char* CName = Pair.first.c_str();
			if (CenteredSelectable(CName))
			{
				AddComponentByName(InSelectedActor, Pair.first);
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndPopup();
	}
}

bool UActorDetailWidget::CenteredSelectable(const char* label)
{
	// 1. 라벨이 보이지 않는 Selectable을 전체 너비로 만들어 호버링/클릭 영역을 잡습니다.
	bool clicked = ImGui::Selectable(std::string("##").append(label).c_str());

	// 2. 방금 만든 Selectable의 사각형 영역 정보를 가져옵니다.
	ImVec2 minRect = ImGui::GetItemRectMin();
	ImVec2 maxRect = ImGui::GetItemRectMax();

	// 3. 텍스트 크기를 계산합니다.
	ImVec2 textSize = ImGui::CalcTextSize(label);

	// 4. 텍스트를 그릴 중앙 위치를 계산합니다.
	ImVec2 textPos = ImVec2(
		minRect.x + (maxRect.x - minRect.x - textSize.x) * 0.5f,
		minRect.y + (maxRect.y - minRect.y - textSize.y) * 0.5f
	);

	// 5. 계산된 위치에 텍스트를 직접 그립니다.
	ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), label);

	return clicked;
}

void UActorDetailWidget::AddComponentByName(AActor* InSelectedActor, const FString& InComponentName)
{
	if (!InSelectedActor)
	{
		UE_LOG_WARNING("ActorDetailWidget: 컴포넌트를 추가할 액터가 선택되지 않았습니다.");
		return;
	}

	UActorComponent* NewComponent = InSelectedActor->AddComponent(ComponentClasses[InComponentName]); 
	if (!NewComponent)
	{
		UE_LOG_ERROR("ActorDetailWidget: 알 수 없는 컴포넌트 타입 '%s'을(를) 추가할 수 없습니다.", InComponentName.data());
		return;
	}

	if (!NewComponent)
	{
		UE_LOG_ERROR("ActorDetailWidget: '%s' 컴포넌트 생성에 실패했습니다.", InComponentName.data());
		return;
	}

	// 1. 새로 만든 컴포넌트가 SceneComponent인지 확인
	if (USceneComponent* NewSceneComponent = Cast<USceneComponent>(NewComponent))
	{
		// 2. 현재 선택된 컴포넌트가 있고, 그것이 SceneComponent인지 확인
		USceneComponent* ParentSceneComponent = Cast<USceneComponent>(SelectedComponent);

		if (ParentSceneComponent)
		{
			// 3. 선택된 컴포넌트(부모)에 새로 만든 컴포넌트(자식)를 붙임
			//    (SetupAttachment는 UCLASS 내에서 호출하는 것을 가정)
			NewSceneComponent->AttachToComponent(ParentSceneComponent);
			UE_LOG_SUCCESS("'%s'를 '%s'의 자식으로 추가했습니다.", NewComponent->GetName().ToString().data(), ParentSceneComponent->GetName().ToString().data());
		}
		else
		{
			// 4. 선택된 컴포넌트가 없으면 액터의 루트 컴포넌트에 붙임
			NewSceneComponent->AttachToComponent(InSelectedActor->GetRootComponent());
			UE_LOG_SUCCESS("'%s'를 액터의 루트에 추가했습니다.", NewComponent->GetName().ToString().data());
		}
		
		NewSceneComponent->SetRelativeLocation(FVector::Zero());
		NewSceneComponent->SetRelativeRotation(FQuaternion::Identity());
		NewSceneComponent->SetRelativeScale3D({1, 1, 1});
	}
	else
	{
		UE_LOG_SUCCESS("Non-Scene Component '%s'를 액터에 추가했습니다.", NewComponent->GetName().ToString().data());
	}
}

void UActorDetailWidget::StartRenamingActor(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	bIsRenamingActor = true;
	FString CurrentName = InActor->GetName().ToString();
	strncpy_s(ActorNameBuffer, CurrentName.data(), sizeof(ActorNameBuffer) - 1);
	ActorNameBuffer[sizeof(ActorNameBuffer) - 1] = '\0';

	UE_LOG("ActorDetailWidget: '%s' 에 대한 이름 변경 시작", CurrentName.data());
}

void UActorDetailWidget::FinishRenamingActor(AActor* InActor)
{
	if (!InActor || !bIsRenamingActor)
	{
		return;
	}

	FString NewName = ActorNameBuffer;
	if (!NewName.empty() && NewName != InActor->GetName().ToString())
	{
		// Actor 이름 변경
		InActor->SetName(NewName);
		UE_LOG_SUCCESS("ActorDetailWidget: Actor의 이름을 '%s' (으)로 변경하였습니다", NewName.data());
	}

	bIsRenamingActor = false;
	ActorNameBuffer[0] = '\0';
}

void UActorDetailWidget::CancelRenamingActor()
{
	bIsRenamingActor = false;
	ActorNameBuffer[0] = '\0';
	UE_LOG_WARNING("ActorDetailWidget: 이름 변경 취소");
}

void UActorDetailWidget::RenderTransformEdit()
{
	if (!SelectedComponent) { return; }
	
	// --- Component General Properties ---
	ImGui::Text("Component Properties");
	ImGui::PushID(SelectedComponent);

	// bCanEverTick 체크박스
	bool bTickEnabled = SelectedComponent->CanEverTick();
	if (ImGui::Checkbox("Enable Tick (bCanEverTick)", &bTickEnabled))
	{
		SelectedComponent->SetCanEverTick(bTickEnabled);
	}

	// bIsEditorOnly 체크박스
	bool bIsEditorOnly = SelectedComponent->IsEditorOnly();
	if (ImGui::Checkbox("Is Editor Only", &bIsEditorOnly))
	{
		SelectedComponent->SetIsEditorOnly(bIsEditorOnly);
	}

	ImGui::PopID();
	ImGui::Separator();

	// --- SceneComponent Transform Properties ---
	USceneComponent* SceneComponent = Cast<USceneComponent>(SelectedComponent);
	if (!SceneComponent) { return; }

	ImGui::Text("Component Transform");
	ImGui::PushID(SceneComponent);

	// Relative Position
	FVector ComponentPosition = SceneComponent->GetRelativeLocation();
	if (ImGui::DragFloat3("Relative Position", &ComponentPosition.X, 0.1f))
	{
		SceneComponent->SetRelativeLocation(ComponentPosition);
	}

	// Relative Rotation
	FVector ComponentRotation = SceneComponent->GetRelativeRotation().ToEuler();
	if (ImGui::DragFloat3("Relative Rotation", &ComponentRotation.X, 1.0f))
	{
		SceneComponent->SetRelativeRotation(FQuaternion::FromEuler(ComponentRotation));
	}

	// Relative Scale
	FVector ComponentScale = SceneComponent->GetRelativeScale3D();
	bool bUniformScale = SceneComponent->IsUniformScale();
	if (bUniformScale)
	{
		float UniformScale = ComponentScale.X;

		if (ImGui::DragFloat("Relative Scale", &UniformScale, 0.1f))
		{
			SceneComponent->SetRelativeScale3D({UniformScale, UniformScale, UniformScale});
		}
	}
	else
	{
		if (ImGui::DragFloat3("Relative Scale", &ComponentScale.X, 0.1f))
		{
			SceneComponent->SetRelativeScale3D(ComponentScale);
		}
	}
	ImGui::Checkbox("Uniform Scale", &bUniformScale);
	SceneComponent->SetUniformScale(bUniformScale);
	
	ImGui::PopID();
}

void UActorDetailWidget::SwapComponents(UActorComponent* A, UActorComponent* B)
{
	if (!A || !B) return;

	AActor* Owner = A->GetOwner();
	if (!Owner) return;

	auto& Components = Owner->GetOwnedComponents();

	auto ItA = std::find(Components.begin(), Components.end(), A);
	auto ItB = std::find(Components.begin(), Components.end(), B);

	if (ItA != Components.end() && ItB != Components.end())
	{
		// 포인터 임시 저장
		UActorComponent* TempA = *ItA;
		UActorComponent* TempB = *ItB;

		// erase 후 push_back
		Components.erase(ItA); // 먼저 A 제거
		// B 제거 (A 제거 후 iterator invalid 되므로 다시 찾기)
		ItB = std::find(Components.begin(), Components.end(), B);
		Components.erase(ItB);

		// 서로 위치를 바꿔 push_back
		Components.push_back(TempA);
		Components.push_back(TempB);

		// SceneComponent라면 부모/자식 관계 교체
		if (USceneComponent* SceneA = Cast<USceneComponent>(A))
			if (USceneComponent* SceneB = Cast<USceneComponent>(B))
			{
				USceneComponent* ParentA = SceneA->GetAttachParent();
				USceneComponent* ParentB = SceneB->GetAttachParent();

				SceneA->AttachToComponent(ParentB);
				SceneB->AttachToComponent(ParentA);
			}
	}
}

void UActorDetailWidget::DecomposeMatrix(const FMatrix& InMatrix, FVector& OutLocation, FVector& OutRotation, FVector& OutScale)
{
    // 스케일 추출
    OutScale.X = FVector(InMatrix.Data[0][0], InMatrix.Data[0][1], InMatrix.Data[0][2]).Length();
    OutScale.Y = FVector(InMatrix.Data[1][0], InMatrix.Data[1][1], InMatrix.Data[1][2]).Length();
    OutScale.Z = FVector(InMatrix.Data[2][0], InMatrix.Data[2][1], InMatrix.Data[2][2]).Length();

    // 위치 추출
    OutLocation.X = InMatrix.Data[3][0];
    OutLocation.Y = InMatrix.Data[3][1];
    OutLocation.Z = InMatrix.Data[3][2];

    // 회전 행렬 추출 (스케일 제거)
    FMatrix RotationMatrix;
	for (int i = 0; i < 3; ++i)
	{
		RotationMatrix.Data[i][0] = InMatrix.Data[i][0] / OutScale.X;
		RotationMatrix.Data[i][1] = InMatrix.Data[i][1] / OutScale.Y;
		RotationMatrix.Data[i][2] = InMatrix.Data[i][2] / OutScale.Z;
	}

    // 오일러 각으로 변환 (Pitch, Yaw, Roll)
    OutRotation.X = atan2(RotationMatrix.Data[2][1], RotationMatrix.Data[2][2]);
    OutRotation.Y = atan2(-RotationMatrix.Data[2][0], sqrt(RotationMatrix.Data[2][1] * RotationMatrix.Data[2][1] + RotationMatrix.Data[2][2] * RotationMatrix.Data[2][2]));
    OutRotation.Z = atan2(RotationMatrix.Data[1][0], RotationMatrix.Data[0][0]);

	OutRotation = FVector::GetRadianToDegree(OutRotation);
}

void UActorDetailWidget::SetSelectedComponent(UActorComponent* InComponent)
{ 
	SelectedComponent = InComponent;
}

void UActorDetailWidget::LoadComponentClasses()
{
	for (UClass* Class : UClass::FindClasses(UActorComponent::StaticClass()))
	{
		if (!Class->IsAbstract())
		{
			ComponentClasses[Class->GetName().ToString().substr(1)] = Class;
		}
	}
}