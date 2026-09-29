#include "Tests/ShooterPackagedSmokeSubsystem.h"
#include "Game/ShooterGameMode.h"
#include "Player/ShooterPlayerController.h"
#include "Characters/MyShooter.h"
#include "AI/ShooterAIController.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Combat/ShooterDamageLibrary.h"
#include "UI/ShooterMenuWidget.h"
#include "UI/ShooterAmmoWidget.h"
#include "UI/ShooterHealthWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterSmoke, Log, All);

void UShooterPackagedSmokeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    // 普通运行完全不注册 Tick、不改游戏状态；仅主动指定命令行参数的打包验证运行。
    if (FParse::Param(FCommandLine::Get(), TEXT("ShooterSmoke")))
    {
        OutputDirectory = FPaths::ProjectSavedDir()/TEXT("T08Smoke");
        FParse::Value(FCommandLine::Get(),TEXT("ShooterSmokeOutput="),OutputDirectory);
        IFileManager::Get().MakeDirectory(*OutputDirectory,true);
        Deadline=FPlatformTime::Seconds()+300;
        TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UShooterPackagedSmokeSubsystem::Step),0.1f);
        UE_LOG(LogShooterSmoke,Display,TEXT("Packaged smoke started; output: %s"),*OutputDirectory);
    }
