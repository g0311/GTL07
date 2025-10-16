#include "pch.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/Viewport.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Manager/UI/Public/UIManager.h"
#include "Manager/Input/Public/InputManager.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Manager/Time/Public/TimeManager.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Level/Public/Level.h"
#include "Global/Quaternion.h"
#include "Utility/Public/ScopeCycleCounter.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Component/Public/DecalSpotLightComponent.h"

UEditor::UEditor()
{
	const TArray<float>& SplitterRatio = UConfigManager::GetInstance().GetSplitterRatio();
	RootSplitter.SetRatio(SplitterRatio[0]);
	LeftSplitter.SetRatio(SplitterRatio[1]);
	RightSplitter.SetRatio(SplitterRatio[2]);

	InitializeLayout();
}

UEditor::~UEditor()
{
	UConfigManager::GetInstance().SetSplitterRatio(RootSplitter.GetRatio(), LeftSplitter.GetRatio(), RightSplitter.GetRatio());
	SafeDelete(DraggedSplitter);
	SafeDelete(InteractionViewport);
}

void UEditor::Update()
{
	URenderer& Renderer = URenderer::GetInstance();
	FViewport* Viewport = Renderer.GetViewportClient();

	// 1. 마우스 위치를 기반으로 활성 뷰포트를 결정합니다.
	Viewport->UpdateActiveViewportClient(UInputManager::GetInstance().GetMousePosition());

	// 2. 활성 뷰포트의 카메라의 제어만 업데이트합니다.
	if (UCamera* ActiveCamera = Viewport->GetActiveCamera())
	{
		// 만약 이동량이 있고, 직교 카메라라면 ViewportClient에 알립니다.
		const FVector MovementDelta = ActiveCamera->UpdateInput();
		if (MovementDelta.LengthSquared() > 0.f && ActiveCamera->GetCameraType() == ECameraType::ECT_Orthographic)
		{
			Viewport->UpdateOrthoFocusPointByDelta(MovementDelta);
		}
	}

	UpdateBatchLines();
	BatchLines.UpdateVertexBuffer();

	ProcessMouseInput();

	UpdateLayout();
}

void UEditor::RenderEditor()
{
	if (GEditor->IsPIESessionActive()) { return; }
	BatchLines.Render();
	Axis.Render();
}

void UEditor::RenderGizmo(UCamera* InCamera)
{
	if (GEditor->IsPIESessionActive()) { return; }
	Gizmo.RenderGizmo(InCamera);
}

void UEditor::SetSingleViewportLayout(int InActiveIndex)
{
	if (ViewportLayoutState == EViewportLayoutState::Animating) return;

	if (ViewportLayoutState == EViewportLayoutState::Multi)
	{
		SavedRootRatio = RootSplitter.GetRatio();
		SavedLeftRatio = LeftSplitter.GetRatio();
		SavedRightRatio = RightSplitter.GetRatio();
	}

	SourceRootRatio = RootSplitter.GetRatio();
	SourceLeftRatio = LeftSplitter.GetRatio();
	SourceRightRatio = RightSplitter.GetRatio();

	TargetRootRatio = SourceRootRatio;
	TargetLeftRatio = SourceLeftRatio;
	TargetRightRatio = SourceRightRatio;

	switch (InActiveIndex)
	{
	case 0: // 좌상단
		TargetRootRatio = 1.0f;
		TargetLeftRatio = 1.0f;
		break;
	case 1: // 좌하단
		TargetRootRatio = 1.0f;
		TargetLeftRatio = 0.0f;
		break;
	case 2: // 우상단
		TargetRootRatio = 0.0f;
		TargetRightRatio = 1.0f;
		break;
	case 3: // 우하단
		TargetRootRatio = 0.0f;
		TargetRightRatio = 0.0f;
		break;
	default:
		RestoreMultiViewportLayout();
		return;
	}

	ViewportLayoutState = EViewportLayoutState::Animating;
	TargetViewportLayoutState = EViewportLayoutState::Single;
	AnimationStartTime = UTimeManager::GetInstance().GetGameTime();
}

