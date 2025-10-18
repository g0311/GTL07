#pragma once

#include "LightComponentWidget.h"

class UClass;
class UAmbientLightComponent;

class UAmbientComponentWidget : public  ULightComponentWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(UAmbientComponentWidget, ULightComponentWidget)
    
public:
    UAmbientComponentWidget() = default;
    virtual ~UAmbientComponentWidget() = default;

public:
    virtual void Initialize() override;
    virtual void Update() override;
    virtual void RenderWidget() override;
    
private:
    UAmbientLightComponent* AmbientLightComponent = nullptr;
};
