#include "pch.h"
#include "Render/UI/Widget/Public/MainBarWidget.h"
#include "Manager/UI/Public/UIManager.h"
#include "Render/UI/Window/Public/UIWindow.h"
#include "Level/Public/Level.h"
#include <shobjidl.h>

IMPLEMENT_CLASS(UMainBarWidget, UWidget)

/**
 * @brief MainBarWidget 초기화 함수
 * UIManager 인스턴스를 여기서 가져온다
 */
void UMainBarWidget::Initialize()
{
	UIManager = &UUIManager::GetInstance();
	if (!UIManager)
	{
		UE_LOG("MainBarWidget: UIManager를 찾을 수 없습니다!");
		return;
	}

	UE_LOG("MainBarWidget: 메인 메뉴바 위젯이 초기화되었습니다");
}

void UMainBarWidget::Update()
{
	// 업데이트 정보 필요할 경우 추가할 것
}

void UMainBarWidget::RenderWidget()
{
	if (!bIsMenuBarVisible)
	{
		MenuBarHeight = 0.0f;
		return;
	}

	// 메인 메뉴바 시작
	if (ImGui::BeginMainMenuBar())
	{
		// 뷰포트 조정을 위해 메뉴바 높이 저장
		MenuBarHeight = ImGui::GetWindowSize().y;

		// 메뉴 Listing
		RenderFileMenu();
		RenderViewMenu();
		RenderShowFlagsMenu();
		RenderWindowsMenu();
		RenderHelpMenu();
		RenderPlayControls();

		// 메인 메뉴바 종료
		ImGui::EndMainMenuBar();
	}
	else
	{
		MenuBarHeight = 0.0f;
	}
}

/**
 * @brief File 메뉴를 렌더링합니다
 */
