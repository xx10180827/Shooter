#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterCombatFeedbackWidget.generated.h"
class UShooterCombatFeedbackComponent;
/** 纯绘制层：四段命中线与击杀徽章，不改血量、枪械或输入。 */
UCLASS()
class MYSHOOT_API UShooterCombatFeedbackWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void ObserveFeedback(UShooterCombatFeedbackComponent* Value);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
    UPROPERTY(Transient) TObjectPtr<UShooterCombatFeedbackComponent> Feedback;
    FSlateBrush IconBrush;
};
