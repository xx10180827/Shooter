// 验证真实射线/GAS 伤害结果、霰弹汇总、无效命中、多杀去重与生命周期清理。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Combat/ShooterCombatFeedbackComponent.h"
#include "Combat/ShooterFeedbackConfig.h"
#include "Combat/ShooterDamageLibrary.h"
#include "GAS/ShooterGameplayTags.h"
#include "AbilitySystemComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterCombatFeedbackTest, "MyShoot.Combat.DamageFeedback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShooterCombatFeedbackTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 Frame = GFrameCounter;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter = Frame; };
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance = [&](float Time) { while (Time > KINDA_SMALL_NUMBER) { const float Step = FMath::Min(Time, .01f); ++GFrameCounter; World->Tick(LEVELTICK_All, Step); Time -= Step; } };
    auto* Definition = NewObject<UShooterWeaponDefinition>(); Definition->bAutomatic = false; Definition->Damage = 5.f; Definition->PelletCount = 8; Definition->SpreadHalfAngle = 0; Definition->RecoilPitch = 0;
    auto* Player = World->SpawnActorDeferred<AMyShooter>(AMyShooter::StaticClass(), FTransform::Identity);
    Player->GetShooterWeapon()->ConfigureLoadout({Definition});
    auto* Camera = NewObject<UCameraComponent>(Player); Camera->SetupAttachment(Player->GetRootComponent()); Camera->RegisterComponent();
    Player->FinishSpawning(FTransform::Identity); Player->GetCharacterMovement()->DisableMovement();
    auto* PC = World->SpawnActor<APlayerController>(); PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Player); PC->SetControlRotation(FRotator::ZeroRotator);
    // Possess 会恢复 CharacterMovement；无地面的测试世界须在接管后固定角色。
    Player->GetCharacterMovement()->DisableMovement();
    auto* Feedback = NewObject<UShooterCombatFeedbackComponent>(PC); Feedback->RegisterComponent();
    auto* Weapon = Player->GetShooterWeapon(); Feedback->ObserveWeapon(Weapon);
    if (!TestNotNull(TEXT("Loaded feedback config"), Feedback->GetConfig())) { return false; }
    TestNotNull(TEXT("Kill icon cooked reference"), Feedback->GetConfig()->KillIcon.Get());
    TestNotNull(TEXT("Blood asset reference"), Feedback->GetConfig()->BloodEffect.Get());
    auto SpawnTarget = [&](FVector Location)
    {
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Target = World->SpawnActor<AShooterCharacterBase>(Location, FRotator::ZeroRotator, P);
        Target->GetCharacterMovement()->DisableMovement(); Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); return Target;
    };
    auto* Target = SpawnTarget(FVector(200, 0, 0));
        auto Fire = [&]()
    {
        PC->PlayerCameraManager->UpdateCamera(.01f);
        FVector Eye; FRotator Facing; PC->GetPlayerViewPoint(Eye, Facing); FHitResult Trace;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FeedbackTest), false, Player);
        World->LineTraceSingleByChannel(Trace, Eye, Eye + Facing.Vector() * 1000.f, ECC_Visibility, Q);
        const bool Fired = Weapon->StartFiring(); Weapon->StopFiring();
        AddInfo(FString::Printf(TEXT("SHOT accepted=%d trace=%s eye=%s target=%s health=%.1f confirmations=%d kills=%d"), Fired, *GetNameSafe(Trace.GetActor()), *Eye.ToString(), *Target->GetActorLocation().ToString(), Target->GetGASHealth(), Feedback->GetConfirmationCount(), Feedback->GetMultiKillCount()));
        return Fired;
    };
    TestTrue(TEXT("Real shotgun fires"), Fire());
    TestEqual(TEXT("Eight pellets deduct forty once"), Target->GetGASHealth(), 60.f);
    TestEqual(TEXT("Eight pellets make one confirmation"), Feedback->GetConfirmationCount(), 1);
    TestFalse(TEXT("Nonlethal marker is white"), Feedback->IsKillMarker());
    TestTrue(TEXT("Hit marker visible"), Feedback->GetMarkerAlpha() > 0.f);
    Advance(.25f); TestEqual(TEXT("Marker fades"), Feedback->GetMarkerAlpha(), 0.f);
    Target->GetAbilitySystemComponent()->AddLooseGameplayTag(ShooterGameplayTags::State_Invulnerable);
    Fire(); TestEqual(TEXT("Invulnerability rejects success feedback"), Feedback->GetConfirmationCount(), 1); TestEqual(TEXT("Invulnerability blocks damage"), Target->GetGASHealth(), 60.f);
    Target->GetAbilitySystemComponent()->RemoveLooseGameplayTag(ShooterGameplayTags::State_Invulnerable); Advance(.12f);
    auto* Wall = World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(10,100,150)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Box->RegisterComponent(); Wall->SetActorLocation(FVector(100,0,0));
    Fire(); TestEqual(TEXT("Wall does not confirm enemy damage"), Feedback->GetConfirmationCount(), 1);
    Wall->Destroy(); Target->SetActorLocation(FVector(200,400,0)); Advance(.12f); Fire(); TestEqual(TEXT("Miss does not confirm"), Feedback->GetConfirmationCount(), 1);
    Target->SetActorLocation(FVector(200,0,0)); Advance(.12f); Fire(); Advance(.12f); Fire();
    TestTrue(TEXT("Actual lethal shot turns marker gold"), Feedback->IsKillMarker()); TestEqual(TEXT("First actual kill"), Feedback->GetMultiKillCount(), 1);
    const int32 Confirmed = Feedback->GetConfirmationCount(); Advance(.12f); Fire(); TestEqual(TEXT("Corpse cannot confirm again"), Feedback->GetConfirmationCount(), Confirmed);

    // 两个目标分别进行真实 GAS 结算，再模拟同发汇总事件，检查声音/HUD 合并及重复回调拒绝。
    auto* A = SpawnTarget(FVector(200,500,0)); auto* B = SpawnTarget(FVector(200,-500,0));
    FShooterShotResult Multi; Multi.ShotId = 100;
    for (auto* Victim : {A, B})
    {
        FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Victim->GetActorLocation(), FVector(-1,0,0)); Hit.bBlockingHit = true;
        auto Result = UShooterDamageLibrary::ResolveGASDamage(Player, Victim, 300.f, Player, Hit);
        TestEqual(TEXT("Actual damage clamps overkill to remaining health"), Result.ActualDamage, 100.f); TestTrue(TEXT("Kill result reflects transition"), Result.bKilled); Multi.Targets.Add(Result);
    }
    Feedback->HandleShotResolved(Multi);
    TestEqual(TEXT("Two same-shot kills add once to streak"), Feedback->GetMultiKillCount(), 3);
    TestEqual(TEXT("Multi-target batch makes one confirmation"), Feedback->GetConfirmationCount(), Confirmed + 1);
    Feedback->HandleShotResolved(Multi); TestEqual(TEXT("Repeated shot result ignored"), Feedback->GetMultiKillCount(), 3);
    TestTrue(TEXT("Kill icon visible"), Feedback->GetIconAlpha() > 0.f);
    Advance(Feedback->GetConfig()->MultiKillWindow + .1f); TestEqual(TEXT("Icon expires"), Feedback->GetIconAlpha(), 0.f);
    auto* C = SpawnTarget(FVector(200,700,0)); FShooterShotResult Next; Next.ShotId = 101;
    Next.Targets.Add(UShooterDamageLibrary::ResolveGASDamage(Player, C, 100.f, Player, FHitResult())); Feedback->HandleShotResolved(Next);
    TestEqual(TEXT("Expired streak restarts at one"), Feedback->GetMultiKillCount(), 1);
    Feedback->HandleRoundState(EShooterRoundState::Won); TestTrue(TEXT("Final kill remains visible over victory"), Feedback->GetIconAlpha() > 0.f);
    Feedback->HandleRoundState(EShooterRoundState::Paused); TestEqual(TEXT("Pause clears marker"), Feedback->GetMarkerAlpha(), 0.f); TestEqual(TEXT("Pause clears streak"), Feedback->GetMultiKillCount(), 0);
    Feedback->HandleRoundState(EShooterRoundState::Lost); TestEqual(TEXT("Death/result clears icon"), Feedback->GetIconAlpha(), 0.f);
    Feedback->ObserveWeapon(nullptr);
    return true;
}
#endif
