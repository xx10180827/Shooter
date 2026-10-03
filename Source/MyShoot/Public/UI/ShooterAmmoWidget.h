#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterAmmoWidget.generated.h"
class UShooterWeaponComponent;
class UTextBlock;

/** 只订阅武器事件的弹药 HUD，不维护第二套弹药规则。 */
UCLASS()
class MYSHOOT_API UShooterAmmoWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void ObserveWeapon(UShooterWeaponComponent* Weapon);
    int32 GetDisplayedAmmo() const { return DisplayedAmmo; }
    int32 GetDisplayedReserve() const { return DisplayedReserve; }
    bool GetDisplayedReloading() const { return bDisplayedReloading; }
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> AmmoValue;
    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> AmmoStatus;
private:
    UFUNCTION() void RefreshAmmo();
    UFUNCTION()
    void OnWeaponChanged(int32 OldSlot, int32 NewSlot);
    UFUNCTION()
    void OnAmmoChanged(int32 OldValue, int32 NewValue);
    UFUNCTION()
    void OnReloadStarted(float Duration);
    UFUNCTION()
    void OnReloadFinished(bool bSucceeded);
    TWeakObjectPtr<UShooterWeaponComponent> ObservedWeapon;
    int32 DisplayedAmmo = 0;
    int32 DisplayedReserve = 0;
    bool bDisplayedReloading = false;
};
