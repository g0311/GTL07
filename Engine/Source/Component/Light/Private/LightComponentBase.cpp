#include "pch.h"
#include "Component/Light/Public/LightComponentBase.h"
#include "Utility/Public/JsonSerializer.h"

namespace
{
    void TryReadVector4(const JSON& InJson, const FString& Key, FVector4& InOutValue)
    {
        if (!InJson.hasKey(Key))
        {
            return;
        }

        const JSON& ColorJson = InJson.at(Key);
        if (ColorJson.JSONType() != JSON::Class::Array || ColorJson.size() != 4)
        {
            return;
        }

        try
        {
            InOutValue = {
                static_cast<float>(ColorJson.at(0).ToFloat()),
                static_cast<float>(ColorJson.at(1).ToFloat()),
                static_cast<float>(ColorJson.at(2).ToFloat()),
                static_cast<float>(ColorJson.at(3).ToFloat())
            };
        }
        catch (...)
        {
            // Leave existing color if parsing fails
        }
    }

    JSON Vector4ToJson(const FVector4& InVector)
    {
        JSON ArrayJson = JSON::Make(JSON::Class::Array);
        ArrayJson.append(InVector.X, InVector.Y, InVector.Z, InVector.W);
        return ArrayJson;
    }
}

IMPLEMENT_ABSTRACT_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity, Intensity, false);
        TryReadVector4(InOutHandle, "Color", Color);
    }
    else
    {
        InOutHandle["Intensity"] = Intensity;
        InOutHandle["Color"] = Vector4ToJson(Color);
    }
}
