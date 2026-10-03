// T15：通过真实触发器、相机射线和领取入口验证物品事务，不手动伪造候选登记。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Interaction/ShooterInteractionComponent.h"
#include "Pickups/ShooterWeaponPickup.h"
#include "Pickups/ShooterAmmoPickup.h"
#include "Game/ShooterGameMode.h"
#include "Combat/ShooterDamageLibrary.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterPickupTest,"MyShoot.Pickups.TriggerAndTransaction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShooterPickupTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    auto* GI=NewObject<UGameInstance>(GEngine); World->SetGameInstance(GI); Context.OwningGameInstance=GI;
    const uint64 SavedFrame=GFrameCounter;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter=SavedFrame; };
    World->GetWorldSettings()->DefaultGameMode=AShooterGameMode::StaticClass(); World->SetGameMode(FURL()); World->InitializeActorsForPlay(FURL());
    auto* Rifle=NewObject<UShooterWeaponDefinition>(World); Rifle->MaxReserveAmmo=100;
    auto* Shotgun=NewObject<UShooterWeaponDefinition>(World); Shotgun->MagazineCapacity=8; Shotgun->InitialReserveAmmo=32; Shotgun->MaxReserveAmmo=40;
    auto* Player=World->SpawnActorDeferred<AMyShooter>(AMyShooter::StaticClass(),FTransform::Identity);
    Player->GetShooterWeapon()->ConfigureLoadout({Rifle,Shotgun},1);
    auto* Camera=NewObject<UCameraComponent>(Player); Camera->SetupAttachment(Player->GetRootComponent()); Camera->SetRelativeLocation(FVector(0,0,64));
    Camera->bUsePawnControlRotation=true; Camera->RegisterComponent(); Player->FinishSpawning(FTransform::Identity);
    auto* PC=World->SpawnActor<APlayerController>(); PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Player); PC->SetViewTarget(Player);
    World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay(); Player->GetCharacterMovement()->DisableMovement();
    auto* GM=CastChecked<AShooterGameMode>(World->GetAuthGameMode()); GM->StartRound();
    auto* W=Player->GetShooterWeapon(); auto* I=Player->FindComponentByClass<UShooterInteractionComponent>();
    auto Advance=[&]() { ++GFrameCounter; World->Tick(LEVELTICK_All,.05f); PC->PlayerCameraManager->UpdateCamera(.01f); };
    auto Aim=[&]() { PC->SetControlRotation(FRotator::ZeroRotator); Advance(); I->RefreshFocus(); };
    auto SpawnGun=[&](FVector Location)
    {
        const FTransform Transform(Location); auto* Item=World->SpawnActorDeferred<AShooterWeaponPickup>(AShooterWeaponPickup::StaticClass(),Transform);
        Item->WeaponDefinition=Shotgun; Item->ItemName=FText::FromString(TEXT("霰弹枪")); Item->FinishSpawning(Transform); Item->ProximityTrigger->UpdateOverlaps(); Advance(); return Item;
    };
    auto SpawnAmmo=[&](UShooterWeaponDefinition* Definition,int32 Amount)
    {
        const FTransform Transform(FVector(160,0,64)); auto* Item=World->SpawnActorDeferred<AShooterAmmoPickup>(AShooterAmmoPickup::StaticClass(),Transform);
        Item->WeaponDefinition=Definition; Item->Amount=Amount; Item->FinishSpawning(Transform); Item->ProximityTrigger->UpdateOverlaps(); Advance(); return Item;
    };
    TestEqual(TEXT("Spawn owns only rifle"),W->GetWeaponCount(),1);
    TestFalse(TEXT("Unowned shotgun cannot equip"),W->EquipWeapon(1));
    auto* Gun=SpawnGun(FVector(160,0,64)); Aim();
    TestTrue(TEXT("Trigger automatically registers nearby gun"),I->GetFocusedPickup()==Gun);
    TestTrue(TEXT("Prompt offers F"),I->GetPrompt().ToString().Contains(TEXT("F")));
    // 即使仍处于重叠范围，墙体也会阻挡准星交互；错误请求不消耗物品。
    auto* Wall=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10,100,120)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); Box->RegisterComponent(); Wall->SetActorLocation(FVector(80,0,64));
    TestFalse(TEXT("Wall blocks F without waiting for refresh"),I->TryInteract()); TestFalse(TEXT("Occluded pickup survives"),Gun->IsConsumed());
    Wall->Destroy(); Gun->SetActorLocation(FVector(400,0,64)); Gun->ProximityTrigger->UpdateOverlaps();
    TestFalse(TEXT("Beyond trigger radius cannot collect"),I->TryInteract());
    Gun->SetActorLocation(FVector(160,0,64)); Gun->ProximityTrigger->UpdateOverlaps(); Aim();
    PC->SetControlRotation(FRotator(0,90,0)); PC->PlayerCameraManager->UpdateCamera(.01f);
    TestFalse(TEXT("Turn away rejects stale focus"),I->TryInteract()); Aim();
    GM->TogglePause(); TestFalse(TEXT("Paused F rejected"),I->TryInteract()); TestTrue(TEXT("Paused prompt hidden"),I->GetPrompt().IsEmpty()); GM->TogglePause(); Aim();
    TestTrue(TEXT("Valid F grants shotgun"),I->TryInteract()); TestTrue(TEXT("Granted gun consumed"),Gun->IsConsumed());
    TestEqual(TEXT("Two owned weapons"),W->GetWeaponCount(),2); TestEqual(TEXT("Pickup keeps current rifle"),W->GetEquippedSlot(),0);
    TestFalse(TEXT("Repeated F cannot grant consumed item"),I->TryInteract());
    auto* Duplicate=SpawnGun(FVector(160,0,64)); Aim(); TestFalse(TEXT("Duplicate gun rejected"),I->TryInteract()); TestFalse(TEXT("Duplicate remains in scene"),Duplicate->IsConsumed()); Duplicate->Destroy();
    // 使用非当前武器缺弹情景，确认同时补满两个槽位的数值而不改当前武器。
    W->EquipWeapon(1); W->StartFiring(); W->StopFiring(); Advance(); Advance(); Advance(); W->EquipWeapon(0);
    TestEqual(TEXT("Holstered shotgun spent one shell"),W->GetMagazineForWeapon(Shotgun),7);
    auto* Shells=SpawnAmmo(Shotgun,20); Aim(); TestTrue(TEXT("Refill holstered weapon"),I->TryInteract());
    TestEqual(TEXT("Holstered magazine restored"),W->GetMagazineForWeapon(Shotgun),8);
    TestEqual(TEXT("Holstered reserve restored to configured cap"),W->GetReserveForWeapon(Shotgun),40);
    TestEqual(TEXT("Rifle reserve unaffected"),W->GetReserveAmmo(),90);
    auto* Full=SpawnAmmo(Shotgun,8); Aim(); TestFalse(TEXT("Both full rejects F"),I->TryInteract()); TestFalse(TEXT("Full pickup remains"),Full->IsConsumed()); Full->Destroy();
    TestTrue(TEXT("Equip acquired shotgun"),W->EquipWeapon(1)); TestEqual(TEXT("Switch preserves refilled reserve"),W->GetReserveAmmo(),40);
    W->EquipWeapon(0); auto* Bullets=SpawnAmmo(Rifle,30); Aim(); TestTrue(TEXT("Magazine full but reserve short can refill"),I->TryInteract()); TestEqual(TEXT("Active reserve restored to test cap"),W->GetReserveAmmo(),100);
    auto EmptyMagazine=[&]()
    {
        W->StartFiring(); for(int32 Step=0;Step<240&&W->GetCurrentAmmo()>0;++Step) { Advance(); } W->StopFiring();
        TestEqual(TEXT("Real GAS fire empties rifle magazine"),W->GetCurrentAmmo(),0);
    };
    EmptyMagazine(); TestEqual(TEXT("Reserve remains full while magazine empty"),W->GetReserveAmmo(),100);
    auto* EmptyClip=SpawnAmmo(Rifle,30); Aim(); TestTrue(TEXT("Empty magazine with full reserve still offers pickup"),I->GetPrompt().ToString().Contains(TEXT("F")));
    TestTrue(TEXT("Empty magazine refills via actual pickup"),I->TryInteract()); TestEqual(TEXT("Magazine restored"),W->GetCurrentAmmo(),30); TestEqual(TEXT("Reserve stays at cap"),W->GetReserveAmmo(),100);
    // 真正打光弹匣与备用再领取，覆盖用户反馈；不直接写内部弹药字段。
    for(int32 Clip=0;Clip<5;++Clip)
    {
        EmptyMagazine(); if(W->GetReserveAmmo()<=0) { break; }
        W->StartReloading(); for(int32 Step=0;Step<35;++Step) { Advance(); }
    }
    TestEqual(TEXT("All reserve spent"),W->GetReserveAmmo(),0);
    auto* EmptyBoth=SpawnAmmo(Rifle,30); Aim(); TestTrue(TEXT("Zero magazine and zero reserve can refill"),I->TryInteract());
    TestEqual(TEXT("Empty rifle magazine restored"),W->GetCurrentAmmo(),30); TestEqual(TEXT("Empty rifle reserve restored"),W->GetReserveAmmo(),100);
    TestFalse(TEXT("Refilled pickup cannot grant twice"),I->TryInteract());
    W->StartFiring(); W->StopFiring(); Advance(); Advance(); Advance();
    TestTrue(TEXT("Reload begins before pickup"),W->StartReloading());
    auto* DuringReload=SpawnAmmo(Rifle,30); Aim(); TestTrue(TEXT("Pickup during reload succeeds"),I->TryInteract()); TestFalse(TEXT("Old reload cancelled"),W->IsReloading());
    for(int32 Step=0;Step<40;++Step) { Advance(); }
    TestEqual(TEXT("No delayed reload reserve deduction"),W->GetReserveAmmo(),100); TestEqual(TEXT("Magazine remains full after old timer"),W->GetCurrentAmmo(),30);
    auto* FullRifle=SpawnAmmo(Rifle,30); Aim(); TestFalse(TEXT("Full rifle does not consume another box"),I->TryInteract()); TestFalse(TEXT("Unused box preserved"),FullRifle->IsConsumed()); FullRifle->Destroy();
    TestEqual(TEXT("Overflow reserve input still rejected"),W->TryAddReserveAmmo(Rifle,MAX_int32),0);
    TestEqual(TEXT("Negative reserve input still rejected"),W->TryAddReserveAmmo(Rifle,-10),0);
    auto* Unknown=NewObject<UShooterWeaponDefinition>(World); auto* UnknownAmmo=SpawnAmmo(Unknown,20); Aim();
    TestFalse(TEXT("Unowned ammo rejected"),I->TryInteract()); TestFalse(TEXT("Unowned ammo pickup preserved"),UnknownAmmo->IsConsumed()); UnknownAmmo->Destroy();
    auto* Fresh=World->SpawnActorDeferred<AMyShooter>(AMyShooter::StaticClass(),FTransform(FVector(0,1000,0)));
    Fresh->GetShooterWeapon()->ConfigureLoadout({Rifle,Shotgun},1); Fresh->FinishSpawning(FTransform(FVector(0,1000,0)));
    TestEqual(TEXT("New player resets inventory"),Fresh->GetShooterWeapon()->GetWeaponCount(),1); TestEqual(TEXT("Shared DA unchanged"),Rifle->InitialReserveAmmo,90);
    auto* Last=SpawnGun(FVector(160,0,64)); UShooterDamageLibrary::ApplyGASDamage(GM,Player,1000,GM,FHitResult());
    TestFalse(TEXT("Dead player cannot interact"),I->TryInteract()); TestFalse(TEXT("Death does not consume item"),Last->IsConsumed());
    return true;
}
#endif
