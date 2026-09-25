// 自动验证：属性初始化、数值约束及控制器切换后保持血量。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Characters/MyShooter.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGASFoundationTest,
    "MyShoot.GAS.Foundation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterGASFoundationTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Create isolated test world"), World))
    {
        return false;
    }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    // 隔离测试世界没有 GameMode，手动派发开始通知以触发角色 BeginPlay。
    World->GetWorldSettings()->NotifyBeginPlay();
    TestTrue(TEXT("Test world has started play"), World->HasBegunPlay());

    AMyShooter* Character = World->SpawnActorDeferred<AMyShooter>(
        AMyShooter::StaticClass(), FTransform::Identity, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Spawn player subclass"), Character))
    {
        return false;
    }

    // 在 BeginPlay 前修改初始配置，模拟蓝图类默认值。
    FFloatProperty* InitialHealth = FindFProperty<FFloatProperty>(
        AShooterCharacterBase::StaticClass(), TEXT("InitialMaxHealth"));
    if (!TestNotNull(TEXT("Designer health configuration exists"), InitialHealth))
    {
        return false;
    }
    InitialHealth->SetPropertyValue_InContainer(Character, 175.0f);
    Character->FinishSpawning(FTransform::Identity);
    TestTrue(TEXT("Spawn dispatches native BeginPlay"), Character->HasActorBegunPlay());

    UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
    if (!TestNotNull(TEXT("Character exposes ASC"), ASC))
    {
        return false;
    }
    TestTrue(TEXT("GAS ready after BeginPlay"), Character->IsGASInitialized());
    TestTrue(TEXT("AttributeSet registered with ASC"), ASC->GetSet<UShooterAttributeSet>() != nullptr);
    TestTrue(TEXT("ASC owner is character"), ASC->GetOwnerActor() == Character);
    TestTrue(TEXT("ASC avatar is character"), ASC->GetAvatarActor() == Character);
    TestEqual(TEXT("Initial effect sets configured MaxHealth"), Character->GetGASMaxHealth(), 175.0f);
    TestEqual(TEXT("Initial effect starts at full health"), Character->GetGASHealth(), 175.0f);

    // 临时测试效果用于验证 GAS 属性机制；生产伤害入口由独立测试覆盖。
    auto ApplyHealthDelta = [ASC](float Delta)
    {
        UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
        Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
        FGameplayModifierInfo Modifier;
        Modifier.Attribute = UShooterAttributeSet::GetHealthAttribute();
        Modifier.ModifierOp = EGameplayModOp::Additive;
        Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Delta));
        Effect->Modifiers.Add(Modifier);
        ASC->ApplyGameplayEffectToSelf(Effect, 1.0f, ASC->MakeEffectContext());
    };

    ApplyHealthDelta(-25.0f);
    TestEqual(TEXT("Effects use registered GAS attributes"), Character->GetGASHealth(), 150.0f);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APlayerController* Controller = World->SpawnActor<APlayerController>(SpawnParams);
    if (!TestNotNull(TEXT("Spawn controller"), Controller))
    {
        return false;
    }
    Controller->Possess(Character);
    TestEqual(TEXT("Possession does not reapply startup health"), Character->GetGASHealth(), 150.0f);
    Controller->UnPossess();
    Controller->Possess(Character);
    TestEqual(TEXT("Repeated possession preserves health"), Character->GetGASHealth(), 150.0f);

    ApplyHealthDelta(500.0f);
    TestEqual(TEXT("Living health cannot exceed MaxHealth"), Character->GetGASHealth(), 175.0f);
    ApplyHealthDelta(-500.0f);
    TestEqual(TEXT("Health cannot fall below zero"), Character->GetGASHealth(), 0.0f);
    TestTrue(TEXT("Lethal effect enters death"), Character->HasGASDeathStarted());
    ApplyHealthDelta(500.0f);
    TestEqual(TEXT("Healing does not resurrect a dead character"), Character->GetGASHealth(), 0.0f);

    Character->Destroy();
    TestFalse(TEXT("EndPlay clears GAS readiness"), Character->IsGASInitialized());
    TestTrue(TEXT("EndPlay clears ASC avatar"), ASC->GetAvatarActor() == nullptr);

    AShooterCharacterBase* FreshCharacter = World->SpawnActor<AShooterCharacterBase>(SpawnParams);
    if (!TestNotNull(TEXT("Spawn fresh base character"), FreshCharacter))
    {
        return false;
    }
    TestTrue(TEXT("Fresh instance initializes independently"), FreshCharacter->IsGASInitialized());
    TestEqual(TEXT("Fresh instance defaults to full health"), FreshCharacter->GetGASHealth(), 100.0f);

    AShooterCharacterBase* InvalidConfigCharacter = World->SpawnActorDeferred<AShooterCharacterBase>(
        AShooterCharacterBase::StaticClass(), FTransform::Identity, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Spawn invalid configuration case"), InvalidConfigCharacter))
    {
        return false;
    }
    InitialHealth->SetPropertyValue_InContainer(InvalidConfigCharacter, 0.0f);
    InvalidConfigCharacter->FinishSpawning(FTransform::Identity);
    TestTrue(TEXT("Invalid starting max is safely initialized"), InvalidConfigCharacter->IsGASInitialized());
    TestEqual(TEXT("Starting max is clamped positive"), InvalidConfigCharacter->GetGASMaxHealth(), 1.0f);
    TestEqual(TEXT("Clamped configuration still starts full"), InvalidConfigCharacter->GetGASHealth(), 1.0f);
    return true;
}

#endif
