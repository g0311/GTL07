#include "pch.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Component/Mesh/Public/VertexDatas.h"
#include "Physics/Public/AABB.h"
#include "Texture/Public/Texture.h"
#include "Manager/Asset/Public/ObjManager.h"
#include "Manager/Path/Public/PathManager.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

IMPLEMENT_SINGLETON_CLASS(UAssetManager, UObject)
UAssetManager::UAssetManager()
{
	TextureManager = new FTextureManager();
}

UAssetManager::~UAssetManager() = default;

void UAssetManager::Initialize()
{
	// 1. Data 폴더 속 모든 Texture 파일 로드 및 캐싱
	TextureManager->LoadAllTexturesFromDirectory(UPathManager::GetInstance().GetDataPath());

	// 2. Data 폴더 속 모든 .mtl 파일 로드 및 캐싱 (Texture와 연결)
	LoadAllMaterialLibraries();

	// 3. Data 폴더 속 모든 .obj 파일 로드 및 캐싱 (MTL과 연결)
	LoadAllObjStaticMesh();

	VertexDatas.emplace(EPrimitiveType::Torus, &VerticesTorus);
	VertexDatas.emplace(EPrimitiveType::Arrow, &VerticesArrow);
	VertexDatas.emplace(EPrimitiveType::CubeArrow, &VerticesCubeArrow);
	VertexDatas.emplace(EPrimitiveType::Ring, &VerticesRing);
	VertexDatas.emplace(EPrimitiveType::Line, &VerticesLine);
	VertexDatas.emplace(EPrimitiveType::Sprite, &VerticesVerticalSquare);

	IndexDatas.emplace(EPrimitiveType::Sprite, &IndicesVerticalSquare);
	IndexBuffers.emplace(EPrimitiveType::Sprite,
		FRenderResourceFactory::CreateIndexBuffer(IndicesVerticalSquare.data(), static_cast<int>(IndicesVerticalSquare.size()) * sizeof(uint32)));

	NumIndices.emplace(EPrimitiveType::Sprite, static_cast<uint32>(IndicesVerticalSquare.size()));
	
	// TArray.GetData(), TArray.Num()*sizeof(FVertexSimple), TArray.GetTypeSize()
	VertexBuffers.emplace(EPrimitiveType::Torus, FRenderResourceFactory::CreateVertexBuffer(
		VerticesTorus.data(), static_cast<int>(VerticesTorus.size() * sizeof(FNormalVertex))));
	VertexBuffers.emplace(EPrimitiveType::Arrow, FRenderResourceFactory::CreateVertexBuffer(
		VerticesArrow.data(), static_cast<int>(VerticesArrow.size() * sizeof(FNormalVertex))));
	VertexBuffers.emplace(EPrimitiveType::CubeArrow, FRenderResourceFactory::CreateVertexBuffer(
		VerticesCubeArrow.data(), static_cast<int>(VerticesCubeArrow.size() * sizeof(FNormalVertex))));
	VertexBuffers.emplace(EPrimitiveType::Ring, FRenderResourceFactory::CreateVertexBuffer(
		VerticesRing.data(), static_cast<int>(VerticesRing.size() * sizeof(FNormalVertex))));
	VertexBuffers.emplace(EPrimitiveType::Line, FRenderResourceFactory::CreateVertexBuffer(
		VerticesLine.data(), static_cast<int>(VerticesLine.size() * sizeof(FNormalVertex))));
	VertexBuffers.emplace(EPrimitiveType::Sprite, FRenderResourceFactory::CreateVertexBuffer(
		VerticesVerticalSquare.data(), static_cast<int>(VerticesVerticalSquare.size() * sizeof(FNormalVertex))));

	NumVertices.emplace(EPrimitiveType::Torus, static_cast<uint32>(VerticesTorus.size()));
	NumVertices.emplace(EPrimitiveType::Arrow, static_cast<uint32>(VerticesArrow.size()));
	NumVertices.emplace(EPrimitiveType::CubeArrow, static_cast<uint32>(VerticesCubeArrow.size()));
	NumVertices.emplace(EPrimitiveType::Ring, static_cast<uint32>(VerticesRing.size()));
	NumVertices.emplace(EPrimitiveType::Line, static_cast<uint32>(VerticesLine.size()));
	NumVertices.emplace(EPrimitiveType::Sprite, static_cast<uint32>(VerticesVerticalSquare.size()));
	
	// Calculate AABB for all primitive types (excluding StaticMesh)
	for (const auto& Pair : VertexDatas)
	{
		EPrimitiveType Type = Pair.first;
		const auto* Vertices = Pair.second;
		if (!Vertices || Vertices->empty())
			continue;

		AABBs[Type] = CalculateAABB(*Vertices);
	}

	// Calculate AABB for each StaticMesh
	for (const auto& MeshPair : StaticMeshCache)
	{
		const FName& ObjPath = MeshPair.first;
		const auto& Mesh = MeshPair.second;
		if (!Mesh || !Mesh->IsValid())
			continue;

		const auto& Vertices = Mesh->GetVertices();
		if (Vertices.empty())
			continue;

		StaticMeshAABBs[ObjPath] = CalculateAABB(Vertices);
	}
}

