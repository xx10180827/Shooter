#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerInput.h"
#include "Combat/ShooterCombatFeedbackComponent.h"
#include "UI/ShooterCombatFeedbackWidget.h"
#include "UI/ShooterMouseSettingsWidget.h"
#include "UI/ShooterHealthWidget.h"
#include "UI/ShooterPickupWidget.h"
#include "UI/ShooterDashWidget.h"
#include "UI/ShooterAmmoWidget.h"
#include "UI/ShooterMenuWidget.h"
#include "UI/ShooterAimReticleWidget.h"
#include "Weapons/ShooterAimComponent.h"
#include "Characters/ShooterCharacterBase.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Components/InputComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/LocalPlayer.h"

AShooterPlayerController::AShooterPlayerController()
{
    OverridePlayerInputClass = UShooterPlayerInput::StaticClass();
    CombatFeedback = CreateDefaultSubobject<UShooterCombatFeedbackComponent>(TEXT("CombatFeedback"));
}

void AShooterPlayerController::BeginPlay()
{
    Super::BeginPlay();
    RefreshHUD();
    ObservedGameMode = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
    if (ObservedGameMode.IsValid())
    {
        ObservedGameMode->OnRoundChanged.AddUniqueDynamic(this, &AShooterPlayerController::HandleRoundChanged);
        HandleRoundChanged(ObservedGameMode->GetRoundState());
    }
}
void AShooterPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    // P 在 PIE 中不与编辑器默认 Esc 停止预览冲突；独立游戏同时支持 Esc。
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AShooterPlayerController::ToggleGameMenu).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AShooterPlayerController::ToggleGameMenu).bExecuteWhenPaused = true;
}
void AShooterPlayerController::ToggleGameMenu()
{
    if (ObservedGameMode.IsValid()) { ObservedGameMode->TogglePause(); }
}
void AShooterPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (HasActorBegunPlay())
    {
        RefreshHUD();
        if (ObservedGameMode.IsValid()) { HandleRoundChanged(ObservedGameMode->GetRoundState()); }
    }
}
void AShooterPlayerController::RefreshHUD()
{
    if (!IsLocalController() || !GetLocalPlayer()) { return; }
    if (!CombatFeedbackWidget)
    {
        CombatFeedbackWidget = CreateWidget<UShooterCombatFeedbackWidget>(this, UShooterCombatFeedbackWidget::StaticClass());
        if (CombatFeedbackWidget) { CombatFeedbackWidget->AddToPlayerScreen(105); CombatFeedbackWidget->SetVisibility(ESlateVisibility::HitTestInvisible); }
    }
    CombatFeedback->ObserveWeapon(GetPawn() ? GetPawn()->FindComponentByClass<UShooterWeaponComponent>() : nullptr);
    if (CombatFeedbackWidget) { CombatFeedbackWidget->ObserveFeedback(CombatFeedback); }
    if(!MouseSettingsWidget)
    {
        MouseSettingsWidget=CreateWidget<UShooterMouseSettingsWidget>(this,UShooterMouseSettingsWidget::StaticClass());
        if(MouseSettingsWidget) { MouseSettingsWidget->AddToPlayerScreen(110); }
    }
    if(!DashWidget)
    {
        DashWidget=CreateWidget<UShooterDashWidget>(this,UShooterDashWidget::StaticClass());
        if(DashWidget) { DashWidget->SetVisibility(ESlateVisibility::HitTestInvisible); DashWidget->AddToPlayerScreen(14); }
    }
    if(!PickupWidget)
    {
        PickupWidget=CreateWidget<UShooterPickupWidget>(this,UShooterPickupWidget::StaticClass());
        if(PickupWidget) { PickupWidget->SetVisibility(ESlateVisibility::HitTestInvisible); PickupWidget->AddToPlayerScreen(13); }
    }
    if (!HealthWidget && HealthWidgetClass)
    {
        HealthWidget = CreateWidget<UShooterHealthWidget>(this, HealthWidgetClass);
        if (HealthWidget) { HealthWidget->AddToPlayerScreen(10); }
    }
    if (!AmmoWidget && AmmoWidgetClass)
    {
        AmmoWidget = CreateWidget<UShooterAmmoWidget>(this, AmmoWidgetClass);
        if (AmmoWidget) { AmmoWidget->AddToPlayerScreen(11); }
    }
    if (!MenuWidget && MenuWidgetClass)
    {
        MenuWidget = CreateWidget<UShooterMenuWidget>(this, MenuWidgetClass);
        if (MenuWidget) { MenuWidget->AddToPlayerScreen(100); }
    }
    if (!AimReticleWidget)
    {
        AimReticleWidget = CreateWidget<UShooterAimReticleWidget>(this, UShooterAimReticleWidget::StaticClass());
        if (AimReticleWidget)
        {
            AimReticleWidget->SetVisibility(ESlateVisibility::Collapsed);
            AimReticleWidget->AddToPlayerScreen(12);
        }
    }
    // 更换角色时解除旧订阅；准星随状态事件更新，不在 Tick 中遍历界面。
    if (ObservedAim.IsValid())
    {
        ObservedAim->OnAimingChanged.RemoveDynamic(this, &AShooterPlayerController::HandleAimingChanged);
    }
    ObservedAim = GetPawn() ? GetPawn()->FindComponentByClass<UShooterAimComponent>() : nullptr;
    if (ObservedAim.IsValid())
    {
        ObservedAim->OnAimingChanged.AddUniqueDynamic(this, &AShooterPlayerController::HandleAimingChanged);
    }
    RefreshCrosshair();
    if (HealthWidget) { HealthWidget->ObserveCharacter(Cast<AShooterCharacterBase>(GetPawn())); }
    if (AmmoWidget) { AmmoWidget->ObserveWeapon(GetPawn() ? GetPawn()->FindComponentByClass<UShooterWeaponComponent>() : nullptr); }
}
void AShooterPlayerController::HandleRoundChanged(EShooterRoundState State)
{
    if (!IsLocalController() || !GetLocalPlayer()) { return; }
    const bool bPlaying = State == EShooterRoundState::Playing;
    CombatFeedback->HandleRoundState(State);
    if (CombatFeedbackWidget) { CombatFeedbackWidget->SetVisibility(bPlaying || State == EShooterRoundState::Won ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    SetPause(State == EShooterRoundState::Menu || State == EShooterRoundState::Paused);
    // 避免每次状态切换累积 IgnoreInput 计数；新一局由关卡重载完全恢复。
    ResetIgnoreMoveInput(); ResetIgnoreLookInput();
    SetIgnoreMoveInput(!bPlaying); SetIgnoreLookInput(!bPlaying);
    if (APawn* ControlledPawn = GetPawn())
    {
        if (bPlaying) { ControlledPawn->EnableInput(this); }
        else { ControlledPawn->DisableInput(this); }
    }
    FlushPressedKeys();
    bShowMouseCursor = !bPlaying;
    if(DashWidget) { DashWidget->SetVisibility(bPlaying?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed); }
    if(PickupWidget) { PickupWidget->SetVisibility(bPlaying?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed); }
    if (HealthWidget) { HealthWidget->SetVisibility(bPlaying ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (AmmoWidget) { AmmoWidget->SetVisibility(bPlaying ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    RefreshCrosshair();
    if (MenuWidget) { MenuWidget->ShowState(State); }
    if(MouseSettingsWidget) { MouseSettingsWidget->ShowForState(State); }
    if (bPlaying)
    {
        FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); SetInputMode(Mode);
    }
    else
    {
        FInputModeUIOnly Mode;
        if (MenuWidget) { Mode.SetWidgetToFocus(MenuWidget->TakeWidget()); }
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
}
void AShooterPlayerController::HandleAimingChanged(bool bIsAiming)
{
    RefreshCrosshair();
}

void AShooterPlayerController::RefreshCrosshair()
{
    if (!IsLocalController()) { return; }
    const AShooterCharacterBase* ControlledCharacter = Cast<AShooterCharacterBase>(GetPawn());
    const bool bShowReticle = ControlledCharacter && ControlledCharacter->GetGASHealth() > 0.f
        && !ControlledCharacter->HasGASDeathStarted() && AShooterGameMode::IsCombatAllowed(this);
    const bool bAiming = ObservedAim.IsValid() && ObservedAim->IsAiming();

    // 保留原 Shooter_UI 资产和创建节点：普通视角显示十字，瞄准时仅显示小红点。
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), true);
    for (UUserWidget* Widget : Widgets)
    {
        if (Widget && Widget->GetClass()->GetFName() == TEXT("Shooter_UI_C")
            && (!Widget->GetOwningPlayer() || Widget->GetOwningPlayer() == this))
        {
            Widget->SetVisibility(bShowReticle && !bAiming
                ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
    }
    if (AimReticleWidget)
    {
        AimReticleWidget->SetVisibility(bShowReticle && bAiming
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}
void AShooterPlayerController::OnUnPossess()
{
    CombatFeedback->ObserveWeapon(nullptr);
    if (HealthWidget) { HealthWidget->ObserveCharacter(nullptr); }
    if (AmmoWidget) { AmmoWidget->ObserveWeapon(nullptr); }
    if (ObservedAim.IsValid())
    {
        ObservedAim->OnAimingChanged.RemoveDynamic(this, &AShooterPlayerController::HandleAimingChanged);
    }
    ObservedAim.Reset();
    Super::OnUnPossess();
    RefreshCrosshair();
}
void AShooterPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    CombatFeedback->ObserveWeapon(nullptr);
    if (CombatFeedbackWidget) { CombatFeedbackWidget->RemoveFromParent(); CombatFeedbackWidget = nullptr; }
    if (ObservedGameMode.IsValid()) { ObservedGameMode->OnRoundChanged.RemoveDynamic(this, &AShooterPlayerController::HandleRoundChanged); }
    if (HealthWidget) { HealthWidget->ObserveCharacter(nullptr); HealthWidget->RemoveFromParent(); HealthWidget = nullptr; }
    if (AmmoWidget) { AmmoWidget->ObserveWeapon(nullptr); AmmoWidget->RemoveFromParent(); AmmoWidget = nullptr; }
    if(MouseSettingsWidget) { MouseSettingsWidget->RemoveFromParent(); MouseSettingsWidget=nullptr; }
    if (MenuWidget) { MenuWidget->RemoveFromParent(); MenuWidget = nullptr; }
    if(PickupWidget) { PickupWidget->RemoveFromParent(); PickupWidget=nullptr; }
    if(DashWidget) { DashWidget->RemoveFromParent(); DashWidget=nullptr; }
    if (ObservedAim.IsValid())
    {
        ObservedAim->OnAimingChanged.RemoveDynamic(this, &AShooterPlayerController::HandleAimingChanged);
    }
    ObservedAim.Reset();
    if (AimReticleWidget) { AimReticleWidget->RemoveFromParent(); AimReticleWidget = nullptr; }
    Super::EndPlay(Reason);
}
