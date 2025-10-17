#pragma once

#include "Render/UI/Widget/Public/Widget.h"

class UClass;
class ULightComponent;

UCLASS()
class ULightComponentWidget : public UWidget 
{
    GENERATED_BODY()
    DECLARE_CLASS(ULightComponentWidget, UWidget)
    
public:
    ULightComponentWidget() = default;
    virtual ~ULightComponentWidget() = default;

    /*-----------------------------------------------------------------------------
    UWidget Features
 -----------------------------------------------------------------------------*/
public:
    virtual void Initialize() override;
    virtual void Update() override;
    virtual void RenderWidget() override;

private:
    ULightComponent* LightComponent = nullptr;
};