void UAssetManager::Release()
{
	// TMap.Value()
	for (auto& Pair : VertexBuffers)
	{
		SafeRelease(Pair.second);
	}
	for (auto& Pair : IndexBuffers)
	{
		SafeRelease(Pair.second);
	}

	for (auto& Pair : StaticMeshVertexBuffers)
	{
		SafeRelease(Pair.second);
	}
	for (auto& Pair : StaticMeshIndexBuffers)
	{
		SafeRelease(Pair.second);
	}

	StaticMeshCache.clear();	// unique ptr 이라서 자동으로 해제됨
	StaticMeshVertexBuffers.clear();
	StaticMeshIndexBuffers.clear();

	// TMap.Empty()
	VertexBuffers.clear();
	IndexBuffers.clear();
	
	SafeDelete(TextureManager);
}

/**
 * @brief Data/ 경로 하위에 모든 .obj 파일을 로드 후 캐싱한다
 */
void UAssetManager::LoadAllObjStaticMesh()
{
	TArray<FName> ObjList;
	const FString DataDirectory = "Data/"; // 검색할 기본 디렉토리
	// 디렉토리가 실제로 존재하는지 먼저 확인합니다.
	if (std::filesystem::exists(DataDirectory) && std::filesystem::is_directory(DataDirectory))
	{
		// recursive_directory_iterator를 사용하여 디렉토리와 모든 하위 디렉토리를 순회합니다.
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataDirectory))
		{
			// 현재 항목이 일반 파일이고, 확장자가 ".obj"인지 확인합니다.
			if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
			{
				// .generic_string()을 사용하여 OS에 상관없이 '/' 구분자를 사용하는 경로를 바로 얻습니다.
				FString PathString = Entry.path().generic_string();

				// 찾은 파일 경로를 FName으로 변환하여 ObjList에 추가합니다.
				ObjList.push_back(FName(PathString));
			}
		}
	}

	// Enable winding order flip for this OBJ file
	FObjImporter::Configuration Config;
	Config.bFlipWindingOrder = false;
	Config.bIsBinaryEnabled = true;
	Config.bPositionToUEBasis = true;
	Config.bNormalToUEBasis = true;
	Config.bUVToUEBasis = true;

	// 범위 기반 for문을 사용하여 배열의 모든 요소를 순회합니다.
	for (const FName& ObjPath : ObjList)
	{
		// FObjManager가 UStaticMesh 포인터를 반환한다고 가정합니다.
		UStaticMesh* LoadedMesh = FObjManager::LoadObjStaticMesh(ObjPath, Config);

		// 로드에 성공했는지 확인합니다.
		if (LoadedMesh)
		{
			StaticMeshCache.emplace(ObjPath, LoadedMesh);

			StaticMeshVertexBuffers.emplace(ObjPath, this->CreateVertexBuffer(LoadedMesh->GetVertices()));
			StaticMeshIndexBuffers.emplace(ObjPath, this->CreateIndexBuffer(LoadedMesh->GetIndices()));
		}
	}
}

