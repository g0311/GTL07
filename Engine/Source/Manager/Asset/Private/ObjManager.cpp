#include "pch.h"

#include "Core/Public/ObjectIterator.h"
#include "Manager/Asset/Public/ObjManager.h"
#include "Manager/Asset/Public/ObjImporter.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Material.h"
#include "Texture/Public/Texture.h"
#include <filesystem>

// static 멤버 변수의 실체를 정의(메모리 할당)합니다.
TMap<FName, std::unique_ptr<FStaticMesh>> FObjManager::ObjFStaticMeshMap;

/** @brief: Vertex Key for creating index buffer */
using VertexKey = std::tuple<size_t, size_t, size_t>;

struct VertexKeyHash
{
	std::size_t operator() (VertexKey Key) const
	{
		auto Hash1 = std::hash<size_t>{}(std::get<0>(Key));
		auto Hash2 = std::hash<size_t>{}(std::get<1>(Key));
		auto Hash3 = std::hash<size_t>{}(std::get<2>(Key));

		std::size_t Seed = Hash1;
		Seed ^= Hash2 + 0x9e3779b97f4a7c15ULL + (Seed << 6) + (Seed >> 2);
		Seed ^= Hash3 + 0x9e3779b97f4a7c15ULL + (Seed << 6) + (Seed >> 2);

		return Seed;
	}
};

/**
 * @brief Calculate tangent and bitangent vectors for normal mapping
 * @param Vertices The vertex array to modify
 * @param Indices The index array defining triangles
 */
static void CalculateTangentBitangent(TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices)
{
	// Initialize tangent and bitangent to zero
	for (auto& Vertex : Vertices)
	{
		Vertex.Tangent = FVector(0.0f, 0.0f, 0.0f);
		Vertex.Bitangent = FVector(0.0f, 0.0f, 0.0f);
	}

	// Calculate tangent and bitangent for each triangle
	for (size_t i = 0; i < Indices.size(); i += 3)
	{
		uint32 Index0 = Indices[i];
		uint32 Index1 = Indices[i + 1];
		uint32 Index2 = Indices[i + 2];

		FNormalVertex& V0 = Vertices[Index0];
		FNormalVertex& V1 = Vertices[Index1];
		FNormalVertex& V2 = Vertices[Index2];

		// Position deltas
		FVector Edge1 = V1.Position - V0.Position;
		FVector Edge2 = V2.Position - V0.Position;

		// UV deltas
		FVector2 DeltaUV1 = V1.TexCoord - V0.TexCoord;
		FVector2 DeltaUV2 = V2.TexCoord - V0.TexCoord;

		// Calculate tangent and bitangent
		float Denominator = DeltaUV1.X * DeltaUV2.Y - DeltaUV2.X * DeltaUV1.Y;

		if (std::abs(Denominator) > 1e-6f)
		{
			float r = 1.0f / Denominator;

			FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * r;
			FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * r;

			// Accumulate tangent and bitangent for each vertex
			V0.Tangent = V0.Tangent + Tangent;
			V1.Tangent = V1.Tangent + Tangent;
			V2.Tangent = V2.Tangent + Tangent;

			V0.Bitangent = V0.Bitangent + Bitangent;
			V1.Bitangent = V1.Bitangent + Bitangent;
			V2.Bitangent = V2.Bitangent + Bitangent;
		}
	}

	// Normalize and orthogonalize tangent and bitangent
	for (auto& Vertex : Vertices)
	{
		FVector N = Vertex.Normal;
		FVector T = Vertex.Tangent;

		// Check if normal is valid
		float NormalLength = std::sqrt(N.X * N.X + N.Y * N.Y + N.Z * N.Z);
		if (NormalLength < 1e-6f)
		{
			// Invalid normal, use default up vector
			N = FVector(0.0f, 0.0f, 1.0f);
			Vertex.Normal = N;
		}
		else
		{
			N = N / NormalLength; // Normalize
		}

		// Check if tangent is valid
		float TangentLength = std::sqrt(T.X * T.X + T.Y * T.Y + T.Z * T.Z);
		if (TangentLength < 1e-6f)
		{
			// No tangent calculated, create arbitrary tangent perpendicular to normal
			if (std::abs(N.Z) < 0.999f)
			{
				T = FVector(0.0f, 0.0f, 1.0f);
			}
			else
			{
				T = FVector(1.0f, 0.0f, 0.0f);
			}
		}

		// Gram-Schmidt orthogonalize tangent with respect to normal
		T = T - N * N.Dot(T);
		float TOrthoLength = std::sqrt(T.X * T.X + T.Y * T.Y + T.Z * T.Z);
		if (TOrthoLength > 1e-6f)
		{
			T = T / TOrthoLength;
		}
		else
		{
			// Fallback: create arbitrary tangent
			if (std::abs(N.Z) < 0.999f)
			{
				T = FVector(0.0f, 0.0f, 1.0f);
			}
			else
			{
				T = FVector(1.0f, 0.0f, 0.0f);
			}
			T = T - N * N.Dot(T);
			T = T / std::sqrt(T.X * T.X + T.Y * T.Y + T.Z * T.Z);
		}

		// Calculate bitangent
		FVector B = N.Cross(T);
		float BLength = std::sqrt(B.X * B.X + B.Y * B.Y + B.Z * B.Z);
		if (BLength > 1e-6f)
		{
			B = B / BLength;
		}

		Vertex.Tangent = T;
		Vertex.Bitangent = B;
	}
}

