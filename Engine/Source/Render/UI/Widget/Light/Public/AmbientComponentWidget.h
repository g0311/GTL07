#pragma once
#include "LightComponentWidget.h"

class UClass;
class UAmbientLightComponent;

UCLASS()
class UAmbientComponentWidget : public ULightComponentWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(UAmbientComponentWidget, ULightComponentWidget)
    
public:
    UAmbientComponentWidget() = default;
    virtual ~UAmbientComponentWidget() = default;
};
