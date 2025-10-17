#pragma once

#include "Render/UI/Widget/Light/Public/LightComponentWidget.h"

UCLASS()
class UAmbientLightComponentWidget : public ULightComponentWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(UAmbientLightComponentWidget, ULightComponentWidget)

public:
    UAmbientLightComponentWidget() = default;
    virtual ~UAmbientLightComponentWidget() = default;
};
