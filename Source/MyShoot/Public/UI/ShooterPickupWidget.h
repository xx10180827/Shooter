#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterPickupWidget.generated.h"
class STextBlock;
/** 屏幕准星下方的提示，仅显示交互组件结果，不负责授予物品。 */
UCLASS()
class MYSHOOT_API UShooterPickupWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    TSharedPtr<STextBlock> PromptText;
};