ID3D11Buffer* UAssetManager::GetVertexBuffer(FName InObjPath)
{
	if (StaticMeshVertexBuffers.count(InObjPath))
	{
		return StaticMeshVertexBuffers[InObjPath];
	}
	return nullptr;
}

ID3D11Buffer* UAssetManager::GetIndexBuffer(FName InObjPath)
{
	if (StaticMeshIndexBuffers.count(InObjPath))
	{
		return StaticMeshIndexBuffers[InObjPath];
	}
	return nullptr;
}

ID3D11Buffer* UAssetManager::CreateVertexBuffer(TArray<FNormalVertex> InVertices)
{
	return FRenderResourceFactory::CreateVertexBuffer(InVertices.data(), static_cast<int>(InVertices.size()) * sizeof(FNormalVertex));
}

ID3D11Buffer* UAssetManager::CreateIndexBuffer(TArray<uint32> InIndices)
{
	return FRenderResourceFactory::CreateIndexBuffer(InIndices.data(), static_cast<int>(InIndices.size()) * sizeof(uint32));
}

TArray<FNormalVertex>* UAssetManager::GetVertexData(EPrimitiveType InType)
{
	return VertexDatas[InType];
}

ID3D11Buffer* UAssetManager::GetVertexbuffer(EPrimitiveType InType)
{
	return VertexBuffers[InType];
}

uint32 UAssetManager::GetNumVertices(EPrimitiveType InType)
{
	return NumVertices[InType];
}

TArray<uint32>* UAssetManager::GetIndexData(EPrimitiveType InType)
{
	return IndexDatas[InType];
}

ID3D11Buffer* UAssetManager::GetIndexBuffer(EPrimitiveType InType)
{
	return IndexBuffers[InType];
}

uint32 UAssetManager::GetNumIndices(EPrimitiveType InType)
{
	return NumIndices[InType];
}

FAABB& UAssetManager::GetAABB(EPrimitiveType InType)
{
	return AABBs[InType];
}

FAABB& UAssetManager::GetStaticMeshAABB(FName InName)
{
	return StaticMeshAABBs[InName];
}

// StaticMesh Cache Accessors
UStaticMesh* UAssetManager::GetStaticMeshFromCache(const FName& InObjPath)
{
	auto It = StaticMeshCache.find(InObjPath);
	if (It != StaticMeshCache.end())
	{
		return It->second.get();
	}
	return nullptr;
}

void UAssetManager::AddStaticMeshToCache(const FName& InObjPath, UStaticMesh* InStaticMesh)
{
	if (!InStaticMesh)
		return;

	if (StaticMeshCache.find(InObjPath) == StaticMeshCache.end())
	{
		StaticMeshCache.emplace(InObjPath, std::unique_ptr<UStaticMesh>(InStaticMesh));
	}
}

/**
 * @brief Vertex 배열로부터 AABB(Axis-Aligned Bounding Box)를 계산하는 헬퍼 함수
 * @param Vertices 정점 데이터 배열
 * @return 계산된 FAABB 객체
 */
FAABB UAssetManager::CalculateAABB(const TArray<FNormalVertex>& Vertices)
{
	FVector MinPoint(+FLT_MAX, +FLT_MAX, +FLT_MAX);
	FVector MaxPoint(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const auto& Vertex : Vertices)
	{
		MinPoint.X = std::min(MinPoint.X, Vertex.Position.X);
		MinPoint.Y = std::min(MinPoint.Y, Vertex.Position.Y);
		MinPoint.Z = std::min(MinPoint.Z, Vertex.Position.Z);

		MaxPoint.X = std::max(MaxPoint.X, Vertex.Position.X);
		MaxPoint.Y = std::max(MaxPoint.Y, Vertex.Position.Y);
		MaxPoint.Z = std::max(MaxPoint.Z, Vertex.Position.Z);
	}

	return FAABB(MinPoint, MaxPoint);
}