/**
 * @brief 저장된 비율을 사용하여 4분할 뷰포트 레이아웃으로 복원합니다.
 */
void UEditor::RestoreMultiViewportLayout()
{
	if (ViewportLayoutState == EViewportLayoutState::Animating) return;

	SourceRootRatio = RootSplitter.GetRatio();
	SourceLeftRatio = LeftSplitter.GetRatio();
	SourceRightRatio = RightSplitter.GetRatio();

	TargetRootRatio = SavedRootRatio;
	TargetLeftRatio = SavedLeftRatio;
	TargetRightRatio = SavedRightRatio;

	ViewportLayoutState = EViewportLayoutState::Animating;
	TargetViewportLayoutState = EViewportLayoutState::Multi;
	AnimationStartTime = UTimeManager::GetInstance().GetGameTime();
}

void UEditor::InitializeLayout()
{
	// 1. 루트 스플리터의 자식으로 2개의 수평 스플리터를 '주소'로 연결합니다.
	RootSplitter.SetChildren(&LeftSplitter, &RightSplitter);
	RootSplitter.SetRatio(0);

	// 2. 각 수평 스플리터의 자식으로 뷰포트 윈도우들을 '주소'로 연결합니다.
	LeftSplitter.SetChildren(&ViewportWindows[0], &ViewportWindows[1]);
	LeftSplitter.SetRatio(0);
	RightSplitter.SetChildren(&ViewportWindows[2], &ViewportWindows[3]);
	RightSplitter.SetRatio(0);

	// 3. 초기 레이아웃 계산
	const D3D11_VIEWPORT& ViewportInfo = URenderer::GetInstance().GetDeviceResources()->GetViewportInfo();
	FRect FullScreenRect = { ViewportInfo.TopLeftX, ViewportInfo.TopLeftY, ViewportInfo.Width, ViewportInfo.Height };
	RootSplitter.Resize(FullScreenRect);
}

void UEditor::UpdateBatchLines()
{
	uint64 ShowFlags = GWorld->GetLevel()->GetShowFlags();

	if (ShowFlags & EEngineShowFlags::SF_Octree)
	{
		BatchLines.UpdateOctreeVertices(GWorld->GetLevel()->GetStaticOctree());
	}
	else
	{
		// If we are not showing the octree, clear the lines, so they don't persist
		BatchLines.ClearOctreeLines();
	}

	if (UActorComponent* Component = GetSelectedComponent())
	{
		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			if (ShowFlags & EEngineShowFlags::SF_Bounds)
			{
				if (PrimitiveComponent->GetBoundingBox()->GetType() == EBoundingVolumeType::AABB)
				{
					FVector WorldMin, WorldMax; PrimitiveComponent->GetWorldAABB(WorldMin, WorldMax);
					FAABB AABB(WorldMin, WorldMax);
					BatchLines.UpdateBoundingBoxVertices(&AABB);
				}
				else 
				{ 
					BatchLines.UpdateBoundingBoxVertices(PrimitiveComponent->GetBoundingBox()); 

					// 만약 선택된 타입이 decalspotlightcomponent라면
					if (Component->IsA(UDecalSpotLightComponent::StaticClass()))
					{
						BatchLines.UpdateSpotLightVertices(Cast<UDecalSpotLightComponent>(Component));
					}
				}
				return; 
			}
		}
	}

	BatchLines.DisableRenderBoundingBox();
}

