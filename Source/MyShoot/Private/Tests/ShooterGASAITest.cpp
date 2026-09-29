// 使用实际控制器定时器与 GAS 伤害，覆盖攻击前摇、遮挡、距离、死亡和多敌人独立状态。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "AI/ShooterAIController.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Weapons/ShooterWeaponComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGASAITest, "MyShoot.GAS.AI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterGASAITest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("AI world"), World)) { return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        GFrameCounter = SavedFrame;
    };
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance = [World](float Seconds)
    {
        while (Seconds > KINDA_SMALL_NUMBER)
        {
            const float Step = FMath::Min(Seconds, 0.02f);
            ++GFrameCounter; World->Tick(LEVELTICK_All, Step); Seconds -= Step;
        }
    };
    auto SpawnCharacter = [World](FVector Location)
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AMyShooter* Character = World->SpawnActor<AMyShooter>(Location, FRotator::ZeroRotator, P);
        if (Character) { Character->GetCharacterMovement()->DisableMovement(); }
        return Character;
    };
    AMyShooter* Target = SpawnCharacter(FVector(180,0,0));
    AMyShooter* Enemy = SpawnCharacter(FVector::ZeroVector);
    AShooterAIController* AI = World->SpawnActor<AShooterAIController>();
    if (!TestNotNull(TEXT("Target"), Target) || !TestNotNull(TEXT("Enemy"), Enemy) || !TestNotNull(TEXT("AI"), AI)) { return false; }
    AI->Possess(Enemy);
    // Possess 会 Restart Pawn 并恢复移动模式；测试空世界无地面，接管后再禁用重力。
    Enemy->GetCharacterMovement()->DisableMovement();
    AI->SetCombatTarget(Target);
    Advance(0.24f);
    TestEqual(TEXT("Valid target enters attack windup"), AI->GetCombatState(), EShooterAIState::Attacking);
    TestEqual(TEXT("Windup does not damage immediately"), Target->GetGASHealth(), 100.0f);
    Advance(0.34f);
    TestEqual(TEXT("One completed attack deals ten GAS damage"), Target->GetGASHealth(), 90.0f);
    Advance(0.3f);
    TestEqual(TEXT("Decision ticks do not bypass cooldown"), Target->GetGASHealth(), 90.0f);

    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(20,200,200));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore);
    Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Box->RegisterComponent();
    Wall->SetActorLocation(FVector(90,0,0));
    Advance(1.5f);
    TestEqual(TEXT("Wall blocks attacks"), Target->GetGASHealth(), 90.0f);
    Wall->Destroy();
    Advance(0.22f);
    Target->SetActorLocation(FVector(1800,0,0));
    Advance(0.5f);
    TestEqual(TEXT("Leaving range during windup avoids damage"), Target->GetGASHealth(), 90.0f);
    Target->SetActorLocation(FVector(4000,0,0));
    Advance(0.22f);
    TestNull(TEXT("Target beyond lose range cleared"), AI->GetCombatTarget());

    // 重新进入有效范围后杀死攻击者，前摇定时器不得留下延迟伤害。
    Target->SetActorLocation(FVector(180,0,0));
    AI->SetCombatTarget(Target);
    Advance(1.5f);
    const float BeforeDeath = Target->GetGASHealth();
    AActor* Source = World->SpawnActor<AActor>();
    UShooterDamageLibrary::ApplyGASDamage(Source, Enemy, 100, Source, FHitResult());
    Advance(1.5f);
    TestEqual(TEXT("Dead enemy stops all future attacks"), Target->GetGASHealth(), BeforeDeath);
    TestEqual(TEXT("AI death is terminal"), AI->GetCombatState(), EShooterAIState::Dead);
    TestNull(TEXT("Dead enemy drops target"), AI->GetCombatTarget());

    AMyShooter* EnemyA = SpawnCharacter(FVector(0,1000,0));
    AMyShooter* EnemyB = SpawnCharacter(FVector(0,2000,0));
    AMyShooter* TargetA = SpawnCharacter(FVector(180,1000,0));
    AMyShooter* TargetB = SpawnCharacter(FVector(180,2000,0));
    AShooterAIController* A = World->SpawnActor<AShooterAIController>();
    AShooterAIController* B = World->SpawnActor<AShooterAIController>();
    if (!EnemyA || !EnemyB || !TargetA || !TargetB || !A || !B) { AddError(TEXT("Multi-agent fixture failed.")); return false; }
    A->Possess(EnemyA); B->Possess(EnemyB);
    EnemyA->GetCharacterMovement()->DisableMovement(); EnemyB->GetCharacterMovement()->DisableMovement();
    A->SetCombatTarget(TargetA); B->SetCombatTarget(TargetB);
    Advance(0.6f);
    TestEqual(TEXT("Enemy A has independent damage"), TargetA->GetGASHealth(), 90.0f);
    TestEqual(TEXT("Enemy B has independent damage"), TargetB->GetGASHealth(), 90.0f);
    UShooterDamageLibrary::ApplyGASDamage(Source, TargetA, 100, Source, FHitResult());
    TestNull(TEXT("Target death clears immediately"), A->GetCombatTarget());
    TestEqual(TEXT("Other enemy keeps its target"), B->GetCombatTarget(), static_cast<AShooterCharacterBase*>(TargetB));
    TargetB->Destroy();
    TestNull(TEXT("Target destruction clears immediately"), B->GetCombatTarget());
    Advance(0.5f);
    // 真正由 AI 的攻击结算杀死正在换弹的玩家，串起 T05/T06 的中断链路。
    AMyShooter* ReloadPlayer = SpawnCharacter(FVector(180,3000,0));
    AMyShooter* Attacker = SpawnCharacter(FVector(0,3000,0));
    AShooterAIController* C = World->SpawnActor<AShooterAIController>();
    if (!ReloadPlayer || !Attacker || !C) { AddError(TEXT("Reload/death fixture failed")); return false; }
    UShooterDamageLibrary::ApplyGASDamage(Source, ReloadPlayer, 90, Source, FHitResult());
    UShooterWeaponComponent* Weapon = ReloadPlayer->GetShooterWeapon();
    Weapon->StartFiring(); Weapon->StopFiring();
    TestTrue(TEXT("Player starts reload before attack"), Weapon->StartReloading());
    C->Possess(Attacker);
    Attacker->GetCharacterMovement()->DisableMovement();
    C->SetCombatTarget(ReloadPlayer);
    Advance(0.6f);
    TestTrue(TEXT("AI damage kills player"), ReloadPlayer->HasGASDeathStarted());
    TestFalse(TEXT("AI kill cancels reload"), Weapon->IsReloading());
    Advance(1.6f);
    TestEqual(TEXT("AI kill leaves no delayed refill"), Weapon->GetCurrentAmmo(), 29);
    TestEqual(TEXT("Cancelled reload keeps reserve"), Weapon->GetReserveAmmo(), 90);
    return true;
}
#endif
