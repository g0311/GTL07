#pragma once

#include <filesystem>

class URenderer;

/**
 * FShaderFileInfo
 * 셰이더 파일의 수정 시간을 추적하는 구조체
 */
struct FShaderFileInfo
{
	wstring FilePath;
	std::filesystem::file_time_type LastModifiedTime;

	FShaderFileInfo()
		: FilePath(L"")
		, LastModifiedTime{}
	{
	}

	FShaderFileInfo(const wstring& InFilePath)
		: FilePath(InFilePath)
		, LastModifiedTime{}
	{
		UpdateModifiedTime();
	}

	bool UpdateModifiedTime()
	{
		if (std::filesystem::exists(FilePath))
		{
			LastModifiedTime = std::filesystem::last_write_time(FilePath);
			return true;
		}
		return false;
	}

	bool IsModified() const
	{
		if (std::filesystem::exists(FilePath))
		{
			auto CurrentTime = std::filesystem::last_write_time(FilePath);
			return CurrentTime > LastModifiedTime;
		}
		return false;
	}
};

/**
 * FShaderHotReload
 * 런타임 중 셰이더 파일 변경을 감지하고 자동으로 재컴파일하는 시스템
 */
class FShaderHotReload
{
public:
	FShaderHotReload();
	~FShaderHotReload();

	/**
	 * 추적할 셰이더 파일을 등록
	 * @param InFilePath 셰이더 파일 경로
	 * @param InShaderName 셰이더 식별자 (예: "UberShader", "DecalShader")
	 */
	void RegisterShader(const wstring& InFilePath, const FString& InShaderName);

	/**
	 * 모든 셰이더 파일의 변경 여부를 확인하고, 변경된 경우 재컴파일
	 * @param InRenderer 셰이더를 재컴파일할 렌더러
	 */
	void CheckForChanges(URenderer* InRenderer);

	/**
	 * 특정 셰이더 파일의 변경 여부 확인
	 * @param InShaderName 셰이더 식별자
	 * @return 변경되었으면 true
	 */
	bool IsShaderModified(const FString& InShaderName) const;

	/**
	 * 모든 추적 중인 셰이더 파일의 수정 시간 갱신
	 */
	void RefreshAllTimestamps();

	/**
	 * 핫 리로드 활성화/비활성화
	 */
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
	bool IsEnabled() const { return bEnabled; }

private:
	/**
	 * 셰이더 재컴파일 트리거
	 * @param InShaderName 재컴파일할 셰이더 식별자
	 */
	void RecompileShader(const FString& InShaderName);

	/** 셰이더 파일 정보 맵 (ShaderName -> FileInfo) */
	TMap<FString, FShaderFileInfo> ShaderFiles;

	/** 핫 리로드 활성화 여부 */
	bool bEnabled;

	/** 마지막 체크 이후 경과 시간 (프레임마다 체크하지 않고 일정 간격으로 체크) */
	float TimeSinceLastCheck;

	/** 체크 간격 (초 단위) */
	float CheckInterval;
};