#endif
}
void UShooterPackagedSmokeSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    Super::Deinitialize();
}
bool UShooterPackagedSmokeSubsystem::Check(bool bOK,const FString& Message)
{
    if (!bOK) { Finish(false,Message); return false; }
    Results.Add(TEXT("PASS: ")+Message); UE_LOG(LogShooterSmoke,Display,TEXT("PASS: %s"),*Message); return true;
}
void UShooterPackagedSmokeSubsystem::Finish(bool bSucceeded,const FString& Message)
{
    if (bFinished) { return; } bFinished=true;
    const FString Report=(bSucceeded ? TEXT("PASS\n") : TEXT("FAIL\n"))+Message+TEXT("\n")+FString::Join(Results,TEXT("\n"));
    FFileHelper::SaveStringToFile(Report,*(OutputDirectory/TEXT("PackagedSmoke.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (bSucceeded) { UE_LOG(LogShooterSmoke,Display,TEXT("PASS packaged smoke: %s"),*Message); }
    else { UE_LOG(LogShooterSmoke,Error,TEXT("FAIL packaged smoke: %s"),*Message); }
    FPlatformMisc::RequestExitWithStatus(false,bSucceeded ? 0 : 1);
}
void UShooterPackagedSmokeSubsystem::Next(int32 NewPhase,float Delay)
{
    Phase=NewPhase; NextTime=FPlatformTime::Seconds()+Delay;
}
void UShooterPackagedSmokeSubsystem::Capture(const TCHAR* Name)
{
    FScreenshotRequest::RequestScreenshot(OutputDirectory/Name,true,false);
}
bool UShooterPackagedSmokeSubsystem::Step(float DeltaSeconds)
{
    if (bFinished) { return false; }
    const double Now=FPlatformTime::Seconds();
    if (Now>Deadline) { Finish(false,FString::Printf(TEXT("Timeout at phase %d"),Phase)); return false; }
    if (Now<NextTime) { return true; }
    UWorld* World=GetWorld();
    AShooterGameMode* GM=World ? Cast<AShooterGameMode>(World->GetAuthGameMode()) : nullptr;
    AShooterPlayerController* PC=World ? Cast<AShooterPlayerController>(World->GetFirstPlayerController()) : nullptr;
    AMyShooter* Player=PC ? Cast<AMyShooter>(PC->GetPawn()) : nullptr;
    if (!World || World->bIsTearingDown || !World->HasBegunPlay() || !GM || !PC || !PC->GetMenuWidget()) { return true; }
    auto Click=[&](const TCHAR* Name)
    {
        UButton* Button=Cast<UButton>(PC->GetMenuWidget()->WidgetTree->FindWidget(FName(Name)));
        if (!Check(Button && Button->GetIsEnabled(),FString(TEXT("Button enabled: "))+Name)) { return false; }
        Button->OnClicked.Broadcast(); return true;
    };
    auto FreshPlayer=[&]()
    {
        return Check(Player && Player->IsGASInitialized() && Player->GetGASHealth()==100
            && Player->GetShooterWeapon()->GetCurrentAmmo()==30 && Player->GetShooterWeapon()->GetReserveAmmo()==90,
            TEXT("Fresh player has 100 health, 30 magazine and 90 reserve"));
    };
    auto UniqueHUD=[&]()
    {
        TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Widgets,UUserWidget::StaticClass(),true);
        int32 Health=0,Ammo=0,Menu=0;
        for (UUserWidget* W : Widgets) { Health+=W->IsA<UShooterHealthWidget>(); Ammo+=W->IsA<UShooterAmmoWidget>(); Menu+=W->IsA<UShooterMenuWidget>(); }
        return Check(Health==1 && Ammo==1 && Menu==1,TEXT("Exactly one health, ammo and menu widget after world load"));
    };
    switch (Phase)
    {
    case 0:
        if (!Player || !Player->IsGASInitialized() || !PC->GetAmmoWidget()) { return true; }
        if (!Check(GM->GetRoundState()==EShooterRoundState::Menu && GM->GetRemainingEnemies()>0,TEXT("Cooked default map starts at menu with registered enemies"))
            || !FreshPlayer() || !UniqueHUD()
            || !Check(!Player->GetShooterWeapon()->StartFiring(),TEXT("Menu blocks firing"))) { return false; }
        Capture(TEXT("01-StartMenu.png")); Next(1,1.0f); break;
    case 1:
        if (!Click(TEXT("StartButton")) || !Check(GM->GetRoundState()==EShooterRoundState::Playing,TEXT("Actual start button enters gameplay"))) { return false; }
        for (TActorIterator<AShooterCharacterBase> It(World); It; ++It)
        {
            if (*It!=Player && Cast<AShooterAIController>(It->GetController())) { TargetEnemy=*It; break; }
        }
        if (!Check(TargetEnemy.IsValid(),TEXT("Cooked enemy owns native AI controller"))) { return false; }
        Player->SetActorLocation(TargetEnemy->GetActorLocation()+FVector(180,0,0),false);
        Player->GetCharacterMovement()->DisableMovement();
        Next(2,1.5f); break;
    case 2:
        if (!Check(Player && Player->GetGASHealth()<100 && Player->GetGASHealth()>0,TEXT("Actual AI attack damages living player"))
            || !Check(PC->GetHealthWidget()->GetDisplayedHealth()==Player->GetGASHealth(),TEXT("Cooked health widget follows GAS damage"))) { return false; }
        Player->SetActorLocation(TargetEnemy->GetActorLocation()+FVector(500,0,0),false);
        // 保持测试靶位置稳定；仍使用真实武器射线、GAS 和蓝图资源。
        TargetEnemy->GetCharacterMovement()->DisableMovement();
        Next(3); break;
    case 3:
        {
            FVector View; FRotator Rotation; PC->GetPlayerViewPoint(View,Rotation);
            const FVector Aim=TargetEnemy->GetActorLocation()+FVector(0,0,35);
            PC->SetControlRotation((Aim-View).Rotation());
            Player->SetActorRotation(FRotator(0,(Aim-View).Rotation().Yaw,0));
            EnemyHealthBeforeShot=TargetEnemy->GetGASHealth();
            Next(4);
        } break;
    case 4:
        {
            UShooterWeaponComponent* Weapon=Player->GetShooterWeapon();
            const bool bAccepted=Weapon->StartFiring(); Weapon->StopFiring();
            int32 Bullets=0; for (TActorIterator<AShooterBulletVisual> It(World); It; ++It) { ++Bullets; }
            if (!Check(bAccepted && Weapon->GetCurrentAmmo()==29 && PC->GetAmmoWidget()->GetDisplayedAmmo()==29,TEXT("Real shot consumes one round and updates HUD"))
                || !Check(FMath::IsNearlyEqual(TargetEnemy->GetGASHealth(),EnemyHealthBeforeShot-25),TEXT("Real packaged hitscan deals exactly 25 damage"))
                || !Check(Bullets==1,TEXT("Real shot spawns one cosmetic bullet"))) { return false; }
            Capture(TEXT("02-Gameplay.png"));
            if (!Check(Weapon->StartReloading(),TEXT("Reload accepted"))) { return false; }
            Next(5,1.8f);
        } break;
    case 5:
        if (!Check(Player->GetShooterWeapon()->GetCurrentAmmo()==30 && Player->GetShooterWeapon()->GetReserveAmmo()==89
            && PC->GetAmmoWidget()->GetDisplayedReserve()==89,TEXT("Cooked reload and HUD transfer ammo correctly"))) { return false; }
        GM->TogglePause();
        if (!Check(GM->GetRoundState()==EShooterRoundState::Paused && !Player->GetShooterWeapon()->StartFiring(),TEXT("Pause blocks combat"))
            || !Click(TEXT("StartButton")) || !Check(GM->GetRoundState()==EShooterRoundState::Playing,TEXT("Actual continue button resumes gameplay"))) { return false; }
        for (TActorIterator<AShooterCharacterBase> It(World); It; ++It)
        {
            if (*It!=Player && !It->HasGASDeathStarted()) { UShooterDamageLibrary::ApplyGASDamage(Player,*It,100000,Player,FHitResult()); }
        }
        Next(6); break;
    case 6:
        if (!Check(GM->GetRoundState()==EShooterRoundState::Won,TEXT("Eliminating registered enemies shows victory"))) { return false; }
        Capture(TEXT("03-Victory.png")); Next(11,0.6f); break;
    case 11:
        PreviousWorld=World;
        if (!Click(TEXT("RestartButton"))) { return false; }
        Next(7,1.0f); break;
    case 7:
        if (World==PreviousWorld.Get() || !Player || !Player->IsGASInitialized()) { return true; }
        if (!Check(GM->GetRoundState()==EShooterRoundState::Playing,TEXT("Restart loads a new world and starts gameplay")) || !FreshPlayer() || !UniqueHUD()) { return false; }
        ++RestartCount;
        Player->GetShooterWeapon()->StartFiring(); Player->GetShooterWeapon()->StopFiring(); Player->GetShooterWeapon()->StartReloading();
        {
            AActor* Hazard=World->SpawnActor<AActor>();
            UShooterDamageLibrary::ApplyGASDamage(Hazard,Player,100000,Hazard,FHitResult());
        }
        if (!Check(!Player->GetShooterWeapon()->IsReloading(),TEXT("Lethal damage cancels reload"))) { return false; }
        Next(8); break;
    case 8:
        if (!Check(GM->GetRoundState()==EShooterRoundState::Lost,TEXT("Player death shows defeat"))) { return false; }
        Capture(TEXT("04-Defeat.png")); Next(12,0.6f); break;
    case 12:
        PreviousWorld=World;
        if (RestartCount<3) { if (!Click(TEXT("RestartButton"))) { return false; } Next(7,1.0f); }
        else { if (!Click(TEXT("MenuButton"))) { return false; } Next(9,1.0f); }
        break;
    case 9:
        if (World==PreviousWorld.Get() || !Player || !Player->IsGASInitialized()) { return true; }
        if (!Check(GM->GetRoundState()==EShooterRoundState::Menu,TEXT("Result Main Menu reloads menu")) || !FreshPlayer() || !UniqueHUD()
            || !Click(TEXT("StartButton"))) { return false; }
        GM->TogglePause(); PreviousWorld=World;
        if (!Click(TEXT("EndButton"))) { return false; }
        Next(10,1.0f); break;
    case 10:
        if (World==PreviousWorld.Get() || !Player || !Player->IsGASInitialized()) { return true; }
        if (!Check(GM->GetRoundState()==EShooterRoundState::Menu,TEXT("Pause End Game returns to menu")) || !FreshPlayer() || !UniqueHUD()) { return false; }
        Finish(true,TEXT("Cooked startup, AI damage, hitscan, bullet, HUD, reload, pause, win/loss, three actual restarts and both return-to-menu paths passed."));
        return false;
    }
    return true;
}
