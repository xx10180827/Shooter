#include "Player/ShooterPlayerController.h"
#include "UI/ShooterHealthWidget.h"
#include "Characters/ShooterCharacterBase.h"

void AShooterPlayerController::BeginPlay()
{
    Super::BeginPlay();
    RefreshHUD();
}
void AShooterPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (HasActorBegunPlay()) { RefreshHUD(); }
}
void AShooterPlayerController::RefreshHUD()
{
    // 只给本地玩家创建屏幕控件；不创建角色上方的 WidgetComponent。
    if (!IsLocalController() || !GetLocalPlayer()) { return; }
    if (!HealthWidget && HealthWidgetClass)
    {
        HealthWidget = CreateWidget<UShooterHealthWidget>(this, HealthWidgetClass);
        if (HealthWidget) { HealthWidget->AddToPlayerScreen(10); }
    }
    if (HealthWidget) { HealthWidget->ObserveCharacter(Cast<AShooterCharacterBase>(GetPawn())); }
}
void AShooterPlayerController::OnUnPossess()
{
    if (HealthWidget) { HealthWidget->ObserveCharacter(nullptr); }
    Super::OnUnPossess();
}
void AShooterPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (HealthWidget)
    {
        HealthWidget->ObserveCharacter(nullptr);
        HealthWidget->RemoveFromParent();
        HealthWidget = nullptr;
    }
    Super::EndPlay(Reason);
}