void UMainBarWidget::RenderFileMenu()
{
	if (ImGui::BeginMenu("파일"))
	{
		// 레벨 관련 메뉴
		if (ImGui::MenuItem("새 레벨", "Ctrl+N"))
		{
			CreateNewLevel();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("레벨 열기", "Ctrl+O"))
		{
			LoadLevel();
		}

		if (ImGui::MenuItem("레벨 저장", "Ctrl+S"))
		{
			SaveCurrentLevel();
		}

		ImGui::Separator();

		// 일반 파일 작업
		if (ImGui::MenuItem("일반 파일 열기"))
		{
			UE_LOG("MainBarWidget: 일반 파일 열기 메뉴 선택됨");
			// TODO(KHJ): 일반 파일 열기 로직 구현
		}

		ImGui::Separator();

		if (ImGui::MenuItem("종료", "Alt+F4"))
		{
			UE_LOG("MainBarWidget: 프로그램 종료 메뉴 선택됨");
			// TODO(KHJ): 프로그램 종료 로직 구현
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief Windows 토글 메뉴를 렌더링하는 함수
 * 등록된 MainMenu를 제외한 모든 UIWindow의 토글 옵션을 표시
 */
void UMainBarWidget::RenderWindowsMenu() const
{
	if (ImGui::BeginMenu("창"))
	{
		if (!UIManager)
		{
			ImGui::Text("UIManager를 사용할 수 없습니다");
			ImGui::EndMenu();
			return;
		}

		// 모든 등록된 UIWindow에 대해 토글 메뉴 항목 생성
		const auto& AllWindows = UIManager->GetAllUIWindows();

		if (AllWindows.empty())
		{
			ImGui::Text("등록된 창이 없습니다");
		}
		else
		{
			for (auto* Window : AllWindows)
			{
				if (!Window)
				{
					continue;
				}

				if (Window->GetWindowTitle() == "MainMenuBar")
				{
					continue;
				}

				if (ImGui::MenuItem(Window->GetWindowTitle().ToString().data(), nullptr, Window->IsVisible()))
				{
					Window->ToggleVisibility();

					UE_LOG("MainBarWidget: %s 창 토글됨 (현재 상태: %s)",
						Window->GetWindowTitle().ToString().data(),
						Window->IsVisible() ? "표시" : "숨김");
				}
			}

			ImGui::Separator();

			// 전체 창 제어 옵션
			if (ImGui::MenuItem("모든 창 표시"))
			{
				UIManager->ShowAllWindows();
				UE_LOG("MainBarWidget: 모든 창 표시됨");
			}

			if (ImGui::MenuItem("모든 창 숨김"))
			{
				for (auto* Window : UIManager->GetAllUIWindows())
				{
					if (!Window)
					{
						continue;
					}

					if (Window->GetWindowTitle() == "MainMenuBar")
					{
						continue;
					}

					Window->SetWindowState(EUIWindowState::Hidden);
				}

				UE_LOG("MainBarWidget: 모든 창 숨겨짐");
			}
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief View 메뉴를 렌더링하는 함수
 * ViewMode 선택 기능 (Lit, Unlit, Wireframe)
 */
void UMainBarWidget::RenderViewMenu()
{
	if (ImGui::BeginMenu("보기"))
	{
		// GEditor에서 EditorModule 가져오기
		UEditor* EditorInstance = GEditor->GetEditorModule();
		if (!EditorInstance)
		{
			ImGui::Text("에디터를 사용할 수 없습니다");
			ImGui::EndMenu();
			return;
		}

		EViewModeIndex CurrentMode = EditorInstance->GetViewMode();

		// ViewMode 메뉴 아이템
		bool bIsLit = (CurrentMode == EViewModeIndex::VMI_Lit);
		bool bIsUnlit = (CurrentMode == EViewModeIndex::VMI_Unlit);
		bool bIsWireframe = (CurrentMode == EViewModeIndex::VMI_Wireframe);
		bool bIsSceneDepth = (CurrentMode == EViewModeIndex::VMI_SceneDepth);

		if (ImGui::MenuItem("조명 적용(Lit)", nullptr, bIsLit) && !bIsLit)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_Lit);
			UE_LOG("MainBarWidget: ViewMode를 Lit으로 변경");
		}

		if (ImGui::MenuItem("조명 비적용(Unlit)", nullptr, bIsUnlit) && !bIsUnlit)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_Unlit);
			UE_LOG("MainBarWidget: ViewMode를 Unlit으로 변경");
		}

		if (ImGui::MenuItem("와이어프레임(Wireframe)", nullptr, bIsWireframe) && !bIsWireframe)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_Wireframe);
			UE_LOG("MainBarWidget: ViewMode를 Wireframe으로 변경");
		}

		if (ImGui::MenuItem("씬 뎁스(SceneDepth)", nullptr, bIsSceneDepth) && !bIsSceneDepth)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_SceneDepth);
			UE_LOG("MainBarWidget: ViewMode를 SceneDepth으로 변경");
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief ShowFlags 메뉴를 렌더링하는 함수
 * Static Mesh, BillBoard 등의 플래그 상태 확인 및 토글
 */
void UMainBarWidget::RenderShowFlagsMenu()
{
	if (ImGui::BeginMenu("표시 옵션"))
	{
		// 현재 레벨 가져오기
		ULevel* CurrentLevel = GWorld->GetLevel();
		if (!CurrentLevel)
		{
			ImGui::Text("현재 레벨을 찾을 수 없습니다");
			ImGui::EndMenu();
			return;
		}

		// ShowFlags 가져오기
		uint64 ShowFlags = CurrentLevel->GetShowFlags();

		// BillBoard Text 표시 옵션
		bool bShowBillboard = (ShowFlags & EEngineShowFlags::SF_Billboard) != 0;
		if (ImGui::MenuItem("빌보드 표시", nullptr, bShowBillboard))
		{
			if (bShowBillboard)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Billboard);
				UE_LOG("MainBarWidget: 빌보드 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Billboard);
				UE_LOG("MainBarWidget: 빌보드 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Bounds 표시 옵션
		bool bShowBounds = (ShowFlags & EEngineShowFlags::SF_Bounds) != 0;
		if (ImGui::MenuItem("바운딩박스 표시", nullptr, bShowBounds))
		{
			if (bShowBounds)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Bounds);
				UE_LOG("MainBarWidget: 바운딩박스 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Bounds);
				UE_LOG("MainBarWidget: 바운딩박스 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// StaticMesh 표시 옵션
		bool bShowStaticMesh = (ShowFlags & EEngineShowFlags::SF_StaticMesh) != 0;
		if (ImGui::MenuItem("스태틱 메쉬 표시", nullptr, bShowStaticMesh))
		{
			if (bShowStaticMesh)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_StaticMesh);
				UE_LOG("MainBarWidget: 스태틱 메쉬 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_StaticMesh);
				UE_LOG("MainBarWidget: 스태틱 메쉬 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Text 표시 옵션
		bool bShowText = (ShowFlags & EEngineShowFlags::SF_Text) != 0;
		if (ImGui::MenuItem("텍스트 표시", nullptr, bShowText))
		{
			if (bShowText)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Text);
				UE_LOG("MainBarWidget: 텍스트 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Text);
				UE_LOG("MainBarWidget: 텍스트 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Decal 표시 옵션
		bool bShowDecal = (ShowFlags & EEngineShowFlags::SF_Decal) != 0;
		if (ImGui::MenuItem("데칼 표시", nullptr, bShowDecal))
		{
			if (bShowDecal)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Decal);
				UE_LOG("MainBarWidget: 데칼 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Decal);
				UE_LOG("MainBarWidget: 데칼 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Octree 표시 옵션
		bool bShowFog = (ShowFlags & EEngineShowFlags::SF_Fog) != 0;
		if (ImGui::MenuItem("Fog 표시", nullptr, bShowFog))
		{
			if (bShowFog)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Fog);
				UE_LOG("MainBarWidget: Fog 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Fog);
				UE_LOG("MainBarWidget: Fog 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}
		
		// Octree 표시 옵션
		bool bShowOctree = (ShowFlags & EEngineShowFlags::SF_Octree) != 0;
		if (ImGui::MenuItem("Octree 표시", nullptr, bShowOctree))
		{
			if (bShowOctree)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Octree);
				UE_LOG("MainBarWidget: Octree 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Octree);
				UE_LOG("MainBarWidget: Octree 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// FXAA 표시 옵션
		bool bEnableFXAA = (ShowFlags & EEngineShowFlags::SF_FXAA) != 0;
		if (ImGui::MenuItem("FXAA 적용", nullptr, bEnableFXAA))
		{
			if (bEnableFXAA)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_FXAA);
				UE_LOG("MainBarWidget: FXAA 비활성화");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_FXAA);
				UE_LOG("MainBarWidget: FXAA 활성화");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief Help 메뉴에 대한 렌더링 함수
 */
void UMainBarWidget::RenderHelpMenu()
{
	if (ImGui::BeginMenu("도움말"))
	{
		if (ImGui::MenuItem("정보", "F1"))
		{
			UE_LOG("MainBarWidget: 정보 메뉴 선택됨");
			// TODO(KHJ): 정보 다이얼로그 표시
		}

		ImGui::EndMenu();
	}
}

void UMainBarWidget::RenderPlayControls()
{  
	EPIEState CurrentState = GEditor->GetPIEState();

	// 위치 조정
	float CurrentMenuEndPos = ImGui::GetCursorPosX();
	float MainBarWidth = ImGui::GetWindowWidth();

	ImVec2 ButtonPadding(8.0f, 4.0f); 
	float ButtonFramePaddingX = ButtonPadding.x * 2.0f; 

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ButtonPadding);
	float ButtonPlayWidth = ImGui::CalcTextSize("▶").x + ButtonFramePaddingX;
	float ButtonPauseWidth = ImGui::CalcTextSize("||").x + ButtonFramePaddingX;
	float ButtonStopWidth = ImGui::CalcTextSize("■").x + ButtonFramePaddingX;
	ImGui::PopStyleVar(2);

	float TotalSpacingWidth = ImGui::GetStyle().ItemSpacing.x * 2.0f;
	float TotalButtonGroupWidth = ButtonPlayWidth + ButtonPauseWidth + ButtonStopWidth + TotalSpacingWidth;

	float TrueCenterStart = (MainBarWidth / 2.0f) - (TotalButtonGroupWidth / 2.0f);
	float StartPos = TrueCenterStart;
	StartPos = std::max(CurrentMenuEndPos, StartPos);
	ImGui::SameLine(StartPos); 

    // 상태 플래그 정의
    bool bCanStart = CurrentState == EPIEState::Stopped; 
    bool bCanPauseOrResume = CurrentState == EPIEState::Playing || CurrentState == EPIEState::Paused;
    bool bCanStop = CurrentState != EPIEState::Stopped; 

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ButtonPadding);

    // =========================================================================
    if (bCanStart) 
    {
       ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
       ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
       ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.0f, 1.0f));
    }
    else 
    {
       ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.3f, 0.1f, 0.6f));
       ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.3f, 0.1f, 0.6f));
       ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.1f, 0.6f));
    }

    if (ImGui::Button("▶"))
    {
       if (bCanStart) 
       {
          GEditor->StartPIE(); 
          UE_LOG("MainBarWidget: PIE 세션 시작 요청");
       }
    }
    ImGui::PopStyleColor(3);


    ImGui::SameLine();

    // =========================================================================
    if (bCanPauseOrResume)
    {
        if (CurrentState == EPIEState::Paused)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.0f, 1.0f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.8f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.0f, 1.0f));
        }
    }
    else
    {
       ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.0f, 0.6f));
       ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.0f, 0.6f));
       ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.0f, 0.6f));
    }
   
    if (ImGui::Button("||"))
    {
       if (CurrentState == EPIEState::Playing)
       {
          GEditor->PausePIE();
          UE_LOG("MainBarWidget: PIE 세션 일시 정지 요청");
       }
       else if (CurrentState == EPIEState::Paused)
       {
          GEditor->ResumePIE();
          UE_LOG("MainBarWidget: PIE 세션 재개 요청");
       }
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    // =========================================================================
    if (bCanStop)
    {
       ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
       ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
       ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.0f, 0.0f, 1.0f));
    }
    else
    {
       ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.0f, 0.0f, 0.6f));
       ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.0f, 0.0f, 0.6f));
       ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.0f, 0.0f, 0.6f));
    }
   
    if (ImGui::Button("■"))
    {
       if (bCanStop)
       {
          GEditor->EndPIE();
          UE_LOG("MainBarWidget: PIE 세션 정지 요청");
       }
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

/**
 * @brief 레벨 저장 기능
 */
void UMainBarWidget::SaveCurrentLevel()
{
	path FilePath = OpenSaveFileDialog();
	if (!FilePath.empty())
	{
		try
		{
			bool bSuccess = GEditor->SaveCurrentLevel(FilePath.string());

			if (bSuccess)
			{
				UE_LOG("MainMenuBar: SceneIO: Level Saved Successfully");
			}
			else
			{
				UE_LOG("MainMenuBar: SceneIO: Failed To Save Level");
			}
		}
		catch (const exception& Exception)
		{
			FString StatusMessage = FString("Save Error: ") + Exception.what();
			UE_LOG("MainMenuBar: SceneIO: Save Error: %s", Exception.what());
		}
	}
}

/**
 * @brief 레벨 로드 기능
 */
void UMainBarWidget::LoadLevel()
{
	path FilePath = OpenLoadFileDialog();

	if (!FilePath.empty())
	{
		try
		{
			bool bSuccess = GEditor->LoadLevel(FilePath.string());

			if (bSuccess)
			{
				UE_LOG("MainMenuBar: SceneIO: 레벨 로드에 성공했습니다");
			}
			else
			{
				UE_LOG("MainMenuBar: SceneIO: 레벨 로드에 실패했습니다");
			}
		}
		catch (const exception& Exception)
		{
			FString StatusMessage = FString("Load Error: ") + Exception.what();
			UE_LOG("SceneIO: Load Error: %s", Exception.what());
		}
	}
}

/**
 * @brief 새로운 레벨 생성 기능
 */
void UMainBarWidget::CreateNewLevel()
{
	if (GEditor->CreateNewLevel())
	{
		UE_LOG("MainBarWidget: 새로운 레벨이 성공적으로 생성되었습니다");
	}
	else
	{
		UE_LOG("MainBarWidget: 새로운 레벨 생성에 실패했습니다");
	}
}

/**
 * @brief Windows API를 활용한 파일 저장 Dialog Modal을 생성하는 UI Window 기능
 * PWSTR: WideStringPointer 클래스
 * @return 선택된 파일 경로 (취소 시 빈 문자열)
 */
path UMainBarWidget::OpenSaveFileDialog()
{
	path ResultPath = L"";

	// COM 라이브러리 초기화
	HRESULT ResultHandle = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(ResultHandle))
	{
		IFileSaveDialog* FileSaveDialogPtr = nullptr;

		// 2. FileSaveDialog 인스턴스 생성
		ResultHandle = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
			IID_IFileSaveDialog, reinterpret_cast<void**>(&FileSaveDialogPtr));

		if (SUCCEEDED(ResultHandle))
		{
			// 3. 대화상자 옵션 설정
			// 파일 타입 필터 설정
			COMDLG_FILTERSPEC SpecificationRange[] = {
				{L"Scene Files (*.scene)", L"*.scene"},
				{L"All Files (*.*)", L"*.*"}
			};
			FileSaveDialogPtr->SetFileTypes(ARRAYSIZE(SpecificationRange), SpecificationRange);

			// 기본 필터를 "Scene Files" 로 설정
			FileSaveDialogPtr->SetFileTypeIndex(1);

			// 기본 확장자 설정
			FileSaveDialogPtr->SetDefaultExtension(L"json");

			// 대화상자 제목 설정
			FileSaveDialogPtr->SetTitle(L"Save Level File");

			// Set Flag
			DWORD DoubleWordFlags;
			FileSaveDialogPtr->GetOptions(&DoubleWordFlags);
			FileSaveDialogPtr->SetOptions(DoubleWordFlags | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST);

			// Show Modal
			// 현재 활성 창을 부모로 가짐
			UE_LOG("SceneIO: Save Dialog Modal Opening...");
			ResultHandle = FileSaveDialogPtr->Show(GetActiveWindow());

			// 결과 처리
			// 사용자가 '저장' 을 눌렀을 경우
			if (SUCCEEDED(ResultHandle))
			{
				UE_LOG("SceneIO: Save Dialog Modal Closed - 파일 선택됨");
				IShellItem* ShellItemResult;
				ResultHandle = FileSaveDialogPtr->GetResult(&ShellItemResult);
				if (SUCCEEDED(ResultHandle))
				{
					// Get File Path from IShellItem
					PWSTR FilePath = nullptr;
					ResultHandle = ShellItemResult->GetDisplayName(SIGDN_FILESYSPATH, &FilePath);

					if (SUCCEEDED(ResultHandle))
					{
						ResultPath = path(FilePath);
						CoTaskMemFree(FilePath);
					}

					ShellItemResult->Release();
				}
			}
			// 사용자가 '취소'를 눌렀거나 오류 발생
			else
			{
				UE_LOG("SceneIO: Save Dialog Modal Closed - 취소됨");
			}

			// Release FileSaveDialog
			FileSaveDialogPtr->Release();
		}

		// COM 라이브러리 해제
		CoUninitialize();
	}

	return ResultPath;
}

