#pragma once
#include "LightComponentWidget.h"

class UClass;
class USpotLightComponent;

class USpotlightComponentWidget : public ULightComponentWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(USpotlightComponentWidget, ULightComponentWidget)
    
public:
    USpotlightComponentWidget() = default;
    virtual ~USpotlightComponentWidget() = default;
    
    virtual void Initialize() override;
    virtual void Update() override;
    virtual void RenderWidget() override;

private:
    
};
