#include "Player/ShooterPlayerController.h"
#include "UI/ShooterHealthWidget.h"
#include "UI/ShooterAmmoWidget.h"
#include "UI/ShooterMenuWidget.h"
#include "Characters/ShooterCharacterBase.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Components/InputComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/LocalPlayer.h"

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
    if (HealthWidget) { HealthWidget->ObserveCharacter(Cast<AShooterCharacterBase>(GetPawn())); }
    if (AmmoWidget) { AmmoWidget->ObserveWeapon(GetPawn() ? GetPawn()->FindComponentByClass<UShooterWeaponComponent>() : nullptr); }
}
void AShooterPlayerController::HandleRoundChanged(EShooterRoundState State)
{
    if (!IsLocalController() || !GetLocalPlayer()) { return; }
    const bool bPlaying = State == EShooterRoundState::Playing;
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
    if (HealthWidget) { HealthWidget->SetVisibility(bPlaying ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (AmmoWidget) { AmmoWidget->SetVisibility(bPlaying ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    // 原准星仍由原蓝图创建，只随菜单切换可见性，不改旧蓝图节点。
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), true);
    for (UUserWidget* Widget : Widgets)
    {
        if (Widget && Widget->GetClass()->GetFName() == TEXT("Shooter_UI_C"))
        {
            Widget->SetVisibility(bPlaying ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
    }
    if (MenuWidget) { MenuWidget->ShowState(State); }
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
void AShooterPlayerController::OnUnPossess()
{
    if (HealthWidget) { HealthWidget->ObserveCharacter(nullptr); }
    if (AmmoWidget) { AmmoWidget->ObserveWeapon(nullptr); }
    Super::OnUnPossess();
}
void AShooterPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ObservedGameMode.IsValid()) { ObservedGameMode->OnRoundChanged.RemoveDynamic(this, &AShooterPlayerController::HandleRoundChanged); }
    if (HealthWidget) { HealthWidget->ObserveCharacter(nullptr); HealthWidget->RemoveFromParent(); HealthWidget = nullptr; }
    if (AmmoWidget) { AmmoWidget->ObserveWeapon(nullptr); AmmoWidget->RemoveFromParent(); AmmoWidget = nullptr; }
    if (MenuWidget) { MenuWidget->RemoveFromParent(); MenuWidget = nullptr; }
    Super::EndPlay(Reason);
}