/**
 * @brief Windows API를 활용한 파일 로드 Dialog Modal을 생성하는 UI Window 기능
 * @return 선택된 파일 경로 (취소 시 빈 문자열)
 */
path UMainBarWidget::OpenLoadFileDialog()
{
	path ResultPath = L"";

	// COM 라이브러리 초기화
	HRESULT ResultHandle = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(ResultHandle))
	{
		IFileOpenDialog* FileOpenDialog = nullptr;

		// FileOpenDialog 인스턴스 생성
		ResultHandle = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
			IID_IFileOpenDialog, reinterpret_cast<void**>(&FileOpenDialog));

		if (SUCCEEDED(ResultHandle))
		{
			// 파일 타입 필터 설정
			COMDLG_FILTERSPEC SpecificationRange[] = {
				{L"Scene Files (*.scene)", L"*.scene"},
				{L"All Files (*.*)", L"*.*"}
			};

			FileOpenDialog->SetFileTypes(ARRAYSIZE(SpecificationRange), SpecificationRange);

			// 기본 필터를 "Scene Files" 로 설정
			FileOpenDialog->SetFileTypeIndex(1);

			// 대화상자 제목 설정
			FileOpenDialog->SetTitle(L"Load Level File");

			// Flag Setting
			DWORD DoubleWordFlags;
			FileOpenDialog->GetOptions(&DoubleWordFlags);
			FileOpenDialog->SetOptions(DoubleWordFlags | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

			// Open Modal
			UE_LOG("SceneIO: Load Dialog Modal Opening...");
			ResultHandle = FileOpenDialog->Show(GetActiveWindow()); // 현재 활성 창을 부모로

			// 결과 처리
			// 사용자가 '열기' 를 눌렀을 경우
			if (SUCCEEDED(ResultHandle))
			{
				UE_LOG("SceneIO: Load Dialog Modal Closed - 파일 선택됨");
				IShellItem* ShellItemResult;
				ResultHandle = FileOpenDialog->GetResult(&ShellItemResult);

				if (SUCCEEDED(ResultHandle))
				{
					// Get File Path from IShellItem
					PWSTR ReturnFilePath = nullptr;
					ResultHandle = ShellItemResult->GetDisplayName(SIGDN_FILESYSPATH, &ReturnFilePath);

					if (SUCCEEDED(ResultHandle))
					{
						ResultPath = path(ReturnFilePath);
						CoTaskMemFree(ReturnFilePath);
					}

					ShellItemResult->Release();
				}
			}
			// 사용자가 '취소' 를 눌렀거나 오류 발생
			else
			{
				UE_LOG("SceneIO: Load Dialog Modal Closed - 취소됨");
			}

			FileOpenDialog->Release();
		}

		// COM 라이브러리 해제
		CoUninitialize();
	}

	return ResultPath;
}

