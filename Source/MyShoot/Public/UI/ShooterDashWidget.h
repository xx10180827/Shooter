#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "ShooterDashWidget.generated.h"
class UMaterialInterface;
class UMaterialInstanceDynamic;
/** 屏幕中央偏下的体力弧线、血条右侧的闪避冷却图标；仅读取 GAS 状态。 */
UCLASS()
class MYSHOOT_API UShooterDashWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UShooterDashWidget(const FObjectInitializer& ObjectInitializer);
protected:
    virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
    virtual void NativeDestruct() override;
    // 软引用纳入资源依赖，实际运行只在控件建立时加载一次。
    UPROPERTY(EditDefaultsOnly,Category="Shooter|StaminaUI") TSoftObjectPtr<UMaterialInterface> StaminaMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> EnergyMaterial;
    FSlateBrush EnergyBrush;
    float LastFraction=-1.f;
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& Elements,int32 LayerId,const FWidgetStyle& Style,bool bParentEnabled) const override;
};