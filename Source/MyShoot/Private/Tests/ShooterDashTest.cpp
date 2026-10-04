// 验证 GAS 闪避生命周期、地面墙体扫掠、成本冷却以及武器和暂停/死亡互斥。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Characters/MyShooter.h"
#include "Movement/ShooterDashComponent.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Weapons/ShooterAimComponent.h"
#include "Game/ShooterGameMode.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "GAS/ShooterGameplayTags.h"
#include "Combat/ShooterDamageLibrary.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterDashTest,"MyShoot.GAS.DashLifecycleAndCollision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShooterDashTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    auto* GI=NewObject<UGameInstance>(GEngine); World->SetGameInstance(GI); Context.OwningGameInstance=GI;
    const uint64 OldFrame=GFrameCounter;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter=OldFrame; };
    World->GetWorldSettings()->DefaultGameMode=AShooterGameMode::StaticClass(); World->SetGameMode(FURL()); World->InitializeActorsForPlay(FURL());
    auto Box=[&](FVector Position,FVector Extent)
    {
        auto* Actor=World->SpawnActor<AActor>(); auto* Shape=NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Shape); Shape->SetBoxExtent(Extent); Shape->SetCollisionProfileName(TEXT("BlockAll"));
        Shape->SetCollisionObjectType(ECC_WorldStatic); Shape->RegisterComponent(); Actor->SetActorLocation(Position); return Actor;
    };
    Box(FVector(0,0,-20),FVector(10000,10000,20));
    auto* Rifle=NewObject<UShooterWeaponDefinition>(World); auto* Shotgun=NewObject<UShooterWeaponDefinition>(World);
    auto* Player=World->SpawnActorDeferred<AMyShooter>(AMyShooter::StaticClass(),FTransform(FVector(0,0,100)));
    Player->GetShooterWeapon()->ConfigureLoadout({Rifle,Shotgun});
    auto* Camera=NewObject<UCameraComponent>(Player); Camera->SetupAttachment(Player->GetRootComponent()); Camera->SetFieldOfView(90.f); Camera->RegisterComponent();
    Player->FinishSpawning(FTransform(FVector(0,0,100)));
    auto* PC=World->SpawnActor<APlayerController>(); PC->Possess(Player);
    World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* GM=CastChecked<AShooterGameMode>(World->GetAuthGameMode());
    auto* Dash=Player->FindComponentByClass<UShooterDashComponent>(); auto* ASC=Player->GetAbilitySystemComponent(); auto* W=Player->GetShooterWeapon(); auto* Aim=Player->GetShooterAim();
    auto Advance=[&](float Time) { for(int32 I=0;I<FMath::CeilToInt(Time/.01f);++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.01f); } };
    auto SetStamina=[&](float Value)
    {
        auto* Effect=NewObject<UGameplayEffect>(); Effect->DurationPolicy=EGameplayEffectDurationType::Instant;
        FGameplayModifierInfo Mod; Mod.Attribute=UShooterAttributeSet::GetStaminaAttribute(); Mod.ModifierOp=EGameplayModOp::Override; Mod.ModifierMagnitude=FScalableFloat(Value);
        Effect->Modifiers.Add(Mod); ASC->ApplyGameplayEffectToSelf(Effect,1,ASC->MakeEffectContext());
    };
    TestFalse(TEXT("Menu rejects dash"),Dash->TryDash()); GM->StartRound(); Advance(.3f);
    TestTrue(TEXT("Character landed"),Player->GetCharacterMovement()->IsMovingOnGround());
    TestEqual(TEXT("Initial stamina"),Dash->GetStamina(),100.f);
    const FVector MeshOrigin=Player->GetMesh()->GetRelativeLocation();
    Aim->StartAiming(); Advance(.2f); W->StartFiring();
    const FVector Start=Player->GetActorLocation();
    if(!TestTrue(TEXT("Ground dash starts"),Dash->TryDash())) { return false; }
    TestEqual(TEXT("Commit spends exactly 25"),Dash->GetStamina(),75.f);
    TestTrue(TEXT("GE grants cooldown"),ASC->HasMatchingGameplayTag(ShooterGameplayTags::Cooldown_Dash));
    TestTrue(TEXT("State tag active"),Dash->IsDashing());
    TestFalse(TEXT("Rapid repeat rejected"),Dash->TryDash());
    TestEqual(TEXT("Repeat does not spend"),Dash->GetStamina(),75.f);
    TestFalse(TEXT("Existing fire cancelled"),W->IsFiring());
    TestFalse(TEXT("Fire blocked"),W->StartFiring()); TestFalse(TEXT("Reload blocked"),W->StartReloading());
    TestFalse(TEXT("Switch blocked"),W->EquipWeapon(1)); TestFalse(TEXT("Aim blocked"),Aim->StartAiming());
    Advance(.1f); TestTrue(TEXT("Camera feedback"),Camera->FieldOfView>90.f);
    TestTrue(TEXT("Hands lower"),Player->GetMesh()->GetRelativeLocation().Z<MeshOrigin.Z);
    Advance(.4f);
    const float Travel=Player->GetActorLocation().X-Start.X;
    TestTrue(TEXT("Configured ground distance"),FMath::Abs(Travel-Dash->DashDistance)<20.f);
    TestFalse(TEXT("Normal completion removes state"),Dash->IsDashing());
    TestTrue(TEXT("Mesh returns"),Player->GetMesh()->GetRelativeLocation().Equals(MeshOrigin,.01f));
    TestTrue(TEXT("FOV returns"),FMath::IsNearlyEqual(Camera->FieldOfView,90.f));
    TestFalse(TEXT("Cooldown rejects new dash"),Dash->TryDash());
    Advance(1.1f); TestTrue(TEXT("Stamina regenerates after cooldown"),Dash->GetStamina()>75.f);
    TestTrue(TEXT("Weapon switching resumes"),W->EquipWeapon(1)); W->EquipWeapon(0);
    W->StartFiring(); W->StopFiring(); TestTrue(TEXT("Reload starts"),W->StartReloading());
    const float Before=Dash->GetStamina(); TestFalse(TEXT("Reload blocks dash"),Dash->TryDash()); TestEqual(TEXT("Rejected dash free"),Dash->GetStamina(),Before); W->CancelReloading();
    SetStamina(10.f); TestFalse(TEXT("Insufficient stamina blocks"),Dash->TryDash()); TestEqual(TEXT("Failed cost unchanged"),Dash->GetStamina(),10.f);
    SetStamina(500.f); TestEqual(TEXT("Stamina clamped to max"),Dash->GetStamina(),100.f);
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling); TestFalse(TEXT("Air dash rejected"),Dash->TryDash()); Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    // 验证移动输入方向，不以角色朝向替代侧移输入。
    Player->AddMovementInput(FVector::RightVector,1.f); const FVector SideStart=Player->GetActorLocation();
    TestTrue(TEXT("Sideways dash starts"),Dash->TryDash()); Advance(.4f);
    TestTrue(TEXT("Dash follows input Y"),Player->GetActorLocation().Y-SideStart.Y>250.f); Advance(1.1f);
    // 模拟一次跨过结束时刻的长帧，验证距离不会按整帧继续积分。
    const FVector HitchStart=Player->GetActorLocation(); PC->SetControlRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Hitch-frame dash starts"),Dash->TryDash());
    for(float Dt:{.07f,.12f,.10f,.10f}) { ++GFrameCounter; World->Tick(LEVELTICK_All,Dt); }
    const float HitchDistance=FVector::Dist2D(HitchStart,Player->GetActorLocation());
    AddInfo(FString::Printf(TEXT("Dash travel: regular=%.2f cm hitch=%.2f cm"),Travel,HitchDistance));
    TestTrue(TEXT("Hitch does not overshoot configured distance"),FMath::Abs(HitchDistance-Dash->DashDistance)<20.f);
    Advance(1.1f);
    // 重置位置并放实体墙；应停在墙前而不是直接 SetActorLocation 到墙后。
    Player->SetActorLocation(FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics); PC->SetControlRotation(FRotator::ZeroRotator); Advance(.2f);
    Box(FVector(180,0,150),FVector(20,500,150));
    TestTrue(TEXT("Dash toward wall starts"),Dash->TryDash()); Advance(.4f);
    TestTrue(TEXT("Capsule cannot cross wall"),Player->GetActorLocation().X<145.f); Advance(1.1f);
    PC->SetControlRotation(FRotator(0,180,0));
    TestTrue(TEXT("Pause test dash starts"),Dash->TryDash()); Advance(.06f); GM->TogglePause();
    const FVector Paused=Player->GetActorLocation();
    TestFalse(TEXT("Pause cancels ability"),Dash->IsDashing()); TestTrue(TEXT("Pause resets FOV"),FMath::IsNearlyEqual(Camera->FieldOfView,90.f));
    TestFalse(TEXT("Pause rejects dash"),Dash->TryDash()); GM->TogglePause(); Advance(.3f);
    TestTrue(TEXT("Resume has no old dash motion"),FVector::Dist2D(Paused,Player->GetActorLocation())<1.f); Advance(1.2f); SetStamina(100.f);
    TestTrue(TEXT("Death test dash starts"),Dash->TryDash()); Advance(.06f);
    auto* Source=World->SpawnActor<AActor>(); UShooterDamageLibrary::ApplyGASDamage(Source,Player,1000,Source,FHitResult());
    TestFalse(TEXT("Death cancels dash"),Dash->IsDashing()); TestFalse(TEXT("Dead cannot dash"),Dash->TryDash());
    const float DeadStamina=Dash->GetStamina(); Advance(1.5f); TestEqual(TEXT("Dead does not regenerate"),Dash->GetStamina(),DeadStamina);
    return true;
}
#endif