void UEditor::UpdateLayout()
{
	URenderer& Renderer = URenderer::GetInstance();
	UInputManager& Input = UInputManager::GetInstance();
	const FPoint MousePosition = { Input.GetMousePosition().X, Input.GetMousePosition().Y };
	bool bIsHoveredOnSplitter = false;

	// 뷰포트를 전환 중이라면 애니메이션을 적용합니다.
	if (ViewportLayoutState == EViewportLayoutState::Animating)
	{
		float ElapsedTime = UTimeManager::GetInstance().GetGameTime() - AnimationStartTime;
		float Alpha = clamp(ElapsedTime / AnimationDuration, 0.0f, 1.0f);

		float NewRootRatio = Lerp(SourceRootRatio, TargetRootRatio, Alpha);
		float NewLeftRatio = Lerp(SourceLeftRatio, TargetLeftRatio, Alpha);
		float NewRightRatio = Lerp(SourceRightRatio, TargetRightRatio, Alpha);

		RootSplitter.SetRatio(NewRootRatio);
		LeftSplitter.SetRatio(NewLeftRatio);
		RightSplitter.SetRatio(NewRightRatio);

		if (Alpha >= 1.0f)
		{
			ViewportLayoutState = TargetViewportLayoutState;
		}
	}

	// 1. 드래그 상태가 아니라면 커서의 상태를 감지합니다.
	if (DraggedSplitter == nullptr)
	{
		if (LeftSplitter.IsHovered(MousePosition) || RightSplitter.IsHovered(MousePosition))
		{
			bIsHoveredOnSplitter = true;
		}
		else if (RootSplitter.IsHovered(MousePosition))
		{
			bIsHoveredOnSplitter = true;
		}
	}

	// 2. 스플리터 위에 커서가 있으며 클릭을 한다면, 드래그 상태로 활성화합니다.
	if (UInputManager::GetInstance().IsKeyPressed(EKeyInput::MouseLeft) && bIsHoveredOnSplitter)
	{
		// 호버 상태에 따라 드래그할 스플리터를 결정합니다.
		if (LeftSplitter.IsHovered(MousePosition)) { DraggedSplitter = &LeftSplitter; }				// 좌상, 좌하
		else if (RightSplitter.IsHovered(MousePosition)) { DraggedSplitter = &RightSplitter; }		// 우상, 우하
		else if (RootSplitter.IsHovered(MousePosition)) { DraggedSplitter = &RootSplitter; }
	}

	// 3. 드래그 상태라면 스플리터 기능을 이행합니다.
	if (DraggedSplitter)
	{
		const ImGuiViewport* Viewport = ImGui::GetMainViewport();
		FRect WorkableRect = { Viewport->WorkPos.x, Viewport->WorkPos.y, Viewport->WorkSize.x, Viewport->WorkSize.y };

		FRect ParentRect;

		if (DraggedSplitter == &RootSplitter)
		{
			ParentRect = WorkableRect;
		}
		else
		{
			if (DraggedSplitter == &LeftSplitter)
			{
				ParentRect.Left = WorkableRect.Left;
				ParentRect.Top = WorkableRect.Top;
				ParentRect.Width = WorkableRect.Width * RootSplitter.GetRatio();
				ParentRect.Height = WorkableRect.Height;
			}
			else if (DraggedSplitter == &RightSplitter)
			{
				ParentRect.Left = WorkableRect.Left + WorkableRect.Width * RootSplitter.GetRatio();
				ParentRect.Top = WorkableRect.Top;
				ParentRect.Width = WorkableRect.Width * (1.0f - RootSplitter.GetRatio());
				ParentRect.Height = WorkableRect.Height;
			}
		}

		// 마우스 위치를 부모 영역에 대한 비율(0.0 ~ 1.0)로 변환합니다.
		float NewRatio = 0.5f;
		if (dynamic_cast<SSplitterV*>(DraggedSplitter)) // 수직 스플리터
		{
			if (ParentRect.Width > 0)
			{
				NewRatio = (MousePosition.X - ParentRect.Left) / ParentRect.Width;
			}
		}
		else // 수평 스플리터
		{
			if (ParentRect.Height > 0)
			{
				NewRatio = (MousePosition.Y - ParentRect.Top) / ParentRect.Height;
			}
		}

		// 계산된 비율을 스플리터에 적용합니다.
		DraggedSplitter->SetRatio(NewRatio);
	}

	// 4. 매 프레임 현재 비율에 맞게 전체 레이아웃 크기를 다시 계산하고, 그 결과를 실제 FViewport에 반영합니다.
	const ImGuiViewport* Viewport = ImGui::GetMainViewport(); // 사용자에게만 보이는 영역의 정보를 가져옵니다. 
	FRect WorkableRect = { Viewport->WorkPos.x, Viewport->WorkPos.y, Viewport->WorkSize.x, Viewport->WorkSize.y };
	RootSplitter.Resize(WorkableRect);

	if (FViewport* ViewportClient = URenderer::GetInstance().GetViewportClient())
	{
		auto& Viewports = ViewportClient->GetViewports();
		for (int i = 0; i < 4; ++i)
		{
			if (i < Viewports.size())
			{
				const FRect& Rect = ViewportWindows[i].Rect;
				Viewports[i].SetViewportInfo({ Rect.Left, Rect.Top, Rect.Width, Rect.Height, 0.0f, 1.0f });
			}
		}
	}

	// 마우스 클릭 해제를 하면 드래그 비활성화
	if (UInputManager::GetInstance().IsKeyReleased(EKeyInput::MouseLeft)) { DraggedSplitter = nullptr; }
}

