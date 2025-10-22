#include "pch.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Physics/Public/AABB.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/UI/Widget/Public/SpriteSelectionWidget.h"
#include "Texture/Public/Texture.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UBillBoardComponent, UPrimitiveComponent)

UBillBoardComponent::UBillBoardComponent()
{
    UAssetManager& ResourceManager = UAssetManager::GetInstance();

	Vertices = ResourceManager.GetVertexData(EPrimitiveType::Sprite);
	VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Sprite);
	NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Sprite);

	Indices = ResourceManager.GetIndexData(EPrimitiveType::Sprite);
	IndexBuffer = ResourceManager.GetIndexBuffer(EPrimitiveType::Sprite);
	NumIndices = ResourceManager.GetNumIndices(EPrimitiveType::Sprite);

	RenderState.CullMode = ECullMode::Back;
	RenderState.FillMode = EFillMode::Solid;
	BoundingBox = &ResourceManager.GetAABB(EPrimitiveType::Sprite);

    const TMap<FName, UTexture*>& TextureCache = UAssetManager::GetInstance().GetTextureCache();
    if (!TextureCache.empty()) { Sprite = TextureCache.begin()->second; }

    bReceivesDecals = false;

	// Set a new base scale because the default from USceneComponent (0.3) is too small.
	SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
}

UBillBoardComponent::~UBillBoardComponent() = default;

void UBillBoardComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        FString SpritePath;
        FJsonSerializer::ReadString(InOutHandle, "BillBoardSprite", SpritePath, "");
        if (!SpritePath.empty())
            SetSprite(UAssetManager::GetInstance().LoadTexture(FName(SpritePath)));

        FString ScreenSizeScaledString;
        FJsonSerializer::ReadString(InOutHandle, "BillBoardScreenSizeScaled", ScreenSizeScaledString, "false");
        bScreenSizeScaled = ScreenSizeScaledString == "true";

        FString ScreenSizeString;
        FJsonSerializer::ReadString(InOutHandle, "BillBoardScreenSize", ScreenSizeString, "0.1");
        ScreenSize = stof(ScreenSizeString);
    }
    // 저장
    else
    {
        InOutHandle["BillBoardSprite"] = Sprite->GetFilePath().ToBaseNameString();
        InOutHandle["BillBoardScreenSizeScaled"] = bScreenSizeScaled ? "true" : "false"; 
        InOutHandle["BillBoardScreenSize"] = to_string(ScreenSize); 
    }}

void UBillBoardComponent::GetWorldAABB(FVector& OutMin, FVector& OutMax)
{
	if (!BoundingBox)
	{
		OutMin = FVector(); OutMax = FVector();
		return;
	}

	if (bIsAABBCacheDirty)
	{
		// 회전 무시하고 위치 기준으로 구형 AABB 반환 (옥트리 업데이트 최소화)
		FVector WorldPos = GetWorldLocation();
		FVector Scale = GetWorldScale3D();
		float MaxExtent = max(max(Scale.X, Scale.Y), Scale.Z) * 0.5f;
		
		CachedWorldMin = WorldPos - FVector(MaxExtent, MaxExtent, MaxExtent);
		CachedWorldMax = WorldPos + FVector(MaxExtent, MaxExtent, MaxExtent);
		bIsAABBCacheDirty = false;
	}
	
	OutMin = CachedWorldMin;
	OutMax = CachedWorldMax;
}

void UBillBoardComponent::FaceCamera(const FVector& CameraForward)
{
	FVector Forward = CameraForward;
	FVector Right = Forward.Cross(FVector::UpVector()); Right.Normalize();
	FVector Up = Right.Cross(Forward); Up.Normalize();
    
	// Construct the rotation matrix from the basis vectors
	FMatrix RotationMatrix = FMatrix(Forward, Right, Up);
    
	// Convert the rotation matrix to a quaternion and set the relative rotation
	SetWorldRotation(FQuaternion::FromRotationMatrix(RotationMatrix));
}

UTexture* UBillBoardComponent::GetSprite() const
{
    return Sprite;
}

void UBillBoardComponent::SetSprite(UTexture* InSprite)
{
    Sprite = InSprite;
}

UClass* UBillBoardComponent::GetSpecificWidgetClass() const
{
    return USpriteSelectionWidget::StaticClass();
}

const FRenderState& UBillBoardComponent::GetClassDefaultRenderState()
{
    static FRenderState DefaultRenderState { ECullMode::Back, EFillMode::Solid };
    return DefaultRenderState;
}
