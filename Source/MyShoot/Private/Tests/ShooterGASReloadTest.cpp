// 验证换弹的时序、守恒、输入互斥及取消清理，不依赖蓝图表现。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemComponent.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "GAS/ShooterGameplayTags.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "GAS/Abilities/ShooterFireAbility.h"
#include "GAS/Abilities/ShooterReloadAbility.h"
#include "GAS/Abilities/ShooterDashAbility.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGASReloadTest, "MyShoot.GAS.Reload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterGASReloadTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Reload world"), World)) { return false; }
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
    auto Advance = [World](float Seconds)
    {
        // 分小帧推进，避免 WorldSettings 对单帧 DeltaSeconds 的限制缩短计时。
        while (Seconds > KINDA_SMALL_NUMBER)
        {
            const float Step = FMath::Min(Seconds, 0.05f);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, Step);
            Seconds -= Step;
        }
    };
    FIntProperty* ReserveProperty = FindFProperty<FIntProperty>(UShooterWeaponComponent::StaticClass(), TEXT("InitialReserveAmmo"));
    if (!TestNotNull(TEXT("Reserve configuration"), ReserveProperty)) { return false; }
    int32 SpawnIndex = 0;
    auto Spawn = [&](int32 Reserve)
    {
        FTransform Transform(FVector(0, ++SpawnIndex * 1000.0f, 0));
        AMyShooter* Player = World->SpawnActorDeferred<AMyShooter>(AMyShooter::StaticClass(),
            Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (Player)
        {
            ReserveProperty->SetPropertyValue_InContainer(Player->GetShooterWeapon(), Reserve);
            Player->FinishSpawning(Transform);
            Player->GetCharacterMovement()->DisableMovement();
        }
        return Player;
    };
    // 通过实际发射消耗弹匣，避免测试通过直接修改运行时字段绕过规则。
    auto Spend = [&](UShooterWeaponComponent* Weapon, int32 Count)
    {
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Advance(0.11f);
            Weapon->StartFiring();
            Weapon->StopFiring();
        }
    };
    AMyShooter* Player = Spawn(50);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    UShooterWeaponComponent* Weapon = Player->GetShooterWeapon();
    TestFalse(TEXT("Full magazine refuses reload"), Weapon->StartReloading());
    Spend(Weapon, 20);
    TestEqual(TEXT("Ten bullets before reload"), Weapon->GetCurrentAmmo(), 10);
    TestTrue(TEXT("Reload starts"), Weapon->StartReloading());
    TestTrue(TEXT("Owned tag active"), Weapon->IsReloading());
    TestFalse(TEXT("Cannot shoot during reload"), Weapon->StartFiring());
    Advance(0.7f);
    TestFalse(TEXT("Repeated R does not restart"), Weapon->StartReloading());
    TestEqual(TEXT("No early transfer"), Weapon->GetCurrentAmmo(), 10);
    TestEqual(TEXT("Reserve unchanged until completion"), Weapon->GetReserveAmmo(), 50);
    Advance(0.81f);
    TestEqual(TEXT("Reload fills 30"), Weapon->GetCurrentAmmo(), 30);
    TestEqual(TEXT("Transfers exactly 20 from reserve"), Weapon->GetReserveAmmo(), 30);
    TestFalse(TEXT("Completion removes tag"), Weapon->IsReloading());
    TestFalse(TEXT("Completion does not resume fire"), Weapon->IsFiring());
    Advance(0.2f);
    TestEqual(TEXT("No duplicate delayed transfer"), Weapon->GetReserveAmmo(), 30);

    AMyShooter* Limited = Spawn(5);
    if (!TestNotNull(TEXT("Limited player"), Limited)) { return false; }
    UShooterWeaponComponent* LimitedWeapon = Limited->GetShooterWeapon();
    Spend(LimitedWeapon, 20);
    TestTrue(TEXT("Limited reserve reload begins"), LimitedWeapon->StartReloading());
    Advance(1.51f);
    TestEqual(TEXT("10 plus 5 becomes 15"), LimitedWeapon->GetCurrentAmmo(), 15);
    TestEqual(TEXT("Reserve exhausted exactly"), LimitedWeapon->GetReserveAmmo(), 0);
    TestFalse(TEXT("Empty reserve refuses reload"), LimitedWeapon->StartReloading());

    // 补验用户反馈中的“打空”：弹匣为零但有备用弹药时必须仍可换弹。
    AMyShooter* EmptyPlayer = Spawn(90);
    if (!TestNotNull(TEXT("Empty-mag player"), EmptyPlayer)) { return false; }
    UShooterWeaponComponent* EmptyWeapon = EmptyPlayer->GetShooterWeapon();
    Spend(EmptyWeapon, 30);
    TestEqual(TEXT("Magazine is truly empty"), EmptyWeapon->GetCurrentAmmo(), 0);
    TestTrue(TEXT("Empty magazine with reserve can reload"), EmptyWeapon->StartReloading());
    Advance(1.6f);
    TestEqual(TEXT("Empty magazine refilled"), EmptyWeapon->GetCurrentAmmo(), 30);
    TestEqual(TEXT("Empty reload consumes thirty reserve"), EmptyWeapon->GetReserveAmmo(), 60);
    // 开火途中换弹取消连射；主动取消与重新换弹不能继承上一轮计时。
    Spend(Weapon, 1);
    Advance(0.11f);
    TestTrue(TEXT("Held fire begins"), Weapon->StartFiring());
    TestTrue(TEXT("Reload interrupts held fire"), Weapon->StartReloading());
    TestFalse(TEXT("Held fire immediately cancelled"), Weapon->IsFiring());
    const int32 BeforeCancel = Weapon->GetCurrentAmmo();
    const int32 ReserveBeforeCancel = Weapon->GetReserveAmmo();
    Advance(0.5f);
    Weapon->CancelReloading();
    TestFalse(TEXT("Cancel removes reload tag"), Weapon->IsReloading());
    Advance(1.2f);
    TestEqual(TEXT("Cancelled timer cannot refill"), Weapon->GetCurrentAmmo(), BeforeCancel);
    TestEqual(TEXT("Cancel consumes no reserve"), Weapon->GetReserveAmmo(), ReserveBeforeCancel);
    TestTrue(TEXT("Can reload again after cancel"), Weapon->StartReloading());
    Advance(0.5f);
    TestEqual(TEXT("New reload needs its full duration"), Weapon->GetCurrentAmmo(), BeforeCancel);
    Advance(1.01f);
    TestEqual(TEXT("Second activation completes"), Weapon->GetCurrentAmmo(), 30);

    Spend(Weapon, 1);
    TestTrue(TEXT("Reload before death"), Weapon->StartReloading());
    const int32 AmmoBeforeDeath = Weapon->GetCurrentAmmo();
    const int32 ReserveBeforeDeath = Weapon->GetReserveAmmo();
    AActor* Source = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Damage source"), Source)) { return false; }
    TestTrue(TEXT("Lethal GAS damage"), UShooterDamageLibrary::ApplyGASDamage(Source, Player, 100.0f, Source, FHitResult()));
    TestFalse(TEXT("Death immediately removes reload tag"), Weapon->IsReloading());
    TestFalse(TEXT("Dead actor refuses reload"), Weapon->StartReloading());
    Advance(1.6f);
    TestEqual(TEXT("Dead actor is not refilled later"), Weapon->GetCurrentAmmo(), AmmoBeforeDeath);
    TestEqual(TEXT("Death consumes no reserve"), Weapon->GetReserveAmmo(), ReserveBeforeDeath);

    AMyShooter* Removed = Spawn(90);
    if (!TestNotNull(TEXT("Removal player"), Removed)) { return false; }
    UShooterWeaponComponent* RemovedWeapon = Removed->GetShooterWeapon();
    Spend(RemovedWeapon, 1);
    TestTrue(TEXT("Reload before component removal"), RemovedWeapon->StartReloading());
    RemovedWeapon->DestroyComponent();
    Advance(1.6f);
    TestNull(TEXT("Removed weapon revokes fire"),Removed->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UShooterFireAbility::StaticClass()));
    TestNull(TEXT("Removed weapon revokes reload"),Removed->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UShooterReloadAbility::StaticClass()));
    TestNotNull(TEXT("Independent dash ability survives weapon removal"),Removed->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UShooterDashAbility::StaticClass()));
    TestFalse(TEXT("Removal leaves no owned reload tag"),
        Removed->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShooterGameplayTags::State_Reloading));
    return true;
}
#endif
