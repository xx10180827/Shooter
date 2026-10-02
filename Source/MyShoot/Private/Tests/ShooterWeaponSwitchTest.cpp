// 验证真实 GAS 能力、跨槽冷却、取消与多弹丸伤害，不直接改运行时弹药。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/UnrealType.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterWeaponSwitchTest,"MyShoot.Weapons.SwitchAndShotgun",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShooterWeaponSwitchTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if(!TestNotNull(TEXT("World"),World)) { return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 SavedFrame=GFrameCounter;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter=SavedFrame; };
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance=[World](float Seconds) { while(Seconds>KINDA_SMALL_NUMBER) { const float Step=FMath::Min(Seconds,.05f); ++GFrameCounter; World->Tick(LEVELTICK_All,Step); Seconds-=Step; } };
    auto* Rifle=NewObject<UShooterWeaponDefinition>(World); Rifle->DisplayName=FText::FromString(TEXT("Rifle"));
    auto* Shotgun=NewObject<UShooterWeaponDefinition>(World); Shotgun->DisplayName=FText::FromString(TEXT("Shotgun"));
    Shotgun->MagazineCapacity=8; Shotgun->InitialReserveAmmo=32; Shotgun->FireInterval=.85f;
    Shotgun->bAutomatic=false; Shotgun->Damage=12; Shotgun->PelletCount=8; Shotgun->SpreadHalfAngle=0;
    auto Spawn=[&](FVector Location)
    {
        const FTransform Transform(Location);
        auto* P=World->SpawnActorDeferred<AMyShooter>(AMyShooter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        P->GetShooterWeapon()->ConfigureLoadout({Rifle,Shotgun}); P->FinishSpawning(Transform); P->GetCharacterMovement()->DisableMovement(); return P;
    };
    auto* Player=Spawn(FVector::ZeroVector); auto* W=Player->GetShooterWeapon();
    TestEqual(TEXT("Two independent slots"),W->GetWeaponCount(),2);
    TestFalse(TEXT("Same slot is no-op"),W->EquipWeapon(0)); TestFalse(TEXT("Invalid slot rejected"),W->EquipWeapon(2));
    TestTrue(TEXT("Rifle fires"),W->StartFiring());
    TestTrue(TEXT("Switch cancels held fire"),W->EquipWeapon(1)); TestFalse(TEXT("No old fire instance"),W->IsFiring());
    TestEqual(TEXT("Shotgun starts with eight"),W->GetCurrentAmmo(),8);
    TestTrue(TEXT("Shotgun one shot"),W->StartFiring()); TestEqual(TEXT("Pellets consume one shell"),W->GetCurrentAmmo(),7);
    TestFalse(TEXT("Semi-auto ends after one shot"),W->IsFiring());
    TestTrue(TEXT("Back to rifle"),W->EquipWeapon(0)); TestEqual(TEXT("Rifle retains spent round"),W->GetCurrentAmmo(),29);
    W->StartFiring(); TestEqual(TEXT("Switch cannot bypass rifle cooldown"),W->GetCurrentAmmo(),29); W->StopFiring();
    W->EquipWeapon(1); W->StartFiring(); TestEqual(TEXT("Switch cannot bypass shotgun cooldown"),W->GetCurrentAmmo(),7); W->StopFiring();
    Advance(1.f); TestEqual(TEXT("Old loops do not fire on new slot"),W->GetCurrentAmmo(),7);
    W->EquipWeapon(0);
    while(W->GetCurrentAmmo()>12) { Advance(.11f); W->StartFiring(); W->StopFiring(); }
    TestTrue(TEXT("Rifle reload at twelve"),W->StartReloading()); Advance(.4f);
    TestTrue(TEXT("Switch during reload"),W->EquipWeapon(1)); TestFalse(TEXT("Reload tag removed"),W->IsReloading());
    TestTrue(TEXT("Back before old reload expiry"),W->EquipWeapon(0)); Advance(2.f);
    TestEqual(TEXT("Old callback cannot refill twelve"),W->GetCurrentAmmo(),12);
    TestEqual(TEXT("Cancelled reload consumes no reserve"),W->GetReserveAmmo(),90);
    W->StartReloading(); Advance(1.6f);
    TestEqual(TEXT("New reload fills rifle"),W->GetCurrentAmmo(),30); TestEqual(TEXT("Transfer exactly eighteen"),W->GetReserveAmmo(),72);
    W->EquipWeapon(1); TestEqual(TEXT("Shotgun still seven"),W->GetCurrentAmmo(),7); TestEqual(TEXT("Shotgun independent reserve"),W->GetReserveAmmo(),32);
    auto* Fresh=Spawn(FVector(0,1500,0)); TestEqual(TEXT("Shared assets do not share ammo"),Fresh->GetShooterWeapon()->GetCurrentAmmo(),30);
    TestEqual(TEXT("Config stays unchanged"),Rifle->InitialReserveAmmo,90);
    auto* Health=FindFProperty<FFloatProperty>(AShooterCharacterBase::StaticClass(),TEXT("InitialMaxHealth"));
    if(!TestNotNull(TEXT("Initial health"),Health)) { return false; }
    const FTransform TargetTransform(FVector(200,0,0));
    auto* Target=World->SpawnActorDeferred<AShooterCharacterBase>(AShooterCharacterBase::StaticClass(),TargetTransform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Health->SetPropertyValue_InContainer(Target,1000.f); Target->FinishSpawning(TargetTransform);
    Target->GetCharacterMovement()->DisableMovement(); Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    TestTrue(TEXT("Shotgun trace activates"),W->StartFiring());
    TestEqual(TEXT("Eight pellets at twelve damage"),Target->GetGASHealth(),904.f);
    TestEqual(TEXT("Eight traces cost one shell"),W->GetCurrentAmmo(),6);
    Advance(1.f); TestEqual(TEXT("Holding semi-auto does not repeat"),W->GetCurrentAmmo(),6);
    // 墙体阻挡所有弹丸；同一射击能力不通过特效或子弹模型再次扣血。
    auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(10,100,150)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Box->RegisterComponent();
    Wall->SetActorLocation(FVector(100,0,0));
    W->StartFiring(); TestEqual(TEXT("Wall blocks shotgun damage"),Target->GetGASHealth(),904.f); TestEqual(TEXT("Blocked shot still costs shell"),W->GetCurrentAmmo(),5);
    W->StartReloading(); UShooterDamageLibrary::ApplyGASDamage(Target,Player,1000.f,Target,FHitResult());
    TestFalse(TEXT("Dead player cannot switch"),W->EquipWeapon(0)); Advance(2.f);
    TestEqual(TEXT("Death leaves no delayed reload"),W->GetCurrentAmmo(),5);
    return true;
}
#endif