/** @todo: std::filesystem으로 변경 */
FStaticMesh* FObjManager::LoadObjStaticMeshAsset(const FName& PathFileName, const FObjImporter::Configuration& Config)
{
	auto Iter = ObjFStaticMeshMap.find(PathFileName);
	if (Iter != ObjFStaticMeshMap.end())
	{
		return Iter->second.get();
	}

	/** #1. '.obj' 파일로부터 오브젝트 정보를 로드 */
	FObjInfo ObjInfo;
	if (!FObjImporter::LoadObj(PathFileName.ToString(), &ObjInfo, Config))
	{
		UE_LOG_ERROR("파일 정보를 읽어오는데 실패했습니다: %s", PathFileName.ToString());
		return nullptr;
	}

	auto StaticMesh = std::make_unique<FStaticMesh>();
	StaticMesh->PathFileName = PathFileName;

	if (ObjInfo.ObjectInfoList.size() == 0)
	{
		UE_LOG_ERROR("오브젝트 정보를 찾을 수 없습니다");
		return nullptr;
	}

	/** #2. 오브젝트 정보로부터 버텍스 배열과 인덱스 배열을 구성 */
	/** @note: Use only first object in '.obj' file to create FStaticMesh. */
	FObjectInfo& ObjectInfo = ObjInfo.ObjectInfoList[0];

	TMap<VertexKey, size_t, VertexKeyHash> VertexMap;
	for (size_t i = 0; i < ObjectInfo.VertexIndexList.size(); ++i)
	{
		size_t VertexIndex = ObjectInfo.VertexIndexList[i];

		size_t NormalIndex = INVALID_INDEX;
		if (!ObjectInfo.NormalIndexList.empty())
		{
			NormalIndex = ObjectInfo.NormalIndexList[i];
		}

		size_t TexCoordIndex = INVALID_INDEX;
		if (!ObjectInfo.TexCoordIndexList.empty())
		{
			TexCoordIndex = ObjectInfo.TexCoordIndexList[i];
		}

		VertexKey Key{ VertexIndex, NormalIndex, TexCoordIndex };
		auto It = VertexMap.find(Key);
		if (It == VertexMap.end())
		{
			FNormalVertex Vertex = {};
			Vertex.Position = ObjInfo.VertexList[VertexIndex];

			if (NormalIndex != INVALID_INDEX)
			{
				assert("Vertex normal index out of range" && NormalIndex < ObjInfo.NormalList.size());
				Vertex.Normal = ObjInfo.NormalList[NormalIndex];
			}

			if (TexCoordIndex != INVALID_INDEX)
			{
				assert("Texture coordinate index out of range" && TexCoordIndex < ObjInfo.TexCoordList.size());
				Vertex.TexCoord = ObjInfo.TexCoordList[TexCoordIndex];
			}

			size_t Index = StaticMesh->Vertices.size();
			StaticMesh->Vertices.push_back(Vertex);
			StaticMesh->Indices.push_back(Index);
			VertexMap[Key] = Index;
		}
		else
		{
			StaticMesh->Indices.push_back(It->second);
		}
	}

	/** #2.5. Normal이 없으면 Face Normal 계산 */
	bool bHasNormals = false;
	for (const auto& Vertex : StaticMesh->Vertices)
	{
		float NormalLength = std::sqrt(Vertex.Normal.X * Vertex.Normal.X + Vertex.Normal.Y * Vertex.Normal.Y + Vertex.Normal.Z * Vertex.Normal.Z);
		if (NormalLength > 1e-6f)
		{
			bHasNormals = true;
			break;
		}
	}

	if (!bHasNormals)
	{
		UE_LOG("ObjManager: Normal 데이터가 없습니다. Face Normal을 계산합니다.");

		// Calculate face normals for each triangle
		for (size_t i = 0; i < StaticMesh->Indices.size(); i += 3)
		{
			uint32 Index0 = StaticMesh->Indices[i];
			uint32 Index1 = StaticMesh->Indices[i + 1];
			uint32 Index2 = StaticMesh->Indices[i + 2];

			FVector P0 = StaticMesh->Vertices[Index0].Position;
			FVector P1 = StaticMesh->Vertices[Index1].Position;
			FVector P2 = StaticMesh->Vertices[Index2].Position;

			// Calculate face normal using cross product
			FVector Edge1 = P1 - P0;
			FVector Edge2 = P2 - P0;
			FVector FaceNormal = Edge1.Cross(Edge2);

			float Length = std::sqrt(FaceNormal.X * FaceNormal.X + FaceNormal.Y * FaceNormal.Y + FaceNormal.Z * FaceNormal.Z);
			if (Length > 1e-6f)
			{
				FaceNormal = FaceNormal / Length;
			}

			// Accumulate face normal to vertex normals
			StaticMesh->Vertices[Index0].Normal = StaticMesh->Vertices[Index0].Normal + FaceNormal;
			StaticMesh->Vertices[Index1].Normal = StaticMesh->Vertices[Index1].Normal + FaceNormal;
			StaticMesh->Vertices[Index2].Normal = StaticMesh->Vertices[Index2].Normal + FaceNormal;
		}

		// Normalize all vertex normals
		for (auto& Vertex : StaticMesh->Vertices)
		{
			float Length = std::sqrt(Vertex.Normal.X * Vertex.Normal.X + Vertex.Normal.Y * Vertex.Normal.Y + Vertex.Normal.Z * Vertex.Normal.Z);
			if (Length > 1e-6f)
			{
				Vertex.Normal = Vertex.Normal / Length;
			}
			else
			{
				Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
			}
		}
	}

	/** #2.6. Tangent와 Bitangent를 계산 */
	CalculateTangentBitangent(StaticMesh->Vertices, StaticMesh->Indices);

	/** #3. 오브젝트가 사용하는 머티리얼의 목록을 저장 */
	TSet<FName> UniqueMaterialNames;
	for (const auto& MaterialName : ObjectInfo.MaterialNameList)
	{
		UniqueMaterialNames.insert(MaterialName);
	}

	StaticMesh->MaterialInfo.resize(UniqueMaterialNames.size());
	TMap<FName, int32> MaterialNameToSlot;
	int32 CurrentMaterialSlot = 0;

	for (const auto& MaterialName : UniqueMaterialNames)
	{
		for (size_t j = 0; j < ObjInfo.ObjectMaterialInfoList.size(); ++j)
		{
			if (MaterialName == ObjInfo.ObjectMaterialInfoList[j].Name)
			{
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Name = std::move(ObjInfo.ObjectMaterialInfoList[j].Name);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ka = ObjInfo.ObjectMaterialInfoList[j].Ka;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Kd = ObjInfo.ObjectMaterialInfoList[j].Kd;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ks = ObjInfo.ObjectMaterialInfoList[j].Ks;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ke = ObjInfo.ObjectMaterialInfoList[j].Ke;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ns = ObjInfo.ObjectMaterialInfoList[j].Ns;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Ni = ObjInfo.ObjectMaterialInfoList[j].Ni;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].D = ObjInfo.ObjectMaterialInfoList[j].D;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].Illumination = ObjInfo.ObjectMaterialInfoList[j].Illumination;
				StaticMesh->MaterialInfo[CurrentMaterialSlot].KaMap = std::move(ObjInfo.ObjectMaterialInfoList[j].KaMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].KdMap = std::move(ObjInfo.ObjectMaterialInfoList[j].KdMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].KsMap = std::move(ObjInfo.ObjectMaterialInfoList[j].KsMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].NsMap = std::move(ObjInfo.ObjectMaterialInfoList[j].NsMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].DMap = std::move(ObjInfo.ObjectMaterialInfoList[j].DMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].NormalMap = std::move(ObjInfo.ObjectMaterialInfoList[j].NormalMap);
				StaticMesh->MaterialInfo[CurrentMaterialSlot].HeightMap = std::move(ObjInfo.ObjectMaterialInfoList[j].HeightMap);

				MaterialNameToSlot.emplace(MaterialName, CurrentMaterialSlot);
				CurrentMaterialSlot++;
				break;
			}
		}
	}
	if (StaticMesh->MaterialInfo.empty())
	{
		StaticMesh->MaterialInfo.resize(1);
		StaticMesh->MaterialInfo[0].Name = "DefaultMaterial";
		StaticMesh->MaterialInfo[0].Kd = FVector(0.9f, 0.9f, 0.9f);
	}
	
	/** #4. 오브젝트의 서브메쉬 정보를 저장 */
	if (ObjectInfo.MaterialNameList.empty())
	{
		StaticMesh->Sections.resize(1);
		StaticMesh->Sections[0].StartIndex = 0;
		StaticMesh->Sections[0].IndexCount = StaticMesh->Indices.size();
		StaticMesh->Sections[0].MaterialSlot = 0;
	}
	else
	{
		StaticMesh->Sections.resize(ObjectInfo.MaterialIndexList.size());
		for (size_t i = 0; i < ObjectInfo.MaterialIndexList.size(); ++i)
		{
			StaticMesh->Sections[i].StartIndex = ObjectInfo.MaterialIndexList[i] * 3;
			if (i < ObjectInfo.MaterialIndexList.size() - 1)
			{
				StaticMesh->Sections[i].IndexCount = (ObjectInfo.MaterialIndexList[i + 1] - ObjectInfo.MaterialIndexList[i]) * 3;
			}
			else
			{
				StaticMesh->Sections[i].IndexCount = (StaticMesh->Indices.size() / 3 - ObjectInfo.MaterialIndexList[i]) * 3;
			}

			const FName& MaterialName = ObjectInfo.MaterialNameList[i];
			auto It = MaterialNameToSlot.find(MaterialName);
			if (It != MaterialNameToSlot.end())
			{
				StaticMesh->Sections[i].MaterialSlot = It->second;
			}
			else
			{
				StaticMesh->Sections[i].MaterialSlot = INVALID_INDEX;
			}
		}
	}

	StaticMesh->BVH.Build(StaticMesh.get()); // 빠른 피킹용 BVH 구축
	ObjFStaticMeshMap.emplace(PathFileName, std::move(StaticMesh));

	return ObjFStaticMeshMap[PathFileName].get();
}

