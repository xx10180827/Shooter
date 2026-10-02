// 验证真实控制器转向、鼠标压枪叠加、弹丸次数、空枪与状态中断。
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
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Weapons/ShooterAimComponent.h"
#include "Weapons/ShooterRecoilComponent.h"
#include "Game/ShooterGameMode.h"
#include "Combat/ShooterDamageLibrary.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterRecoilTest,"MyShoot.Weapons.RealViewRecoil",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShooterRecoilTest::RunTest(const FString& Parameters)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    auto* GI=NewObject<UGameInstance>(GEngine); World->SetGameInstance(GI); Context.OwningGameInstance=GI;
    const uint64 SavedFrame=GFrameCounter;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter=SavedFrame; };
    World->GetWorldSettings()->DefaultGameMode=AShooterGameMode::StaticClass(); World->SetGameMode(FURL()); World->InitializeActorsForPlay(FURL());
    auto* Shotgun=NewObject<UShooterWeaponDefinition>(World); Shotgun->RecoilPitch=3.f; Shotgun->RecoilKickDuration=.06f; Shotgun->AimRecoilMultiplier=.5f;
    Shotgun->MagazineCapacity=4; Shotgun->PelletCount=8; Shotgun->FireInterval=.01f; Shotgun->bAutomatic=false;
    // 使用磁盘步枪配置验证自动连射；复制到临时对象，测试不写回数据资产。
    auto* RifleAsset=LoadObject<UShooterWeaponDefinition>(nullptr,TEXT("/Game/Weapons/Data/DA_Rifle.DA_Rifle"));
    if(!TestNotNull(TEXT("Rifle data asset exists"),RifleAsset)) { return false; }
    auto* Other=DuplicateObject<UShooterWeaponDefinition>(RifleAsset,World);
    Other->BulletVisualClass=nullptr; Other->FireSound=nullptr; Other->ReloadSound=nullptr;
    Other->FireMontage=nullptr; Other->AimFireMontage=nullptr; Other->ReloadMontage=nullptr;
    auto* Player=World->SpawnActorDeferred<AMyShooter>(AMyShooter::StaticClass(),FTransform::Identity);
    Player->GetShooterWeapon()->ConfigureLoadout({Shotgun,Other});
    auto* Camera=NewObject<UCameraComponent>(Player); Camera->SetupAttachment(Player->GetRootComponent()); Camera->bUsePawnControlRotation=true; Camera->RegisterComponent();
    Player->FinishSpawning(FTransform::Identity);
    auto* PC=World->SpawnActor<APlayerController>();
    // CameraManager 只为绑定本地玩家的控制器更新视角，模拟实际游戏的玩家连接。
    PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Player); PC->SetViewTarget(Player);
    World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay(); Player->GetCharacterMovement()->DisableMovement();
    auto* GM=CastChecked<AShooterGameMode>(World->GetAuthGameMode()); GM->StartRound();
    auto* W=Player->GetShooterWeapon(); auto* Recoil=Player->FindComponentByClass<UShooterRecoilComponent>();
    auto Advance=[World](float Seconds) { while(Seconds>KINDA_SMALL_NUMBER) { const float Step=FMath::Min(Seconds,.01f); ++GFrameCounter; World->Tick(LEVELTICK_All,Step); Seconds-=Step; } };
    auto Pitch=[PC]() { return FRotator::NormalizeAxis(PC->GetControlRotation().Pitch); };
    TestTrue(TEXT("Rifle has enabled recoil"),Other->RecoilPitch>0.f);
    TestTrue(TEXT("Rifle remains automatic"),Other->bAutomatic);
    TestTrue(TEXT("Equip rifle for continuous recoil"),W->EquipWeapon(1));
    PC->SetControlRotation(FRotator::ZeroRotator);
    const int32 BeforeBurst=W->GetCurrentAmmo();
    TestTrue(TEXT("Hold rifle trigger"),W->StartFiring()); Advance(.35f);
    W->StopFiring(); Advance(.1f);
    const int32 BurstShots=BeforeBurst-W->GetCurrentAmmo();
    TestTrue(TEXT("Holding fires multiple rounds"),BurstShots>=3);
    TestTrue(TEXT("Each round adds one recoil increment"),FMath::IsNearlyEqual(Pitch(),BurstShots*Other->RecoilPitch,.02f));
    const float ReleasedPitch=Pitch(); const int32 ReleasedAmmo=W->GetCurrentAmmo(); Advance(.3f);
    TestEqual(TEXT("Release stops automatic ammo use"),W->GetCurrentAmmo(),ReleasedAmmo);
    TestTrue(TEXT("Release settles without auto return or continued drift"),FMath::IsNearlyEqual(Pitch(),ReleasedPitch,.02f));
    Player->GetShooterAim()->StartAiming(); PC->SetControlRotation(FRotator::ZeroRotator);
    const int32 BeforeAim=W->GetCurrentAmmo();
    TestTrue(TEXT("ADS rifle burst"),W->StartFiring()); Advance(.15f);
    auto AimRotation=PC->GetControlRotation(); AimRotation.Pitch-=.2f; PC->SetControlRotation(AimRotation);
    W->StopFiring(); Advance(.1f);
    const float ExpectedAim=(BeforeAim-W->GetCurrentAmmo())*Other->RecoilPitch*Other->AimRecoilMultiplier-.2f;
    TestTrue(TEXT("ADS burst preserves manual compensation"),FMath::IsNearlyEqual(Pitch(),ExpectedAim,.02f));
    PC->PlayerCameraManager->UpdateCamera(.01f);
    FVector RifleViewLocation; FRotator RifleViewRotation; PC->GetPlayerViewPoint(RifleViewLocation,RifleViewRotation);
    TestTrue(TEXT("Rifle actual camera follows recoil"),FMath::IsNearlyEqual(FRotator::NormalizeAxis(RifleViewRotation.Pitch),ExpectedAim,.02f));
    AddInfo(FString::Printf(TEXT("Rifle burst shots=%d hip=%.4f ADS compensated=%.4f camera=%.4f"),BurstShots,ReleasedPitch,Pitch(),FRotator::NormalizeAxis(RifleViewRotation.Pitch)));
    Player->GetShooterAim()->ResetAiming(); W->EquipWeapon(0);
    PC->SetControlRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Successful blast"),W->StartFiring());
    TestTrue(TEXT("Eight pellets queue one three-degree kick"),FMath::IsNearlyEqual(Recoil->GetPendingPitch(),3.f));
    W->StartFiring(); TestEqual(TEXT("Cooldown does not consume another shell"),W->GetCurrentAmmo(),3);
    W->StopFiring(); // 松开按钮取消等待射速间隔的能力，不取消已经成功发射的后坐力。
    Advance(.03f);
    // 模拟玩家在后坐力未完成时主动向下压 1 度；组件不得覆盖这次输入。
    auto Rotation=PC->GetControlRotation(); Rotation.Pitch-=1; PC->SetControlRotation(Rotation);
    Advance(.1f); TestTrue(TEXT("Recoil adds to player compensation"),FMath::IsNearlyEqual(Pitch(),2.f,.02f));
    // 独立 UWorld 测试不经过 UGameEngine 的相机阶段，显式执行与真实游戏相同的相机更新。
    PC->PlayerCameraManager->UpdateCamera(.01f);
    FVector View; FRotator ViewRotation; PC->GetPlayerViewPoint(View,ViewRotation);
    AddInfo(FString::Printf(TEXT("ControllerPitch=%.3f ViewPitch=%.3f LocalPlayer=%d CameraCacheTime=%.3f"),Pitch(),FRotator::NormalizeAxis(ViewRotation.Pitch),PC->IsLocalPlayerController(),PC->PlayerCameraManager->GetCameraCacheTime()));
    TestTrue(TEXT("Actual viewpoint used by next trace is raised"),FMath::IsNearlyEqual(FRotator::NormalizeAxis(ViewRotation.Pitch),Pitch(),.02f));
    Player->GetShooterAim()->StartAiming(); PC->SetControlRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("ADS blast"),W->StartFiring()); Advance(.1f);
    TestTrue(TEXT("ADS multiplier reduces real recoil"),FMath::IsNearlyEqual(Pitch(),1.5f,.02f));
    Player->GetShooterAim()->ResetAiming();
    TestTrue(TEXT("Third blast"),W->StartFiring()); TestTrue(TEXT("Switch while recoil pending"),W->EquipWeapon(1));
    const float SwitchedPitch=Pitch(); Advance(.1f); TestTrue(TEXT("Switch cancels pending kick without snapping view"),FMath::IsNearlyEqual(Pitch(),SwitchedPitch,.02f));
    W->EquipWeapon(0); PC->SetControlRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Last shell"),W->StartFiring()); Advance(.1f);
    const float EmptyPitch=Pitch(); TestFalse(TEXT("Empty gun cannot fire"),W->StartFiring()); Advance(.1f);
    TestTrue(TEXT("Empty click has no recoil"),FMath::IsNearlyEqual(Pitch(),EmptyPitch,.02f));
    W->StartReloading(); Advance(1.7f);
    TestTrue(TEXT("Fire after reload"),W->StartFiring()); GM->TogglePause();
    TestEqual(TEXT("Pause immediately clears pending kick"),Recoil->GetPendingPitch(),0.f); GM->TogglePause(); Advance(.1f);
    TestTrue(TEXT("No delayed recoil after resume"),FMath::IsNearlyEqual(Pitch(),EmptyPitch,.02f));
    W->StartFiring(); W->StartReloading();
    TestEqual(TEXT("Reload cancels pending kick"),Recoil->GetPendingPitch(),0.f);
    W->CancelReloading(); Advance(.1f); W->StartFiring();
    UShooterDamageLibrary::ApplyGASDamage(GM,Player,1000,GM,FHitResult());
    TestEqual(TEXT("Death cancels pending kick"),Recoil->GetPendingPitch(),0.f);
    return true;
}
#endif
