// 对局规则测试：开始门禁、胜负唯一性、同帧死亡失败优先、暂停及重复世界生命周期。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/ShooterGameMode.h"
#include "AI/ShooterAIController.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "UI/ShooterAmmoWidget.h"
#include "Weapons/ShooterBulletVisual.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterRoundTest, "MyShoot.GAS.Round",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShooterRoundTest::RunTest(const FString& Parameters)
{
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT { GFrameCounter = SavedFrame; };
    for (int32 Scenario = 0; Scenario < 4; ++Scenario)
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        if (!World) { return false; }
        FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
        UGameInstance* GI = NewObject<UGameInstance>(GEngine); World->SetGameInstance(GI); Context.OwningGameInstance=GI;
        ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
        World->GetWorldSettings()->DefaultGameMode=AShooterGameMode::StaticClass();
        if (!TestTrue(TEXT("GameMode creation"), World->SetGameMode(FURL()))) { return false; }
        World->InitializeActorsForPlay(FURL());
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AMyShooter* Player=World->SpawnActor<AMyShooter>(FVector(0,0,200),FRotator::ZeroRotator,P);
        AShooterCharacterBase* Enemy=World->SpawnActor<AShooterCharacterBase>(FVector(1000,0,200),FRotator::ZeroRotator,P);
        Enemy->AIControllerClass=AShooterAIController::StaticClass();
        AActor* Hazard = World->SpawnActor<AActor>();
        World->BeginPlay();
        Player->GetCharacterMovement()->DisableMovement(); Enemy->GetCharacterMovement()->DisableMovement();
        AShooterGameMode* GM=CastChecked<AShooterGameMode>(World->GetAuthGameMode());
        auto Advance=[World](float Seconds)
        {
            while (Seconds>KINDA_SMALL_NUMBER) { const float Step=FMath::Min(Seconds,0.02f); ++GFrameCounter; World->Tick(LEVELTICK_All,Step); Seconds-=Step; }
        };
        Advance(0.02f);
        TestEqual(TEXT("Fresh world starts at menu"), GM->GetRoundState(), EShooterRoundState::Menu);
        TestEqual(TEXT("Enemy registered once"), GM->GetRemainingEnemies(), 1);
        GM->RegisterCombatant(Enemy); TestEqual(TEXT("Duplicate registration ignored"),GM->GetRemainingEnemies(),1);
        TestFalse(TEXT("Menu blocks firing"),Player->GetShooterWeapon()->StartFiring());
        TestTrue(TEXT("Start accepted"),GM->StartRound()); TestFalse(TEXT("Duplicate start ignored"),GM->StartRound());
        GM->TogglePause();
        TestEqual(TEXT("Pause state"),GM->GetRoundState(),EShooterRoundState::Paused);
        TestFalse(TEXT("Paused firing blocked"),Player->GetShooterWeapon()->StartFiring());
        GM->TogglePause();
        if (Scenario == 0)
        {
            UShooterDamageLibrary::ApplyGASDamage(Hazard,Enemy,1000,Hazard,FHitResult());
            TestEqual(TEXT("Outcome deferred until end of frame"),GM->GetRoundState(),EShooterRoundState::Playing);
            Advance(0.02f);
            TestEqual(TEXT("Last enemy death wins"),GM->GetRoundState(),EShooterRoundState::Won);
            TestFalse(TEXT("Won stops firing"),Player->GetShooterWeapon()->StartFiring());
            UShooterDamageLibrary::ApplyGASDamage(Hazard,Player,1000,Hazard,FHitResult());
            Advance(0.02f);
            TestEqual(TEXT("Result committed only once"),GM->GetRoundState(),EShooterRoundState::Won);
        }
        else if (Scenario == 1 || Scenario == 2)
        {
            if (Scenario==1) { UShooterDamageLibrary::ApplyGASDamage(Hazard,Enemy,1000,Hazard,FHitResult()); }
            UShooterDamageLibrary::ApplyGASDamage(Hazard,Player,1000,Hazard,FHitResult());
            if (Scenario==2) { UShooterDamageLibrary::ApplyGASDamage(Hazard,Enemy,1000,Hazard,FHitResult()); }
            TestTrue(TEXT("Both characters actually died"), Player->HasGASDeathStarted() && Enemy->HasGASDeathStarted());
            Advance(0.02f);
            TestEqual(TEXT("Same-frame death loses in both callback orders"),GM->GetRoundState(),EShooterRoundState::Lost);
        }
        else
        {
            // 直接删除活敌人不能冒充击杀；不会在错误空名单时自动获胜。
            Enemy->Destroy(); Advance(0.02f);
            TestEqual(TEXT("Removing live enemy is not a kill"),GM->GetRoundState(),EShooterRoundState::Playing);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterAmmoVisualTest, "MyShoot.GAS.AmmoAndBulletVisual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShooterAmmoVisualTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if (!World) { return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 SavedFrame=GFrameCounter;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter=SavedFrame; };
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance=[World](float Seconds) { while(Seconds>KINDA_SMALL_NUMBER) { const float Step=FMath::Min(Seconds,0.02f); ++GFrameCounter; World->Tick(LEVELTICK_All,Step); Seconds-=Step; } };
    Advance(0.02f);
    AMyShooter* Player=World->SpawnActor<AMyShooter>();
    AMyShooter* Other=World->SpawnActor<AMyShooter>(FVector(0,1000,0),FRotator::ZeroRotator);
    if (!Player || !Other) { return false; }
    Player->GetCharacterMovement()->DisableMovement(); Other->GetCharacterMovement()->DisableMovement();
    UShooterWeaponComponent* Weapon=Player->GetShooterWeapon();
    UShooterAmmoWidget* Widget=NewObject<UShooterAmmoWidget>(World); Widget->ObserveWeapon(Weapon);
    TestEqual(TEXT("Initial magazine"),Widget->GetDisplayedAmmo(),30);
    TestEqual(TEXT("Initial reserve"),Widget->GetDisplayedReserve(),90);
    Weapon->StartFiring(); Weapon->StopFiring();
    TestEqual(TEXT("Shot updates HUD once"),Widget->GetDisplayedAmmo(),29);
    Weapon->StartReloading(); TestTrue(TEXT("Reload status visible"),Widget->GetDisplayedReloading());
    Advance(1.6f);
    TestEqual(TEXT("Reload transfers magazine"),Widget->GetDisplayedAmmo(),30);
    TestEqual(TEXT("Reload transfers reserve"),Widget->GetDisplayedReserve(),89);
    TestFalse(TEXT("Reload status clears"),Widget->GetDisplayedReloading());
    Widget->ObserveWeapon(Other->GetShooterWeapon());
    Weapon->StartFiring(); Weapon->StopFiring();
    TestEqual(TEXT("Old weapon no longer changes widget"),Widget->GetDisplayedAmmo(),30);
    Widget->ObserveWeapon(nullptr); TestEqual(TEXT("Detach clears values"),Widget->GetDisplayedAmmo(),0);
    AShooterBulletVisual* Bullet=World->SpawnActor<AShooterBulletVisual>(FVector(0,500,100),FRotator::ZeroRotator);
    Bullet->Launch(FVector(100,500,100),1000);
    Advance(0.04f);
    TestTrue(TEXT("Visible bullet moves"),Bullet->GetActorLocation().X>0 && Bullet->GetActorLocation().X<100);
    Advance(0.1f);
    TestTrue(TEXT("Bullet cleans up at endpoint"),!IsValid(Bullet) || Bullet->IsActorBeingDestroyed());
    TestEqual(TEXT("Cosmetic bullet never applies damage"),Other->GetGASHealth(),100.0f);
    return true;
}
#endif
