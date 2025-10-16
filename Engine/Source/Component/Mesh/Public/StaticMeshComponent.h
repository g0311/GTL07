#pragma once
#include "Core/Public/Class.h"       // UObject 기반 클래스 및 매크로
#include "Component/Mesh/Public/MeshComponent.h"
#include "Component/Mesh/Public/StaticMesh.h"

namespace json { class JSON; }
using JSON = json::JSON;

UCLASS()
class UStaticMeshComponent : public UMeshComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

public:
	UStaticMeshComponent();
	~UStaticMeshComponent();

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

public:
	UStaticMesh* GetStaticMesh() { return StaticMesh; }
	void SetStaticMesh(const FName& InObjPath);

	UClass* GetSpecificWidgetClass() const override;

	UMaterial* GetMaterial(int32 Index) const;
	void SetMaterial(int32 Index, UMaterial* InMaterial);

	void EnableScroll() { bIsScrollEnabled = true; }
	void DisableScroll() { bIsScrollEnabled = false; }
	bool IsScrollEnabled() const { return bIsScrollEnabled; }

	void SetElapsedTime(float InElapsedTime) { ElapsedTime = InElapsedTime; }
	float GetElapsedTime() const { return ElapsedTime; }

	static const FRenderState& GetClassDefaultRenderState(); 

private:
	UStaticMesh* StaticMesh;

	// MaterialList
	TArray<UMaterial*> OverrideMaterials;

	// Scroll
	bool bIsScrollEnabled;
	float ElapsedTime;
	
public:
	virtual UObject* Duplicate() override;

protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;
};
