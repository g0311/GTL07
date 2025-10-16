#include "pch.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Core/Public/Class.h"
#include "Editor/Public/Camera.h"
#include "Utility/Public/JsonSerializer.h"

#include <json.hpp>

IMPLEMENT_SINGLETON_CLASS(UConfigManager, UObject)

UConfigManager::UConfigManager()
	: EditorIniFileName("editor.ini")
{
	LoadEditorSetting();
}

UConfigManager::~UConfigManager()
{
	SaveEditorSetting();
}

void UConfigManager::LoadEditorSetting()
{
	const FString& FileNameStr = EditorIniFileName.ToString();
	std::ifstream Ifs(FileNameStr);
	if (!Ifs.is_open())
	{
		CellSize = 1.0f;
		CameraSensitivity = UCamera::DEFAULT_SPEED;
		return; // 파일이 없으면 기본값 유지
	}

	FString Line;
	while (std::getline(Ifs, Line))
	{
		size_t DelimiterPos = Line.find('=');
		if (DelimiterPos == FString::npos) continue;

		FString Key = Line.substr(0, DelimiterPos);
		FString Value = Line.substr(DelimiterPos + 1);

		try
		{
			if (Key == "CellSize") CellSize = std::stof(Value);
			else if (Key == "CameraSensitivity") CameraSensitivity = std::stof(Value);
			else if (Key == "RootSplitterRatio") RootSplitterRatio = std::stof(Value);
			else if (Key == "LeftSplitterRatio") LeftSplitterRatio = std::stof(Value);
			else if (Key == "RightSplitterRatio") RightSplitterRatio = std::stof(Value);
			else if (Key == "LastUsedLevelPath") LastUsedLevelPath = Value;
		}
		catch (const std::exception&) {}
	}
}

void UConfigManager::SaveEditorSetting()
{
	std::ofstream Ofs(EditorIniFileName.ToString());
	if (Ofs.is_open())
	{
		Ofs << "CellSize=" << CellSize << "\n";
		Ofs << "CameraSensitivity=" << CameraSensitivity << "\n";
		Ofs << "RootSplitterRatio=" << RootSplitterRatio << "\n";
		Ofs << "LeftSplitterRatio=" << LeftSplitterRatio << "\n";
		Ofs << "RightSplitterRatio=" << RightSplitterRatio << "\n";
		Ofs << "LastUsedLevelPath=" << LastUsedLevelPath << "\n";
	}
}

JSON UConfigManager::GetCameraSettingsAsJson()
{
	JSON cameraArray = json::Array(); // 반환할 JSON 배열 생성

	// 4개의 뷰포트 카메라 설정을 모두 순회
	for (int i = 0; i < 4; ++i)
	{
		JSON cameraObject = json::Object();
		const auto& data = ViewportCameraSettings[i];

		// 각 카메라의 데이터를 JSON 객체에 저장
		cameraObject["Location"] = FJsonSerializer::VectorToJson(data.Location);
		cameraObject["Rotation"] = FJsonSerializer::VectorToJson(data.Rotation);
		cameraObject["FarClip"] = FJsonSerializer::FloatToArrayJson(data.FarClip);
		cameraObject["NearClip"] = FJsonSerializer::FloatToArrayJson(data.NearClip);
		cameraObject["FOV"] = FJsonSerializer::FloatToArrayJson(data.FovY);
		cameraObject["ViewportCameraType"] = static_cast<int>(data.ViewportCameraType);

		// 완성된 객체를 배열에 추가
		cameraArray.append(cameraObject);
	}
	return cameraArray;
}

void UConfigManager::SetCameraSettingsFromJson(const JSON& InData)
{
	if (InData.JSONType() != JSON::Class::Array || InData.size() != 4)
	{
		return;
	}

	int i = 0;
	for (const JSON& cameraObject : InData.ArrayRange())
	{
		if (i >= 4) break; // 안전장치
		if (cameraObject.JSONType() != JSON::Class::Object) continue;

		FJsonSerializer::ReadVector(cameraObject, "Location", ViewportCameraSettings[i].Location);
		FJsonSerializer::ReadVector(cameraObject, "Rotation", ViewportCameraSettings[i].Rotation);
		FJsonSerializer::ReadArrayFloat(cameraObject, "FOV", ViewportCameraSettings[i].FovY);
		FJsonSerializer::ReadArrayFloat(cameraObject, "FarClip", ViewportCameraSettings[i].FarClip);
		FJsonSerializer::ReadArrayFloat(cameraObject, "NearClip", ViewportCameraSettings[i].NearClip);
		
		// ReadInt32 대신 더 안전한 방식으로 enum 값을 읽어옵니다.
		if (cameraObject.hasKey("ViewportCameraType") && cameraObject.at("ViewportCameraType").JSONType() == JSON::Class::Integral)
		{
			ViewportCameraSettings[i].ViewportCameraType = static_cast<EViewportCameraType>(cameraObject.at("ViewportCameraType").ToInt());
		}
		else
		{
			ViewportCameraSettings[i].ViewportCameraType = EViewportCameraType::Perspective;
		}

		i++;
	}
}
