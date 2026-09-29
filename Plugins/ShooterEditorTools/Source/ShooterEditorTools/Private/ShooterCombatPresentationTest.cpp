// 使用保存后的敌人蓝图和真实 AnimInstance，验证开火表现与 GAS 结算和中断的关系。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/ShooterAIController.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterCombatPresentationTest, "MyShoot.Presentation.AIAttack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterCombatPresentationTest::RunTest(const FString& Parameters)
{
    UClass* EnemyClass = LoadClass<AShooterCharacterBase>(nullptr, TEXT("/Game/Blueprints/Boot_Shooter_BP.Boot_Shooter_BP_C"));
    UClass* ControllerClass = LoadClass<AShooterAIController>(nullptr, TEXT("/Game/Blueprints/Boot_Shooter_controller.Boot_Shooter_controller_C"));
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animations/AM_AI_RifleFire.AM_AI_RifleFire"));
    if (!TestNotNull(TEXT("Saved enemy"), EnemyClass) || !TestNotNull(TEXT("Saved AI"), ControllerClass)
        || !TestNotNull(TEXT("Saved attack montage"), Montage)) { return false; }
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter = SavedFrame;
    };
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance = [World](float Seconds)
    {
        while (Seconds > KINDA_SMALL_NUMBER)
        {
            const float Step = FMath::Min(Seconds, 0.01f); ++GFrameCounter;
            World->Tick(LEVELTICK_All, Step); Seconds -= Step;
        }
    };
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AShooterCharacterBase* Enemy = World->SpawnActor<AShooterCharacterBase>(EnemyClass, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    AMyShooter* Target = World->SpawnActor<AMyShooter>(FVector(180,0,0), FRotator::ZeroRotator, Spawn);
    AShooterAIController* AI = Cast<AShooterAIController>(Enemy->GetController());
    if (!AI) { AI = World->SpawnActor<AShooterAIController>(ControllerClass); AI->Possess(Enemy); }
    Enemy->GetCharacterMovement()->DisableMovement(); Target->GetCharacterMovement()->DisableMovement();
    Enemy->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    UAnimInstance* Anim = Enemy->GetMesh()->GetAnimInstance();
    if (!TestNotNull(TEXT("Real enemy animation instance"), Anim)) { return false; }
    AI->SetCombatTarget(Target);
    Advance(0.24f);
    TestFalse(TEXT("Windup has not fired animation"), Anim->Montage_IsPlaying(Montage));
    TestEqual(TEXT("Windup does not deal damage"), Target->GetGASHealth(), 100.f);
    // TimerManager 在帧边界推进，等待真实伤害发生而不是假定恰好第 0.52 秒结算。
    for (int32 Step = 0; Step < 100 && Target->GetGASHealth() == 100.f; ++Step) { Advance(0.01f); }
    TestEqual(TEXT("One attack still deals ten damage"), Target->GetGASHealth(), 90.f);
    TestTrue(TEXT("Successful attack plays saved montage on real AnimInstance"), Anim->Montage_IsPlaying(Montage));
    AI->SuspendCombat(); Advance(0.1f);
    TestFalse(TEXT("Suspend stops only attack montage"), Anim->Montage_IsPlaying(Montage));
    AI->SetCombatTarget(Target);
    Advance(1.35f);
    // 明确进入下一轮前摇再离开攻击范围，动画和伤害均不触发。
    AI->SuspendCombat(); Advance(0.1f); AI->SetCombatTarget(Target);
    const float Before = Target->GetGASHealth();
    Target->SetActorLocation(FVector(1800,0,0)); Advance(1.4f);
    TestEqual(TEXT("Out of range has no delayed damage"), Target->GetGASHealth(), Before);
    TestFalse(TEXT("Out of range has no firing animation"), Anim->Montage_IsPlaying(Montage));
    Target->SetActorLocation(FVector(180,0,0)); AI->SetCombatTarget(Target);
    // 等待实际攻击动画出现，再在动画中杀死敌人。
    bool bPlayed = false;
    for (int32 Step = 0; Step < 200 && !bPlayed; ++Step) { Advance(0.01f); bPlayed = Anim->Montage_IsPlaying(Montage); }
    TestTrue(TEXT("Enemy attacks again after returning"), bPlayed);
    UShooterDamageLibrary::ApplyGASDamage(Target, Enemy, 1000, Target, FHitResult());
    Advance(0.12f);
    TestTrue(TEXT("Enemy death retained"), Enemy->HasGASDeathStarted());
    TestFalse(TEXT("Death stops attack montage before death presentation"), Anim->Montage_IsPlaying(Montage));
    const float AfterDeath = Target->GetGASHealth(); Advance(1.5f);
    TestEqual(TEXT("Dead AI cannot damage later"), Target->GetGASHealth(), AfterDeath);
    return true;
}
#endif
