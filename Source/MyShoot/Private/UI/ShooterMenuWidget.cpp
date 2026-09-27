#include "UI/ShooterMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

AShooterGameMode* UShooterMenuWidget::GetShooterGameMode() const
{
    return GetWorld() ? Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
}
void UShooterMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    StartButton->OnClicked.AddUniqueDynamic(this, &UShooterMenuWidget::StartClicked);
    EndButton->OnClicked.AddUniqueDynamic(this, &UShooterMenuWidget::EndClicked);
    MenuButton->OnClicked.AddUniqueDynamic(this, &UShooterMenuWidget::EndClicked);
    QuitButton->OnClicked.AddUniqueDynamic(this, &UShooterMenuWidget::QuitClicked);
    ResultQuitButton->OnClicked.AddUniqueDynamic(this, &UShooterMenuWidget::QuitClicked);
    RestartButton->OnClicked.AddUniqueDynamic(this, &UShooterMenuWidget::RestartClicked);
    if (AShooterGameMode* GM = GetShooterGameMode()) { ShowState(GM->GetRoundState()); }
}
void UShooterMenuWidget::ShowState(EShooterRoundState State)
{
    const bool bResult = State == EShooterRoundState::Won || State == EShooterRoundState::Lost;
    StartPanel->SetVisibility(bResult ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    ResultPanel->SetVisibility(bResult ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    EndButton->SetIsEnabled(State == EShooterRoundState::Paused);
    EndButton->SetToolTipText(FText::FromString(TEXT("End this round and return to the start menu")));
    MenuHint->SetText(FText::FromString(State == EShooterRoundState::Paused
        ? TEXT("PAUSED  |  START GAME: CONTINUE  |  END GAME: MAIN MENU")
        : TEXT("WASD MOVE   |   MOUSE FIRE   |   R RELOAD   |   P / ESC MENU")));
    ResultTitle->SetText(FText::FromString(State == EShooterRoundState::Won ? TEXT("VICTORY") : TEXT("DEFEAT")));
    ResultTitle->SetColorAndOpacity(FSlateColor(State == EShooterRoundState::Won ? FLinearColor(0.1f,0.85f,1) : FLinearColor(1,0.22f,0.1f)));
    SetVisibility(State == EShooterRoundState::Playing ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if (GetOwningPlayer() && State != EShooterRoundState::Playing) { SetKeyboardFocus(); }
}
void UShooterMenuWidget::StartClicked()
{
    if (AShooterGameMode* GM = GetShooterGameMode())
    {
        if (GM->GetRoundState() == EShooterRoundState::Paused) { GM->TogglePause(); }
        else { GM->StartRound(); }
    }
}
void UShooterMenuWidget::EndClicked() { if (AShooterGameMode* GM = GetShooterGameMode()) { GM->ReturnToMenu(); } }
void UShooterMenuWidget::RestartClicked() { if (AShooterGameMode* GM = GetShooterGameMode()) { GM->RestartRound(); } }
void UShooterMenuWidget::QuitClicked()
{
    // PIE 中停止预览；独立游戏中正常退出，不强制终止编辑器进程。
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
FReply UShooterMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::P)
    {
        if (AShooterGameMode* GM = GetShooterGameMode()) { GM->TogglePause(); }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, Event);
}
void UShooterMenuWidget::NativeDestruct()
{
    for (UButton* Button : {StartButton.Get(), EndButton.Get(), QuitButton.Get(), RestartButton.Get(), MenuButton.Get(), ResultQuitButton.Get()})
    {
        if (Button) { Button->OnClicked.RemoveAll(this); }
    }
    Super::NativeDestruct();
}