/**
 * @brief Data/ 경로 하위에 모든 .mtl 파일을 로드 후 캐싱한다
 */
void UAssetManager::LoadAllMaterialLibraries()
{
	TArray<FName> MtlList;
	const std::filesystem::path DataDirectory = UPathManager::GetInstance().GetDataPath();

	UE_LOG("LoadAllMaterialLibraries 시작: Data 경로 검색 중... (%s)", DataDirectory.string().c_str());

	// 디렉토리가 실제로 존재하는지 먼저 확인합니다.
	if (std::filesystem::exists(DataDirectory) && std::filesystem::is_directory(DataDirectory))
	{
		UE_LOG("Data 디렉토리 발견: %s", DataDirectory.string().c_str());

		// recursive_directory_iterator를 사용하여 디렉토리와 모든 하위 디렉토리를 순회합니다.
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataDirectory))
		{
			// 현재 항목이 일반 파일이고, 확장자가 ".mtl"인지 확인합니다.
			if (Entry.is_regular_file() && Entry.path().extension() == ".mtl")
			{
				// .generic_string()을 사용하여 OS에 상관없이 '/' 구분자를 사용하는 경로를 바로 얻습니다.
				FString PathString = Entry.path().generic_string();

				// 찾은 파일 경로를 FName으로 변환하여 MtlList에 추가합니다.
				MtlList.push_back(FName(PathString));
				UE_LOG("MTL 파일 발견: %s", PathString.c_str());
			}
		}
	}
	else
	{
		UE_LOG_ERROR("Data 디렉토리를 찾을 수 없습니다: %s", DataDirectory.string().c_str());
	}

	// MTL 파일 경로의 부모 디렉토리를 추적하기 위한 맵
	std::filesystem::path MtlDirectory = DataDirectory;

	// 범위 기반 for문을 사용하여 배열의 모든 요소를 순회합니다.
	for (const FName& MtlPath : MtlList)
	{
		TArray<FObjectMaterialInfo> MaterialList;

		// FObjImporter를 통해 .mtl 파일을 로드합니다.
		if (FObjImporter::LoadMaterialLibrary(MtlPath.ToString(), &MaterialList))
		{
			size_t MaterialCount = MaterialList.size();
			MaterialLibraryCache.emplace(MtlPath, MaterialList); // Move를 제거하여 MaterialList를 유지

			// MTL 파일의 디렉토리 경로 추출
			std::filesystem::path MtlFilePath(MtlPath.ToString());
			std::filesystem::path MtlFileDir = MtlFilePath.parent_path();

			// 각 머티리얼 정보를 UMaterial 객체로 변환하여 등록
			for (const FObjectMaterialInfo& MatInfo : MaterialList)
			{
				auto* Material = NewObject<UMaterial>();
				Material->SetName(MatInfo.Name);

				// Diffuse 텍스처 로드 (map_Kd)
				if (!MatInfo.KdMap.empty())
				{
					FString TexturePathStr = (MtlFileDir / MatInfo.KdMap).generic_string();
					if (std::filesystem::exists(TexturePathStr))
					{
						UTexture* DiffuseTexture = LoadTexture(TexturePathStr);
						if (DiffuseTexture)
						{
							Material->SetDiffuseTexture(DiffuseTexture);
						}
					}
				}

				// Ambient 텍스처 로드 (map_Ka)
				if (!MatInfo.KaMap.empty())
				{
					FString TexturePathStr = (MtlFileDir / MatInfo.KaMap).generic_string();
					if (std::filesystem::exists(TexturePathStr))
					{
						UTexture* AmbientTexture = LoadTexture(TexturePathStr);
						if (AmbientTexture)
						{
							Material->SetAmbientTexture(AmbientTexture);
						}
					}
				}

				// Specular 텍스처 로드 (map_Ks)
				if (!MatInfo.KsMap.empty())
				{
					FString TexturePathStr = (MtlFileDir / MatInfo.KsMap).generic_string();
					if (std::filesystem::exists(TexturePathStr))
					{
						UTexture* SpecularTexture = LoadTexture(TexturePathStr);
						if (SpecularTexture)
						{
							Material->SetSpecularTexture(SpecularTexture);
						}
					}
				}

				// Normal 텍스처 로드 (map_bump)
				if (!MatInfo.NormalMap.empty())
				{
					FString TexturePathStr = (MtlFileDir / MatInfo.NormalMap).generic_string();
					if (std::filesystem::exists(TexturePathStr))
					{
						UTexture* NormalTexture = LoadTexture(TexturePathStr);
						if (NormalTexture)
						{
							Material->SetNormalTexture(NormalTexture);
						}
					}
				}

				// Height/Displacement 텍스처 로드 (disp)
				if (!MatInfo.HeightMap.empty())
				{
					FString TexturePathStr = (MtlFileDir / MatInfo.HeightMap).generic_string();
					if (std::filesystem::exists(TexturePathStr))
					{
						UTexture* HeightTexture = LoadTexture(TexturePathStr);
						if (HeightTexture)
						{
							Material->SetHeightTexture(HeightTexture);
						}
					}
				}

				// Alpha 텍스처 로드 (map_d)
				if (!MatInfo.DMap.empty())
				{
					FString TexturePathStr = (MtlFileDir / MatInfo.DMap).generic_string();
					if (std::filesystem::exists(TexturePathStr))
					{
						UTexture* AlphaTexture = LoadTexture(TexturePathStr);
						if (AlphaTexture)
						{
							Material->SetAlphaTexture(AlphaTexture);
						}
					}
				}

				UE_LOG("UMaterial 생성 완료: %s", MatInfo.Name.c_str());
			}

			UE_LOG("MaterialLibrary 로드 성공: %s (재질 개수: %zu)", MtlPath.ToString().c_str(), MaterialCount);
		}
		else
		{
			UE_LOG_ERROR("MaterialLibrary 로드 실패: %s", MtlPath.ToString().c_str());
		}
	}

	UE_LOG("총 %zu개의 MTL 파일을 로드했습니다.", MaterialLibraryCache.size());
}

