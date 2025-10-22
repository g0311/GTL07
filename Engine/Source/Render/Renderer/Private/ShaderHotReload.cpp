#include "pch.h"
#include "Render/Renderer/Public/ShaderHotReload.h"
#include "Render/Renderer/Public/Renderer.h"

FShaderHotReload::FShaderHotReload()
	: bEnabled(true)
	, TimeSinceLastCheck(0.0f)
	, CheckInterval(0.5f) // 0.5초마다 체크
{
}

FShaderHotReload::~FShaderHotReload()
{
	ShaderFiles.clear();
}

void FShaderHotReload::RegisterShader(const wstring& InFilePath, const FString& InShaderName)
{
	FShaderFileInfo FileInfo(InFilePath);
	ShaderFiles[InShaderName] = FileInfo;

	UE_LOG("ShaderHotReload: Registered shader - %s", InShaderName.c_str());
}

void FShaderHotReload::CheckForChanges(URenderer* InRenderer)
{
	if (!bEnabled || !InRenderer)
	{
		return;
	}

	// 변경된 셰이더 목록 수집
	TArray<FString> ModifiedShaders;

	for (auto& Pair : ShaderFiles)
	{
		const FString& ShaderName = Pair.first;
		FShaderFileInfo& FileInfo = Pair.second;

		if (FileInfo.IsModified())
		{
			ModifiedShaders.push_back(ShaderName);
		}
	}

	// 변경된 셰이더 재컴파일
	if (!ModifiedShaders.empty())
	{
		UE_LOG("ShaderHotReload: Detected modified shaders");
		for (const auto& ShaderName : ModifiedShaders)
		{
			UE_LOG("  - %s", ShaderName.c_str());
			RecompileShader(ShaderName);
		}
	}
}

bool FShaderHotReload::IsShaderModified(const FString& InShaderName) const
{
	auto It = ShaderFiles.find(InShaderName);
	if (It != ShaderFiles.end())
	{
		return It->second.IsModified();
	}
	return false;
}

void FShaderHotReload::RefreshAllTimestamps()
{
	for (auto& Pair : ShaderFiles)
	{
		Pair.second.UpdateModifiedTime();
	}
}

void FShaderHotReload::RecompileShader(const FString& InShaderName)
{
	UE_LOG("ShaderHotReload: Recompiling shader - %s", InShaderName.c_str());
	
	auto& Renderer = URenderer::GetInstance();
	// 셰이더 타입에 따라 재컴파일 수행
	if (InShaderName == "UberShader")
	{
		Renderer.ReloadUberShader();
	}
	else if (InShaderName == "DecalShader")
	{
		Renderer.ReloadDecalShader();
	}
	else if (InShaderName == "PointLightShader")
	{
		Renderer.ReloadPointLightShader();
	}
	else if (InShaderName == "FogShader")
	{
		Renderer.ReloadFogShader();
	}
	else if (InShaderName == "CopyShader")
	{
		Renderer.ReloadCopyShader();
	}
	else if (InShaderName == "FXAAShader")
	{
		Renderer.ReloadFXAAShader();
	}
	else if (InShaderName == "BillboardShader")
	{
		Renderer.ReloadBillboardShader();
	}
	else if (InShaderName == "DefaultShader")
	{
		Renderer.ReloadDefaultShader();
	}
	else if (InShaderName == "LightCullingShader")
	{
		Renderer.ReloadLightCullingShader();
	}
	else if (InShaderName == "LightCommon")
	{
		Renderer.ReloadUberShader();
		Renderer.ReloadDecalShader();
	}
	
	// 타임스탬프 갱신
	auto It = ShaderFiles.find(InShaderName);
	if (It != ShaderFiles.end())
	{
		It->second.UpdateModifiedTime();
	}

	UE_LOG("ShaderHotReload: Successfully recompiled - %s", InShaderName.c_str());
}
