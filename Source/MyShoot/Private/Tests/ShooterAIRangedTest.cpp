// 真实控制器自动选敌：覆盖背后不发现、10 米停步射击、距离和遮挡中断、最后位置记忆。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "AI/ShooterAIController.h"
#include "Characters/MyShooter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterAIRangedTest, "MyShoot.Gameplay.AIRangedSight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterAIRangedTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter = SavedFrame;
    };
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance = [World](float Seconds)
    {
        while (Seconds > KINDA_SMALL_NUMBER)
        {
            const float Step = FMath::Min(Seconds, 0.01f);
            ++GFrameCounter; World->Tick(LEVELTICK_All, Step); Seconds -= Step;
        }
    };
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AMyShooter* Enemy = World->SpawnActor<AMyShooter>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    AMyShooter* Player = World->SpawnActor<AMyShooter>(FVector(-1000,0,0), FRotator::ZeroRotator, Spawn);
    AShooterAIController* AI = World->SpawnActor<AShooterAIController>();
    APlayerController* PC = World->SpawnActor<APlayerController>();
    if (!Enemy || !Player || !AI || !PC) { AddError(TEXT("AI sight fixture failed")); return false; }
    AI->Possess(Enemy); PC->Possess(Player);
    Enemy->GetCharacterMovement()->DisableMovement(); Player->GetCharacterMovement()->DisableMovement();

    // 不手动注入 CombatTarget，验证正常发现流程。
    Advance(0.8f);
    TestNull(TEXT("Player behind enemy is not automatically detected"), AI->GetCombatTarget());
    TestEqual(TEXT("Player behind enemy takes no damage"), Player->GetGASHealth(), 100.f);
    Player->SetActorLocation(FVector(1000,0,0)); Advance(0.24f);
    TestEqual(TEXT("Front player at ten metres is acquired"), AI->GetCombatTarget(), static_cast<AShooterCharacterBase*>(Player));
    TestEqual(TEXT("Ranged attack starts without contact"), AI->GetCombatState(), EShooterAIState::Attacking);
    Advance(0.34f);
    TestEqual(TEXT("Ten metre shot deals one GAS hit"), Player->GetGASHealth(), 90.f);
    TestTrue(TEXT("AI remains at firing position"), Enemy->GetActorLocation().Equals(FVector::ZeroVector, 0.1f));

    Player->SetActorLocation(FVector(1800,0,0)); Advance(0.3f);
    TestEqual(TEXT("Visible target outside firing range triggers chase"), AI->GetCombatState(), EShooterAIState::Chasing);
    const float BeforeOutOfRange = Player->GetGASHealth(); Advance(0.8f);
    TestEqual(TEXT("No damage beyond attack range"), Player->GetGASHealth(), BeforeOutOfRange);

    Player->SetActorLocation(FVector(1000,0,0)); Advance(0.24f);
    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(25,1000,300));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Box->RegisterComponent(); Wall->SetActorLocation(FVector(500,0,0));
    const float BeforeCover = Player->GetGASHealth();
    Player->SetActorLocation(FVector(1200,400,0)); Advance(0.3f);
    TestEqual(TEXT("Cover changes AI to last-seen search"), AI->GetCombatState(), EShooterAIState::Searching);
    TestTrue(TEXT("Search focuses last seen position, not hidden player"), AI->GetFocalPoint().Equals(FVector(1000,0,0), 1.f));
    Advance(1.f);
    TestEqual(TEXT("Wall cancels pending and future damage"), Player->GetGASHealth(), BeforeCover);
    Advance(2.f);
    TestNull(TEXT("Expired sight memory releases target"), AI->GetCombatTarget());
    TestEqual(TEXT("Expired search returns to idle"), AI->GetCombatState(), EShooterAIState::Idle);
    Wall->Destroy(); Player->SetActorLocation(FVector(1000,0,0)); Advance(0.8f);
    TestEqual(TEXT("Player can be reacquired after leaving cover"), AI->GetCombatTarget(), static_cast<AShooterCharacterBase*>(Player));
    TestTrue(TEXT("Reacquired target can be shot again"), Player->GetGASHealth() < BeforeCover);
    return true;
}
#endif