/**
 * @brief 캐싱된 Material Library를 반환하는 함수
 * @param InMtlPath .mtl 파일 경로
 * @return 캐싱된 Material Library 배열의 포인터, 없으면 nullptr
 */
const TArray<FObjectMaterialInfo>* UAssetManager::GetMaterialLibrary(const FName& InMtlPath) const
{
	auto It = MaterialLibraryCache.find(InMtlPath);
	if (It != MaterialLibraryCache.end())
	{
		return &It->second;
	}
	return nullptr;
}

/**
 * @brief Material Library를 캐시에 추가하는 함수
 * @param InMtlPath .mtl 파일 경로
 * @param InMaterialList 추가할 Material 배열
 */
void UAssetManager::AddMaterialLibrary(const FName& InMtlPath, const TArray<FObjectMaterialInfo>& InMaterialList)
{
	if (MaterialLibraryCache.find(InMtlPath) == MaterialLibraryCache.end())
	{
		MaterialLibraryCache.emplace(InMtlPath, InMaterialList);
	}
}

/**
 * @brief 넘겨준 경로로 캐싱된 UTexture 포인터를 반환해주는 함수
 * @param 로드할 텍스처 경로
 * @return 캐싱된 UTexture 포인터
 */
UTexture* UAssetManager::LoadTexture(const FName& InFilePath)
{
	return TextureManager->LoadTexture(InFilePath);
}

/**
 * @brief 지금까지 캐싱된 UTexture 포인터 목록 반환해주는 함수
 * @return {경로, 캐싱된 UTexture 포인터}
 */
const TMap<FName, UTexture*>& UAssetManager::GetTextureCache() const
{
	return TextureManager->GetTextureCache();
}
