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

protected:
    ULightComponent* GetLightComponent() const { return LightComponent; }
    template<typename T> T* GetLightComponentAs() const { return Cast<T>(LightComponent); }

private:
    ULightComponent* LightComponent = nullptr;
};






