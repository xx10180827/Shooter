#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterHealthWidget.generated.h"
class AShooterCharacterBase;
class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

/** 屏幕玩家血条：控件布局/图片由 Widget Blueprint 配置，只订阅 GAS，不维护战斗血量。 */
UCLASS()
class MYSHOOT_API UShooterHealthWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Shooter|HUD")
    void ObserveCharacter(AShooterCharacterBase* Character);
    UFUNCTION(BlueprintPure, Category="Shooter|HUD")
    float GetDisplayedHealth() const { return DisplayedHealth; }
    UFUNCTION(BlueprintPure, Category="Shooter|HUD")
    float GetDisplayedMaxHealth() const { return DisplayedMaxHealth; }
    UFUNCTION(BlueprintPure, Category="Shooter|HUD")
    float GetDisplayedFraction() const;
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    /** 必须在 WBP_ShooterHealthBar 中使用对应名称，绑定由蓝图编译器校验。 */
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UProgressBar> HealthFill;
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> HealthValue;
private:
    void UnbindAttributes();
    void HandleAttributeChanged(const FOnAttributeChangeData& Data);
    void RefreshHealth();
    TWeakObjectPtr<AShooterCharacterBase> ObservedCharacter;
    TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
    FDelegateHandle HealthHandle;
    FDelegateHandle MaxHealthHandle;
    float DisplayedHealth = 0;
    float DisplayedMaxHealth = 100;
};
