// 自动验证：GAS 伤害、来源上下文、非法输入与重复击杀。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Characters/MyShooter.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "Combat/ShooterDamageLibrary.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGASDamageTest, "MyShoot.GAS.Damage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterGASDamageTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Test world"), World)) { return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->GetWorldSettings()->NotifyBeginPlay();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Source = World->SpawnActor<AActor>(Params);
    AActor* Weapon = World->SpawnActor<AActor>(Params);
    AActor* Wall = World->SpawnActor<AActor>(Params);
    AMyShooter* Target = World->SpawnActor<AMyShooter>(Params);
    if (!TestNotNull(TEXT("Target"), Target) || !TestNotNull(TEXT("Source"), Source)
        || !TestNotNull(TEXT("Weapon"), Weapon) || !TestNotNull(TEXT("Wall"), Wall)) { return false; }

    FHitResult Hit(Target, nullptr, FVector(10, 20, 30), FVector::UpVector);
    Hit.bBlockingHit = true;
    UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
    int32 HealthEvents = 0;
    int32 DamageEffects = 0;
    AActor* LastInstigator = nullptr;
    AActor* LastCauser = nullptr;
    bool bRecordedHit = false;
    const FDelegateHandle HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(
        UShooterAttributeSet::GetHealthAttribute()).AddLambda(
            [&HealthEvents](const FOnAttributeChangeData& Data)
            { if (Data.OldValue != Data.NewValue) { ++HealthEvents; } });
    const FDelegateHandle EffectHandle = ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
        [&](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
        {
            ++DamageEffects;
            LastInstigator = Spec.GetContext().GetInstigator();
            LastCauser = Spec.GetContext().GetEffectCauser();
            const FHitResult* RecordedHit = Spec.GetContext().GetHitResult();
            bRecordedHit = RecordedHit && RecordedHit->ImpactPoint.Equals(Hit.ImpactPoint);
        });
    ON_SCOPE_EXIT
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UShooterAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
        ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(EffectHandle);
    };

    TestFalse(TEXT("Miss is safe"), UShooterDamageLibrary::ApplyGASDamage(Source, nullptr, 25, Weapon, Hit));
    TestFalse(TEXT("Non-GAS wall is safe"), UShooterDamageLibrary::ApplyGASDamage(Source, Wall, 25, Weapon, Hit));
    TestFalse(TEXT("Missing source rejected"), UShooterDamageLibrary::ApplyGASDamage(nullptr, Target, 25, Weapon, Hit));
    TestFalse(TEXT("Zero damage rejected"), UShooterDamageLibrary::ApplyGASDamage(Source, Target, 0, Weapon, Hit));
    TestFalse(TEXT("Negative damage cannot heal"), UShooterDamageLibrary::ApplyGASDamage(Source, Target, -25, Weapon, Hit));
    TestFalse(TEXT("NaN damage rejected"), UShooterDamageLibrary::ApplyGASDamage(
        Source, Target, std::numeric_limits<float>::quiet_NaN(), Weapon, Hit));
    TestEqual(TEXT("Rejected inputs leave health unchanged"), Target->GetGASHealth(), 100.0f);
    TestEqual(TEXT("Rejected inputs apply no effect"), DamageEffects, 0);

    for (int32 Shot = 1; Shot <= 4; ++Shot)
    {
        TestTrue(TEXT("Positive shot applies damage"), UShooterDamageLibrary::ApplyGASDamage(Source, Target, 25, Weapon, Hit));
        TestEqual(FString::Printf(TEXT("Health after shot %d"), Shot), Target->GetGASHealth(), 100.0f - Shot * 25.0f);
    }
    TestEqual(TEXT("One effect per shot"), DamageEffects, 4);
    TestEqual(TEXT("One value-change event per shot"), HealthEvents, 4);
    TestTrue(TEXT("Legacy source preserved as instigator"), LastInstigator == Source);
    TestTrue(TEXT("Weapon preserved as effect causer"), LastCauser == Weapon);
    TestTrue(TEXT("Hit information preserved"), bRecordedHit);
    TestFalse(TEXT("Zero-health target rejects further damage"),
        UShooterDamageLibrary::ApplyGASDamage(Source, Target, 25, Weapon, Hit));
    TestEqual(TEXT("Corpse hit adds no effect"), DamageEffects, 4);

    AMyShooter* SecondTarget = World->SpawnActor<AMyShooter>(Params);
    AMyShooter* GASSource = World->SpawnActor<AMyShooter>(Params);
    if (!TestNotNull(TEXT("Second target"), SecondTarget) || !TestNotNull(TEXT("GAS source"), GASSource)) { return false; }
    TestTrue(TEXT("Source with ASC also works"),
        UShooterDamageLibrary::ApplyGASDamage(GASSource, SecondTarget, 150, nullptr, FHitResult()));
    TestEqual(TEXT("Overkill clamps to zero"), SecondTarget->GetGASHealth(), 0.0f);
    TestEqual(TEXT("Target damage leaves source untouched"), GASSource->GetGASHealth(), 100.0f);

    AMyShooter* Pending = World->SpawnActorDeferred<AMyShooter>(
        AMyShooter::StaticClass(), FTransform::Identity, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Deferred target"), Pending)) { return false; }
    TestFalse(TEXT("Uninitialized target rejects damage"),
        UShooterDamageLibrary::ApplyGASDamage(Source, Pending, 25, Weapon, Hit));
    Pending->FinishSpawning(FTransform::Identity);
    Pending->Destroy();
    TestFalse(TEXT("Destroyed target is safe"),
        UShooterDamageLibrary::ApplyGASDamage(Source, Pending, 25, Weapon, Hit));
    return true;
}
#endif