void UEditor::ProcessMouseInput()
{
	// 선택된 뷰포트의 정보들을 가져옵니다.
	FViewport* ViewportClient = URenderer::GetInstance().GetViewportClient();
	FViewportClient* CurrentViewport = nullptr;
	UCamera* CurrentCamera = nullptr;

	// 이미 선택된 뷰포트 영역이 존재한다면 선택된 뷰포트 처리를 진행합니다.
	if (InteractionViewport) { CurrentViewport = InteractionViewport; }
	// 선택된 뷰포트 영역이 존재하지 않는다면 현재 마우스 위치의 뷰포트를 선택합니다.
	else { CurrentViewport = ViewportClient->GetActiveViewportClient(); }

	// 처리할 영역이 존재하지 않으면 진행을 중단합니다.
	if (CurrentViewport == nullptr) { return; }

	CurrentCamera = &CurrentViewport->Camera;

	AActor* ActorPicked = GetSelectedActor();
	if (ActorPicked)
	{
		// 피킹 전 현재 카메라에 맞는 기즈모 스케일 업데이트
		Gizmo.UpdateScale(CurrentCamera);
	}

	const UInputManager& InputManager = UInputManager::GetInstance();
	const FVector& MousePos = InputManager.GetMousePosition();
	const D3D11_VIEWPORT& ViewportInfo = CurrentViewport->GetViewportInfo();

	const float NdcX = ((MousePos.X - ViewportInfo.TopLeftX) / ViewportInfo.Width) * 2.0f - 1.0f;
	const float NdcY = -(((MousePos.Y - ViewportInfo.TopLeftY) / ViewportInfo.Height) * 2.0f - 1.0f);

	FRay WorldRay = CurrentCamera->ConvertToWorldRay(NdcX, NdcY);

	static EGizmoDirection PreviousGizmoDirection = EGizmoDirection::None;
	FVector CollisionPoint;
	float ActorDistance = -1;

	if (InputManager.IsKeyPressed(EKeyInput::Tab))
	{
		Gizmo.IsWorldMode() ? Gizmo.SetLocal() : Gizmo.SetWorld();
	}
	if (InputManager.IsKeyPressed(EKeyInput::Space))
	{
		Gizmo.ChangeGizmoMode();
	}
	if (InputManager.IsKeyReleased(EKeyInput::MouseLeft))
	{
		Gizmo.EndDrag();
		// 드래그가 끝나면 선택된 뷰포트를 비활성화 합니다.
		InteractionViewport = nullptr;
	}

	if (Gizmo.IsDragging() && IsValid<USceneComponent>(Gizmo.GetSelectedComponent()))
	{
		switch (Gizmo.GetGizmoMode())
		{
		case EGizmoMode::Translate:
		{
			FVector GizmoDragLocation = GetGizmoDragLocation(CurrentCamera, WorldRay);
			Gizmo.SetLocation(GizmoDragLocation);
			break;
		}
		case EGizmoMode::Rotate:
		{
			FVector GizmoDragRotation = GetGizmoDragRotation(CurrentCamera, WorldRay);
			Gizmo.SetComponentRotation(GizmoDragRotation);
			break;
		}
		case EGizmoMode::Scale:
		{
			FVector GizmoDragScale = GetGizmoDragScale(CurrentCamera, WorldRay);
			Gizmo.SetComponentScale(GizmoDragScale);
		}
		}
	}
	else
	{
		if (GetSelectedActor() && Gizmo.HasComponent())
		{
			ObjectPicker.PickGizmo(CurrentCamera, WorldRay, Gizmo, CollisionPoint);
		}
		else
		{
			Gizmo.SetGizmoDirection(EGizmoDirection::None);
		}

		if (!ImGui::GetIO().WantCaptureMouse && InputManager.IsKeyPressed(EKeyInput::MouseLeft))
		{
			if (GWorld->GetLevel()->GetShowFlags())
			{
				TArray<UPrimitiveComponent*> Candidate;

				ULevel* CurrentLevel = GWorld->GetLevel();
				ObjectPicker.FindCandidateFromOctree(CurrentLevel->GetStaticOctree(), WorldRay, Candidate);

				TArray<UPrimitiveComponent*>& DynamicCandidates = CurrentLevel->GetDynamicPrimitives();
				if (!DynamicCandidates.empty())
				{
					Candidate.insert(Candidate.end(), DynamicCandidates.begin(), DynamicCandidates.end());
				}
				

				TStatId StatId("Picking");
				FScopeCycleCounter PickCounter(StatId);
				UPrimitiveComponent* PrimitiveCollided = ObjectPicker.PickPrimitive(CurrentCamera, WorldRay, Candidate, &ActorDistance);
				ActorPicked = PrimitiveCollided ? PrimitiveCollided->GetOwner() : nullptr;
				float ElapsedMs = PickCounter.Finish(); // 피킹 시간 측정 종료
				UStatOverlay::GetInstance().RecordPickingStats(ElapsedMs);
			}
		}

		if (Gizmo.GetGizmoDirection() == EGizmoDirection::None)
		{
			SelectActor(ActorPicked);
			if (PreviousGizmoDirection != EGizmoDirection::None)
			{
				Gizmo.OnMouseRelease(PreviousGizmoDirection);
			}
		}
		else
		{
			PreviousGizmoDirection = Gizmo.GetGizmoDirection();
			if (InputManager.IsKeyPressed(EKeyInput::MouseLeft))
			{
				Gizmo.OnMouseDragStart(CollisionPoint);
				// 드래그가 활성화하면 뷰포트를 고정합니다.
				InteractionViewport = CurrentViewport;
			}
			else
			{
				Gizmo.OnMouseHovering();
			}
		}
	}
}