/**
 * @brief MTL 정보를 바탕으로 UStaticMesh에 재질을 설정하는 함수
 */
void FObjManager::CreateMaterialsFromMTL(UStaticMesh* StaticMesh, FStaticMesh* StaticMeshAsset, const FName& ObjFilePath)
{
	if (!StaticMesh || !StaticMeshAsset || StaticMeshAsset->MaterialInfo.empty())
	{
		return;
	}

	// OBJ 파일이 있는 디렉토리 경로 추출
	std::filesystem::path ObjDirectory = std::filesystem::path(ObjFilePath.ToString()).parent_path();

	UAssetManager& AssetManager = UAssetManager::GetInstance();

	size_t MaterialCount = StaticMeshAsset->MaterialInfo.size();
	for (size_t i = 0; i < MaterialCount; ++i)
	{
		const FMaterial& MaterialInfo = StaticMeshAsset->MaterialInfo[i];
		auto* Material = NewObject<UMaterial>();
		Material->SetName(MaterialInfo.Name);

		// Diffuse 텍스처 로드 (map_Kd)
		if (!MaterialInfo.KdMap.empty())
		{
			// .generic_string()을 사용하여 경로를 '/'로 통일하고 std::replace 제거
			FString TexturePathStr = (ObjDirectory / MaterialInfo.KdMap).generic_string();

			if (std::filesystem::exists(TexturePathStr))
			{
				UTexture* DiffuseTexture = AssetManager.LoadTexture(TexturePathStr);
				if (DiffuseTexture)
				{
					Material->SetDiffuseTexture(DiffuseTexture);
				}
			}
		}

		// Ambient 텍스처 로드 (map_Ka)
		if (!MaterialInfo.KaMap.empty())
		{
			FString TexturePathStr = (ObjDirectory / MaterialInfo.KaMap).generic_string();

			if (std::filesystem::exists(TexturePathStr))
			{
				UTexture* AmbientTexture = AssetManager.LoadTexture(TexturePathStr);
				if (AmbientTexture)
				{
					Material->SetAmbientTexture(AmbientTexture);
				}
			}
		}

		// Specular 텍스처 로드 (map_Ks)
		if (!MaterialInfo.KsMap.empty())
		{
			FString TexturePathStr = (ObjDirectory / MaterialInfo.KsMap).generic_string();

			if (std::filesystem::exists(TexturePathStr))
			{
				UTexture* SpecularTexture = AssetManager.LoadTexture(TexturePathStr);
				if (SpecularTexture)
				{
					Material->SetSpecularTexture(SpecularTexture);
				}
			}
		}

		// Alpha 텍스처 로드 (map_d)
		if (!MaterialInfo.DMap.empty())
		{
			FString TexturePathStr = (ObjDirectory / MaterialInfo.DMap).generic_string();

			if (std::filesystem::exists(TexturePathStr))
			{
				UTexture* AlphaTexture = AssetManager.LoadTexture(TexturePathStr);
				if (AlphaTexture)
				{
					Material->SetAlphaTexture(AlphaTexture);
				}
			}
		}

		StaticMesh->SetMaterial(static_cast<int32>(i), Material);
	}
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FName& PathFileName, const FObjImporter::Configuration& Config)
{
	// 1) Try AssetManager cache first (non-owning lookup)
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	if (UStaticMesh* Cached = AssetManager.GetStaticMeshFromCache(PathFileName))
	{
		return Cached;
	}

	// 2) Load asset-level data (FStaticMesh) if not cached
	FStaticMesh* StaticMeshAsset = FObjManager::LoadObjStaticMeshAsset(PathFileName, Config);
	if (StaticMeshAsset)
	{
		// Create runtime UStaticMesh and register to AssetManager cache (ownership there)
		UStaticMesh* StaticMesh = new UStaticMesh();
		StaticMesh->SetStaticMeshAsset(StaticMeshAsset);

		// Create materials based on MTL information
		CreateMaterialsFromMTL(StaticMesh, StaticMeshAsset, PathFileName);

		// Register into AssetManager's cache (takes ownership)
		AssetManager.AddStaticMeshToCache(PathFileName, StaticMesh);

		return StaticMesh;
	}

	return nullptr;
}
