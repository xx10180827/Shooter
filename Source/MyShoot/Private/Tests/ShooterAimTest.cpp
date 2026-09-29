// 开镜规则验证：切换、原视野恢复、标签计数以及换弹/暂停/死亡中断。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterAimComponent.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Game/ShooterGameMode.h"
#include "AbilitySystemComponent.h"
#include "GAS/ShooterGameplayTags.h"
#include "Combat/ShooterDamageLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterAimTest,"MyShoot.Gameplay.Aiming",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShooterAimTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    FWorldContext& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    UGameInstance* GI=NewObject<UGameInstance>(GEngine); World->SetGameInstance(GI); Context.OwningGameInstance=GI;
    const uint64 SavedFrame=GFrameCounter;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter=SavedFrame; };
    World->GetWorldSettings()->DefaultGameMode=AShooterGameMode::StaticClass();
    if(!World->SetGameMode(FURL())) { AddError(TEXT("Missing test GameMode")); return false; }
    World->InitializeActorsForPlay(FURL());
    AMyShooter* Player=World->SpawnActor<AMyShooter>();
    UCameraComponent* Camera=NewObject<UCameraComponent>(Player,TEXT("AimTestCamera"));
    Camera->SetupAttachment(Player->GetRootComponent()); Camera->SetFieldOfView(87.f); Camera->RegisterComponent();
    World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay(); Player->GetCharacterMovement()->DisableMovement();
    AShooterGameMode* GM=CastChecked<AShooterGameMode>(World->GetAuthGameMode());
    UShooterAimComponent* Aim=Player->GetShooterAim(); UAbilitySystemComponent* ASC=Player->GetAbilitySystemComponent();
    auto Advance=[World](float Seconds) { while(Seconds>KINDA_SMALL_NUMBER) { float Step=FMath::Min(Seconds,0.01f); ++GFrameCounter; World->Tick(LEVELTICK_All,Step); Seconds-=Step; } };
    TestFalse(TEXT("Menu rejects aiming"),Aim->StartAiming()); GM->StartRound();
    Aim->ToggleAiming(); Advance(0.25f);
    TestTrue(TEXT("First press enters aiming"),Aim->IsAiming()); TestEqual(TEXT("ADS field of view"),Camera->FieldOfView,65.f);
    TestEqual(TEXT("One owned aiming tag"),ASC->GetTagCount(ShooterGameplayTags::State_Aiming),1);
    Aim->StartAiming(); Aim->StartAiming(); TestEqual(TEXT("Repeated requests do not stack tags"),ASC->GetTagCount(ShooterGameplayTags::State_Aiming),1);
    Aim->ToggleAiming(); Advance(0.25f);
    TestFalse(TEXT("Second press exits aiming"),Aim->IsAiming()); TestEqual(TEXT("Restores original FOV, not hardcoded 90"),Camera->FieldOfView,87.f);
    const FVector Baseline=Player->GetMesh()->GetRelativeLocation();
    for(int32 Index=0;Index<30;++Index) { Aim->ToggleAiming(); Advance(0.015f); }
    Advance(0.25f); TestEqual(TEXT("Rapid toggles return to original FOV"),Camera->FieldOfView,87.f);
    TestTrue(TEXT("No cumulative mesh offset"),Player->GetMesh()->GetRelativeLocation().Equals(Baseline));
    TestEqual(TEXT("Rapid toggles leave no tag"),ASC->GetTagCount(ShooterGameplayTags::State_Aiming),0);
    Player->GetShooterWeapon()->StartFiring(); Player->GetShooterWeapon()->StopFiring(); Aim->StartAiming(); Advance(0.2f);
    TestTrue(TEXT("Reload starts"),Player->GetShooterWeapon()->StartReloading());
    TestFalse(TEXT("Reload immediately exits ADS"),Aim->IsAiming()); TestEqual(TEXT("Reload resets camera immediately"),Camera->FieldOfView,87.f);
    TestFalse(TEXT("Reload blocks new ADS request"),Aim->StartAiming()); Advance(2.f);
    TestFalse(TEXT("Reload completion does not re-enter ADS"),Aim->IsAiming());
    Aim->StartAiming(); Advance(0.2f); GM->TogglePause();
    TestFalse(TEXT("Pause exits ADS"),Aim->IsAiming()); TestEqual(TEXT("Pause immediately restores FOV"),Camera->FieldOfView,87.f);
    TestFalse(TEXT("Pause blocks ADS"),Aim->StartAiming()); GM->TogglePause(); TestFalse(TEXT("Resume does not latch ADS"),Aim->IsAiming());
    Aim->StartAiming(); Advance(0.2f);
    AActor* DamageSource=World->SpawnActor<AActor>(); UShooterDamageLibrary::ApplyGASDamage(DamageSource,Player,1000,DamageSource,FHitResult());
    TestFalse(TEXT("Death exits ADS"),Aim->IsAiming()); TestEqual(TEXT("Death clears aiming tag"),ASC->GetTagCount(ShooterGameplayTags::State_Aiming),0);
    TestEqual(TEXT("Death restores FOV"),Camera->FieldOfView,87.f); TestFalse(TEXT("Dead player cannot aim"),Aim->StartAiming());
    return true;
}
#endif