FVector UEditor::GetGizmoDragLocation(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin{ Gizmo.GetGizmoLocation() };
	FVector GizmoAxis = Gizmo.GetGizmoAxis();

	if (!Gizmo.IsWorldMode())
	{
		// RotationMatrix 로직에 문제
		// FVector4 GizmoAxis4{ GizmoAxis.X, GizmoAxis.Y, GizmoAxis.Z, 0.0f };
		// FVector RadRotation = FVector::GetDegreeToRadian(Gizmo.GetComponentRotation());
		// GizmoAxis = GizmoAxis4 * FMatrix::RotationMatrix(RadRotation);
		FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
		GizmoAxis = q.RotateVector(GizmoAxis); 
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, InActiveCamera->CalculatePlaneNormal(GizmoAxis).Cross(GizmoAxis), MouseWorld))
	{
		FVector MouseDistance = MouseWorld - Gizmo.GetDragStartMouseLocation();
		return Gizmo.GetDragStartActorLocation() + GizmoAxis * MouseDistance.Dot(GizmoAxis);
	}
	return Gizmo.GetGizmoLocation();
}

FVector UEditor::GetGizmoDragRotation(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin{ Gizmo.GetGizmoLocation() };
	FVector GizmoAxis = Gizmo.GetGizmoAxis();

	if (!Gizmo.IsWorldMode())
	{
		FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
		GizmoAxis = q.RotateVector(GizmoAxis); 
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, GizmoAxis, MouseWorld))
	{
		FVector PlaneOriginToMouse = MouseWorld - PlaneOrigin;
		FVector PlaneOriginToMouseStart = Gizmo.GetDragStartMouseLocation() - PlaneOrigin;
		PlaneOriginToMouse.Normalize();
		PlaneOriginToMouseStart.Normalize();
		float DotResult = (PlaneOriginToMouseStart).Dot(PlaneOriginToMouse);
		float Angle = acosf(std::max(-1.0f, std::min(1.0f, DotResult)));
		if ((PlaneOriginToMouseStart.Cross(PlaneOriginToMouse)).Dot(GizmoAxis) < 0)
		{
			Angle = -Angle;
		}

		FQuaternion StartRotQuat = FQuaternion::FromEuler(Gizmo.GetDragStartActorRotation());
		FQuaternion DeltaRotQuat = FQuaternion::FromAxisAngle(Gizmo.GetGizmoAxis(), Angle);
		if (Gizmo.IsWorldMode())
		{
			return (StartRotQuat * DeltaRotQuat).ToEuler();
		}
		else
		{
			return (DeltaRotQuat * StartRotQuat).ToEuler();
		}
	}
	return Gizmo.GetComponentRotation();
}

