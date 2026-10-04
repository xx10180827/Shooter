// 自动验证：真实射线伤害、单次扣弹、点按射速、松开停止、空弹匣及换弹/死亡中断。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemComponent.h"
#include "Characters/MyShooter.h"
#include "Characters/ShooterCharacterBase.h"
#include "Combat/ShooterDamageLibrary.h"
#include "GAS/ShooterGameplayTags.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "GAS/Abilities/ShooterFireAbility.h"
#include "GAS/Abilities/ShooterReloadAbility.h"
#include "GAS/Abilities/ShooterDashAbility.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGASFireTest, "MyShoot.GAS.Fire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterGASFireTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Fire test world"), World)) { return false; }
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

    // 同时推进世界时间和定时器；每步模拟一帧，避免仅推进定时器而射速时钟不变。
    auto Advance = [World](float Seconds)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, Seconds);
    };
    auto SpawnPlayer = [World](const FVector& Location)
    {
        AMyShooter* Player = World->SpawnActor<AMyShooter>(Location, FRotator::ZeroRotator);
        if (Player) { Player->GetCharacterMovement()->DisableMovement(); }
        return Player;
    };

    AMyShooter* Player = World->SpawnActorDeferred<AMyShooter>(AMyShooter::StaticClass(),
        FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    UShooterWeaponComponent* Weapon = Player->GetShooterWeapon();
    FIntProperty* Capacity = FindFProperty<FIntProperty>(UShooterWeaponComponent::StaticClass(), TEXT("MagazineCapacity"));
    if (!TestNotNull(TEXT("Weapon"), Weapon) || !TestNotNull(TEXT("Capacity property"), Capacity)) { return false; }
    Capacity->SetPropertyValue_InContainer(Weapon, 3);
    Player->FinishSpawning(FTransform::Identity);
    Player->GetCharacterMovement()->DisableMovement();

    AShooterCharacterBase* Target = World->SpawnActorDeferred<AShooterCharacterBase>(
        AShooterCharacterBase::StaticClass(), FTransform(FVector(200.0f, 0.0f, 0.0f)), nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Trace target"), Target)) { return false; }
    FFloatProperty* Health = FindFProperty<FFloatProperty>(AShooterCharacterBase::StaticClass(), TEXT("InitialMaxHealth"));
    if (!TestNotNull(TEXT("Initial health property"), Health)) { return false; }
    Health->SetPropertyValue_InContainer(Target, 250.0f);
    Target->FinishSpawning(FTransform(FVector(200.0f, 0.0f, 0.0f)));
    Target->GetCharacterMovement()->DisableMovement();
    Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    TestEqual(TEXT("Starts with configured full magazine"), Weapon->GetCurrentAmmo(), 3);
    TestTrue(TEXT("First press activates ability"), Weapon->StartFiring());
    TestEqual(TEXT("First shot consumes one bullet"), Weapon->GetCurrentAmmo(), 2);
    TestEqual(TEXT("Real trace applies GAS damage once"), Target->GetGASHealth(), 225.0f);
    TestTrue(TEXT("Repeated press keeps single activation"), Weapon->StartFiring());
    TestEqual(TEXT("Repeated press cannot fire again"), Weapon->GetCurrentAmmo(), 2);

    // 松开再快速按下：仍需等待上一发的射速间隔，不能在新激活时立刻补一发。
    Weapon->StopFiring();
    Advance(0.05f);
    TestEqual(TEXT("Released timer cannot shoot"), Weapon->GetCurrentAmmo(), 2);
    TestTrue(TEXT("Rapid re-press accepted but waits"), Weapon->StartFiring());
    TestEqual(TEXT("Reactivation respects persistent cooldown"), Weapon->GetCurrentAmmo(), 2);
    Advance(0.02f);
    TestEqual(TEXT("No shot before interval expires"), Weapon->GetCurrentAmmo(), 2);
    Advance(0.04f);
    TestEqual(TEXT("Held ability fires after interval"), Weapon->GetCurrentAmmo(), 1);
    TestEqual(TEXT("Second trace damage"), Target->GetGASHealth(), 200.0f);

    Weapon->StopFiring();
    Advance(0.11f);
    TestEqual(TEXT("Release clears next pending shot"), Weapon->GetCurrentAmmo(), 1);
    TestTrue(TEXT("Last bullet request accepted"), Weapon->StartFiring());
    TestEqual(TEXT("Last shot consumes final bullet"), Weapon->GetCurrentAmmo(), 0);
    TestFalse(TEXT("Empty magazine ends ability"), Weapon->IsFiring());
    TestFalse(TEXT("Empty magazine rejects new activation"), Weapon->StartFiring());
    Advance(0.5f);
    TestEqual(TEXT("Empty magazine cannot damage again"), Target->GetGASHealth(), 175.0f);

    // 第二个角色打空也扣弹；换弹状态必须立即取消能力，移除状态不自动恢复射击。
    AMyShooter* OtherPlayer = SpawnPlayer(FVector(0.0f, 1000.0f, 0.0f));
    if (!TestNotNull(TEXT("Second player"), OtherPlayer)) { return false; }
    UShooterWeaponComponent* OtherWeapon = OtherPlayer->GetShooterWeapon();
    UAbilitySystemComponent* ASC = OtherPlayer->GetAbilitySystemComponent();
    TestTrue(TEXT("Miss still fires"), OtherWeapon->StartFiring());
    TestEqual(TEXT("Miss consumes one bullet"), OtherWeapon->GetCurrentAmmo(), 29);
    ASC->AddLooseGameplayTag(ShooterGameplayTags::State_Reloading);
    TestFalse(TEXT("Reload tag immediately cancels fire"), OtherWeapon->IsFiring());
    TestFalse(TEXT("Reload tag blocks fire activation"), OtherWeapon->StartFiring());
    Advance(0.2f);
    TestEqual(TEXT("Reload cancellation clears timer"), OtherWeapon->GetCurrentAmmo(), 29);
    ASC->RemoveLooseGameplayTag(ShooterGameplayTags::State_Reloading);
    Advance(0.2f);
    TestEqual(TEXT("Reload completion does not auto-fire"), OtherWeapon->GetCurrentAmmo(), 29);

    // 活跃连射过程中死亡：CancelAllAbilities 必须清除能力持有的定时器。
    TestTrue(TEXT("Can fire again after reload state removed"), OtherWeapon->StartFiring());
    AActor* DamageSource = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("External damage source"), DamageSource)) { return false; }
    TestTrue(TEXT("Lethal damage applied"), UShooterDamageLibrary::ApplyGASDamage(
        DamageSource, OtherPlayer, 100.0f, DamageSource, FHitResult()));
    TestFalse(TEXT("Death immediately cancels fire"), OtherWeapon->IsFiring());
    Advance(0.3f);
    TestEqual(TEXT("Death cancellation leaves no extra shot"), OtherWeapon->GetCurrentAmmo(), 28);
    TestFalse(TEXT("Dead owner cannot activate fire"), OtherWeapon->StartFiring());

    // 移除组件时撤销能力；重新创建的角色应有独立弹匣，不继承旧定时器和弹药。
    AMyShooter* Fresh = SpawnPlayer(FVector(0.0f, 2000.0f, 0.0f));
    if (!TestNotNull(TEXT("Fresh player"), Fresh)) { return false; }
    TestEqual(TEXT("Fresh actor has independent full magazine"), Fresh->GetShooterWeapon()->GetCurrentAmmo(), 30);
    TestTrue(TEXT("Fresh weapon activates"), Fresh->GetShooterWeapon()->StartFiring());
    Fresh->GetShooterWeapon()->DestroyComponent();
    Advance(0.2f);
    // 武器只撤销自身能力；闪避由另一组件授予，应继续保留。
    TestNull(TEXT("Removed weapon revokes fire"),Fresh->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UShooterFireAbility::StaticClass()));
    TestNull(TEXT("Removed weapon revokes reload"),Fresh->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UShooterReloadAbility::StaticClass()));
    TestNotNull(TEXT("Independent dash ability survives weapon removal"),Fresh->GetAbilitySystemComponent()->FindAbilitySpecFromClass(UShooterDashAbility::StaticClass()));
    return true;
}
#endif
