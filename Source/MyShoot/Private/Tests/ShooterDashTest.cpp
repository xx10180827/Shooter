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
#include "GAS/Effects/ShooterDamageEffect.h"
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
    auto* Source=World->SpawnActor<AActor>();
    TestTrue(TEXT("Damage before dash applies"),UShooterDamageLibrary::ApplyGASDamage(Source,Player,20,Source,FHitResult()));
    const FVector Start=Player->GetActorLocation();
    if(!TestTrue(TEXT("Ground dash starts"),Dash->TryDash())) { return false; }
    TestEqual(TEXT("Commit spends exactly 25"),Dash->GetStamina(),75.f);
    TestTrue(TEXT("GE grants cooldown"),ASC->HasMatchingGameplayTag(ShooterGameplayTags::Cooldown_Dash));
    TestTrue(TEXT("State tag active"),Dash->IsDashing());
    TestTrue(TEXT("Invulnerability active only with dash"),ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Invulnerable));
    TestFalse(TEXT("Damage library rejects hit during dash"),UShooterDamageLibrary::ApplyGASDamage(Source,Player,1000,Source,FHitResult()));
    auto DamageSpec=ASC->MakeOutgoingSpec(UShooterDamageEffect::StaticClass(),1.f,ASC->MakeEffectContext());
    DamageSpec.Data->SetSetByCallerMagnitude(UShooterDamageEffect::GetHealthDeltaTag(),-50.f);
    ASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
    TestEqual(TEXT("Direct damage GE is also blocked"),Player->GetGASHealth(),80.f);
    DamageSpec.Data->SetSetByCallerMagnitude(UShooterDamageEffect::GetHealthDeltaTag(),5.f);
    ASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
    TestEqual(TEXT("Invulnerability still permits healing"),Player->GetGASHealth(),85.f);
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
    TestFalse(TEXT("Normal completion removes invulnerability"),ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Invulnerable));
    TestTrue(TEXT("Cooldown does not grant immunity"),UShooterDamageLibrary::ApplyGASDamage(Source,Player,5,Source,FHitResult()));
    TestTrue(TEXT("Mesh returns"),Player->GetMesh()->GetRelativeLocation().Equals(MeshOrigin,.01f));
    TestTrue(TEXT("FOV returns"),FMath::IsNearlyEqual(Camera->FieldOfView,90.f));
    TestFalse(TEXT("Cooldown rejects new dash"),Dash->TryDash());
    Advance(1.1f); TestTrue(TEXT("Stamina regenerates after cooldown"),Dash->GetStamina()>75.f);
    TestTrue(TEXT("Weapon switching resumes"),W->EquipWeapon(1)); W->EquipWeapon(0);
    W->StartFiring(); W->StopFiring(); TestTrue(TEXT("Reload starts"),W->StartReloading());
    const float Before=Dash->GetStamina(); TestFalse(TEXT("Reload blocks dash"),Dash->TryDash()); TestEqual(TEXT("Rejected dash free"),Dash->GetStamina(),Before); W->CancelReloading();
    SetStamina(10.f); TestFalse(TEXT("Insufficient stamina blocks"),Dash->TryDash()); TestEqual(TEXT("Failed cost unchanged"),Dash->GetStamina(),10.f);
    SetStamina(500.f); TestEqual(TEXT("Stamina clamped to max"),Dash->GetStamina(),100.f);
    auto* Movement=Player->GetCharacterMovement();
    // 上升和下落两个入口均锁定高度；使用非默认重力验证结束时恢复原值。
    for(float VerticalSpeed:{600.f,-600.f})
    {
        SetStamina(100.f); Player->SetActorLocation(FVector(0,2000,1000),false,nullptr,ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Falling); Movement->GravityScale=1.7f; Movement->Velocity=FVector(0,0,VerticalSpeed);
        PC->SetControlRotation(FRotator(40,0,0)); const FVector AirStart=Player->GetActorLocation();
        if(!TestTrue(TEXT("Air dash starts rising or falling"),Dash->TryDash())) { return false; }
        for(int32 I=0;I<20;++I)
        {
            Advance(.01f);
            TestTrue(TEXT("Air dash holds height every active frame"),FMath::Abs(Player->GetActorLocation().Z-AirStart.Z)<.1f);
        }
        TestTrue(TEXT("Air dash remains falling mode"),Movement->IsFalling());
        Advance(.05f);
        const float AirTravel=FVector::Dist2D(AirStart,Player->GetActorLocation());
        AddInfo(FString::Printf(TEXT("Air dash from Vz %.0f: %.2f cm, gravity %.2f"),VerticalSpeed,AirTravel,Movement->GravityScale));
        TestTrue(TEXT("Air distance matches ground distance"),FMath::Abs(AirTravel-Dash->DashDistance)<20.f);
        TestFalse(TEXT("Air completion removes immunity"),ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Invulnerable));
        TestEqual(TEXT("Original gravity restored"),Movement->GravityScale,1.7f);
        Advance(.15f); TestTrue(TEXT("Gravity resumes after dash"),Movement->Velocity.Z<0.f&&Player->GetActorLocation().Z<AirStart.Z-1.f);
        Advance(1.2f);
    }
    // 空中也验证跨过结束时刻的长帧，不能因积分方式不同而比地面走得更远。
    Player->SetActorLocation(FVector(0,2000,1000),false,nullptr,ETeleportType::TeleportPhysics);
    Movement->SetMovementMode(MOVE_Falling); Movement->Velocity=FVector::ZeroVector; SetStamina(100.f);
    const FVector AirHitchStart=Player->GetActorLocation();
    TestTrue(TEXT("Air hitch dash starts"),Dash->TryDash());
    for(float Dt:{.07f,.12f,.10f,.10f}) { ++GFrameCounter; World->Tick(LEVELTICK_All,Dt); }
    const float AirHitchTravel=FVector::Dist2D(AirHitchStart,Player->GetActorLocation());
    AddInfo(FString::Printf(TEXT("Air hitch distance %.2f cm"),AirHitchTravel));
    TestTrue(TEXT("Air hitch distance matches ground"),FMath::Abs(AirHitchTravel-Dash->DashDistance)<20.f);
    Advance(1.4f);
    // 空中碰墙、手动取消以及暂停取消都不能残留悬浮或无敌。
    Box(FVector(180,3000,1000),FVector(20,300,300));
    Player->SetActorLocation(FVector(0,3000,1000),false,nullptr,ETeleportType::TeleportPhysics);
    Movement->SetMovementMode(MOVE_Falling); Movement->Velocity=FVector::ZeroVector; SetStamina(100.f);
    TestTrue(TEXT("Air wall dash starts"),Dash->TryDash()); Advance(.18f);
    TestTrue(TEXT("Air wall blocks capsule"),Player->GetActorLocation().X<145.f);
    TestTrue(TEXT("Air wall does not cause fall during dash"),FMath::Abs(Player->GetActorLocation().Z-1000.f)<.1f);
    Dash->CancelDash(); TestEqual(TEXT("Cancel restores gravity"),Movement->GravityScale,1.7f);
    TestFalse(TEXT("Cancel removes immunity"),ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Invulnerable));
    TestTrue(TEXT("Cancelled dash can take damage immediately"),UShooterDamageLibrary::ApplyGASDamage(Source,Player,5,Source,FHitResult()));
    Advance(1.4f);
    Player->SetActorLocation(FVector(0,4000,1000),false,nullptr,ETeleportType::TeleportPhysics);
    Movement->SetMovementMode(MOVE_Falling); Movement->Velocity=FVector::ZeroVector; SetStamina(100.f);
    TestTrue(TEXT("Air pause dash starts"),Dash->TryDash()); Advance(.05f); GM->TogglePause();
    TestFalse(TEXT("Air pause removes immunity"),ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Invulnerable));
    TestEqual(TEXT("Air pause restores gravity"),Movement->GravityScale,1.7f);
    GM->TogglePause(); const FVector AirPaused=Player->GetActorLocation(); Advance(.2f);
    TestTrue(TEXT("Resume falls without old horizontal motion"),Player->GetActorLocation().Z<AirPaused.Z&&FVector::Dist2D(AirPaused,Player->GetActorLocation())<1.f);
    Advance(1.3f); Movement->GravityScale=1.f;
    Player->SetActorLocation(FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics); Advance(.3f);
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
    // 独立验证暂停取消；前面的新增空中场景已消耗多次体力。
    SetStamina(100.f); PC->SetControlRotation(FRotator(0,180,0));
    TestTrue(TEXT("Pause test dash starts"),Dash->TryDash()); Advance(.06f); GM->TogglePause();
    const FVector Paused=Player->GetActorLocation();
    TestFalse(TEXT("Pause cancels ability"),Dash->IsDashing()); TestTrue(TEXT("Pause resets FOV"),FMath::IsNearlyEqual(Camera->FieldOfView,90.f));
    TestFalse(TEXT("Pause rejects dash"),Dash->TryDash()); GM->TogglePause(); Advance(.3f);
    TestTrue(TEXT("Resume has no old dash motion"),FVector::Dist2D(Paused,Player->GetActorLocation())<1.f); Advance(1.2f); SetStamina(100.f);
    TestTrue(TEXT("Death test dash starts"),Dash->TryDash()); Advance(.06f);
    // 强制生命值归零模拟对局强制死亡；普通攻击在此刻必须被无敌拦截。
    TestFalse(TEXT("Lethal combat hit blocked during dash"),UShooterDamageLibrary::ApplyGASDamage(Source,Player,1000,Source,FHitResult()));
    ASC->SetNumericAttributeBase(UShooterAttributeSet::GetHealthAttribute(),0.f);
    TestFalse(TEXT("Death removes invulnerability"),ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Invulnerable));
    TestEqual(TEXT("Death restores gravity"),Movement->GravityScale,1.f);
    TestFalse(TEXT("Death cancels dash"),Dash->IsDashing()); TestFalse(TEXT("Dead cannot dash"),Dash->TryDash());
    const float DeadStamina=Dash->GetStamina(); Advance(1.5f); TestEqual(TEXT("Dead does not regenerate"),Dash->GetStamina(),DeadStamina);
    return true;
}
#endif