FVector UEditor::GetGizmoDragScale(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin = Gizmo.GetGizmoLocation();
	FVector CardinalAxis = Gizmo.GetGizmoAxis();
	
	FVector GizmoAxis = Gizmo.GetGizmoAxis();
	FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
	GizmoAxis = q.RotateVector(GizmoAxis); 

	FVector PlaneNormal = InActiveCamera->CalculatePlaneNormal(GizmoAxis).Cross(GizmoAxis);
	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
	{
		FVector PlaneOriginToMouse = MouseWorld - PlaneOrigin;
		FVector PlaneOriginToMouseStart = Gizmo.GetDragStartMouseLocation() - PlaneOrigin;
		float DragStartAxisDistance = PlaneOriginToMouseStart.Dot(GizmoAxis);
		float DragAxisDistance = PlaneOriginToMouse.Dot(GizmoAxis);
		float ScaleFactor = 1.0f;
		if (abs(DragStartAxisDistance) > 0.1f)
		{
			ScaleFactor = DragAxisDistance / DragStartAxisDistance;
		}

		FVector DragStartScale = Gizmo.GetDragStartActorScale();
		if (ScaleFactor > MinScale)
		{
			if (Gizmo.GetSelectedComponent()->IsUniformScale())
			{
				float UniformValue = DragStartScale.Dot(CardinalAxis);
				return FVector(1.0f, 1.0f, 1.0f) * UniformValue * ScaleFactor;
			}
			else
			{
				return DragStartScale + CardinalAxis * (ScaleFactor - 1.0f) * DragStartScale.Dot(CardinalAxis);
			}
		}
		return Gizmo.GetComponentScale();
	}
	return Gizmo.GetComponentScale();
}

void UEditor::SelectActor(AActor* InActor)
{
	if (InActor == SelectedActor) return;
	
	SelectedActor = InActor;
	if (SelectedActor) { SelectComponent(InActor->GetRootComponent()); }
	else { SelectComponent(nullptr); }
}

void UEditor::SelectComponent(UActorComponent* InComponent)
{
	if (InComponent == SelectedComponent) return;
	
	if (SelectedComponent)
	{
		SelectedComponent->OnDeselected();
	}

	SelectedComponent = InComponent;
	if (SelectedComponent)
	{
		SelectedComponent->OnSelected();
	}
	UUIManager::GetInstance().OnSelectedComponentChanged(SelectedComponent);
}
