#pragma once
#include "LightComponentWidget.h"

class UClass;
class UPointLightComponent;

UCLASS()
class UPointLightComponentWidget : public ULightComponentWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(UPointLightComponentWidget, ULightComponentWidget)

public:
    UPointLightComponentWidget() = default;
    virtual ~UPointLightComponentWidget() = default;

    virtual void Initialize() override;
    virtual void Update() override;
    virtual void RenderWidget() override;

private:
    UPointLightComponent* PointLightComponent = nullptr;
};
