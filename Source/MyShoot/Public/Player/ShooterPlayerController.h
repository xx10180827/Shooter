#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/ShooterGameMode.h"
#include "ShooterPlayerController.generated.h"
class UShooterMouseSettingsWidget;
class UShooterPickupWidget;
class UShooterDashWidget;
class UShooterHealthWidget;
class UShooterAmmoWidget;
class UShooterMenuWidget;
class UShooterAimReticleWidget;
class UShooterAimComponent;

/** 本地界面与输入模式切换；GameMode 负责规则，控制器负责展示和生命周期。 */
UCLASS()
class MYSHOOT_API AShooterPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AShooterPlayerController();
    UShooterMouseSettingsWidget* GetMouseSettingsWidget() const { return MouseSettingsWidget; }
    UFUNCTION(BlueprintPure, Category="Shooter|HUD")
    UShooterHealthWidget* GetHealthWidget() const { return HealthWidget; }
    UShooterAmmoWidget* GetAmmoWidget() const { return AmmoWidget; }
    UShooterMenuWidget* GetMenuWidget() const { return MenuWidget; }
protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|HUD")
    TSubclassOf<UShooterHealthWidget> HealthWidgetClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|HUD")
    TSubclassOf<UShooterAmmoWidget> AmmoWidgetClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|HUD")
    TSubclassOf<UShooterMenuWidget> MenuWidgetClass;
private:
    void RefreshHUD();
    void RefreshCrosshair();
    UFUNCTION() void HandleAimingChanged(bool bIsAiming);
    void ToggleGameMenu();
    UFUNCTION() void HandleRoundChanged(EShooterRoundState State);
    UPROPERTY(Transient) TObjectPtr<UShooterMouseSettingsWidget> MouseSettingsWidget;
    UPROPERTY(Transient) TObjectPtr<UShooterDashWidget> DashWidget;
    UPROPERTY(Transient) TObjectPtr<UShooterPickupWidget> PickupWidget;
    UPROPERTY(Transient) TObjectPtr<UShooterHealthWidget> HealthWidget;
    UPROPERTY(Transient) TObjectPtr<UShooterAmmoWidget> AmmoWidget;
    UPROPERTY(Transient) TObjectPtr<UShooterMenuWidget> MenuWidget;
    UPROPERTY(Transient) TObjectPtr<UShooterAimReticleWidget> AimReticleWidget;
    TWeakObjectPtr<UShooterAimComponent> ObservedAim;
    TWeakObjectPtr<AShooterGameMode> ObservedGameMode;
};
