// 自动验证：死亡状态只触发一次、活动停止、能力取消和尸体兜底清理。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Characters/ShooterCharacterBase.h"
#include "Combat/ShooterDamageLibrary.h"
#include "GAS/Abilities/ShooterGameplayAbility.h"
#include "GAS/ShooterGameplayTags.h"
#include "AbilitySystemComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGASDeathTest, "MyShoot.GAS.Death",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterGASDeathTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Death test world"), World)) { return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 SavedFrameCounter = GFrameCounter;
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        GFrameCounter = SavedFrameCounter;
    };
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->GetWorldSettings()->NotifyBeginPlay();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Source = World->SpawnActor<AActor>(Params);
    AShooterCharacterBase* Victim = World->SpawnActor<AShooterCharacterBase>(Params);
    AShooterCharacterBase* LivingTarget = World->SpawnActor<AShooterCharacterBase>(Params);
    AAIController* AI = World->SpawnActor<AAIController>(Params);
    if (!TestNotNull(TEXT("Source"), Source) || !TestNotNull(TEXT("Victim"), Victim)
        || !TestNotNull(TEXT("Living target"), LivingTarget) || !TestNotNull(TEXT("AI"), AI)) { return false; }
    AI->Possess(Victim);
    Victim->FinishGASDeath();
    TestFalse(TEXT("Finish callback cannot destroy a living actor"), Victim->IsActorBeingDestroyed());
    Victim->GetCharacterMovement()->Velocity = FVector(150.0f, 0.0f, 0.0f);

    FTimerHandle CharacterTimer;
    FTimerDelegate CharacterDelegate;
    CharacterDelegate.BindUObject(Victim, &ACharacter::Jump);
    World->GetTimerManager().SetTimer(CharacterTimer, CharacterDelegate, 10.0f, true);
    FTimerHandle ControllerTimer;
    FTimerDelegate ControllerDelegate;
    ControllerDelegate.BindUObject(AI, &AAIController::StopMovement);
    World->GetTimerManager().SetTimer(ControllerTimer, ControllerDelegate, 10.0f, true);

    UAbilitySystemComponent* ASC = Victim->GetAbilitySystemComponent();
    const FGameplayAbilitySpecHandle Ability = ASC->GiveAbility(FGameplayAbilitySpec(
        UShooterGameplayAbility::StaticClass(), 1));
    TestTrue(TEXT("Project ability can activate while alive"), ASC->TryActivateAbility(Ability));
    TestTrue(TEXT("Ability is active before death"), ASC->FindAbilitySpecFromHandle(Ability)->IsActive());

    int32 DeathTransitions = 0;
    bool bRejectedReentrantDamage = false;
    const FDelegateHandle TagHandle = ASC->RegisterGameplayTagEvent(
        ShooterGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddLambda(
        [&](FGameplayTag, int32 Count)
        {
            if (Count > 0)
            {
                ++DeathTransitions;
                bRejectedReentrantDamage = Victim->HasGASDeathStarted()
                    && !UShooterDamageLibrary::ApplyGASDamage(Source, Victim, 25, nullptr, FHitResult());
            }
        });
    ON_SCOPE_EXIT
    {
        ASC->RegisterGameplayTagEvent(ShooterGameplayTags::State_Dead,
            EGameplayTagEventType::NewOrRemoved).Remove(TagHandle);
    };

    TestTrue(TEXT("Lethal shot applies"), UShooterDamageLibrary::ApplyGASDamage(
        Source, Victim, 150, nullptr, FHitResult()));
    TestTrue(TEXT("Death state entered"), Victim->HasGASDeathStarted());
    TestEqual(TEXT("Dead tag added exactly once"), ASC->GetTagCount(ShooterGameplayTags::State_Dead), 1);
    TestEqual(TEXT("One death transition"), DeathTransitions, 1);
    TestTrue(TEXT("State is set before callbacks can reenter"), bRejectedReentrantDamage);
    TestEqual(TEXT("Movement disabled"), Victim->GetCharacterMovement()->MovementMode, MOVE_None);
    TestTrue(TEXT("Velocity stopped"), Victim->GetVelocity().IsNearlyZero());
    TestFalse(TEXT("Character tick disabled"), Victim->IsActorTickEnabled());
    TestFalse(TEXT("AI tick disabled"), AI->IsActorTickEnabled());
    TestEqual(TEXT("Capsule collision disabled"), Victim->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    TestFalse(TEXT("Character legacy timer removed"), World->GetTimerManager().TimerExists(CharacterTimer));
    TestFalse(TEXT("Controller legacy timer removed"), World->GetTimerManager().TimerExists(ControllerTimer));
    TestFalse(TEXT("Active ability cancelled"), ASC->FindAbilitySpecFromHandle(Ability)->IsActive());
    TestFalse(TEXT("Ability cannot reactivate while dead"), ASC->TryActivateAbility(Ability));
    TestFalse(TEXT("Dead source cannot deal damage"), UShooterDamageLibrary::ApplyGASDamage(
        Victim, LivingTarget, 25, nullptr, FHitResult()));
    const float CleanupRemaining = Victim->GetLifeSpan();
    TestTrue(TEXT("Cleanup fallback armed"), CleanupRemaining > 0.0f);
    TestFalse(TEXT("Repeated corpse hit ignored"), UShooterDamageLibrary::ApplyGASDamage(
        Source, Victim, 25, nullptr, FHitResult()));
    TestEqual(TEXT("Repeated hit cannot extend cleanup"), Victim->GetLifeSpan(), CleanupRemaining);
    TestEqual(TEXT("Still one death transition"), DeathTransitions, 1);
    Victim->FinishGASDeath();
    TestTrue(TEXT("Animation completion destroys victim"), Victim->IsActorBeingDestroyed());
    TestFalse(TEXT("Destroyed victim has no initialized GAS"), Victim->IsGASInitialized());
    Victim->FinishGASDeath();

    AShooterCharacterBase* NoAnimation = World->SpawnActorDeferred<AShooterCharacterBase>(
        AShooterCharacterBase::StaticClass(), FTransform::Identity, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("No-animation actor"), NoAnimation)) { return false; }
    FFloatProperty* Delay = FindFProperty<FFloatProperty>(AShooterCharacterBase::StaticClass(), TEXT("DeathCleanupDelay"));
    if (!TestNotNull(TEXT("Cleanup delay property"), Delay)) { return false; }
    Delay->SetPropertyValue_InContainer(NoAnimation, 0.0f);
    NoAnimation->FinishSpawning(FTransform::Identity);
    TestTrue(TEXT("No-animation lethal hit"), UShooterDamageLibrary::ApplyGASDamage(
        Source, NoAnimation, 150, nullptr, FHitResult()));
    TestTrue(TEXT("Zero timeout config is clamped to a positive fallback"), NoAnimation->GetLifeSpan() > 0.0f);
    ++GFrameCounter;
    World->GetTimerManager().Tick(0.0f);
    ++GFrameCounter;
    World->GetTimerManager().Tick(0.25f);
    TestTrue(TEXT("Fallback destroys actor without animation callback"), NoAnimation->IsActorBeingDestroyed());

    AShooterCharacterBase* Fresh = World->SpawnActor<AShooterCharacterBase>(Params);
    if (!TestNotNull(TEXT("Fresh actor"), Fresh)) { return false; }
    TestFalse(TEXT("Fresh actor is alive"), Fresh->HasGASDeathStarted());
    TestFalse(TEXT("Fresh actor has no dead tag"), Fresh->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShooterGameplayTags::State_Dead));
    TestEqual(TEXT("Fresh actor starts full"), Fresh->GetGASHealth(), 100.0f);
    return true;
}
#endif
