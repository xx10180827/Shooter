#include "Tests/ShooterWeaponSmokeSubsystem.h"
#include "Characters/MyShooter.h"
#include "AI/ShooterAIController.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Weapons/ShooterWeaponPresentationComponent.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Weapons/ShooterAimComponent.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Animation/AnimInstance.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimMontage.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Game/ShooterGameMode.h"
#include "Player/ShooterPlayerController.h"
#include "UI/ShooterAmmoWidget.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterWeaponSmoke,Log,All);
void UShooterWeaponSmokeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("ShooterMouseSettingsSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ShooterDashSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ShooterWeaponSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("ShooterPickupSmoke")))
    {
        bMouseSettings=FParse::Param(FCommandLine::Get(),TEXT("ShooterMouseSettingsSmoke"));
        bDash=FParse::Param(FCommandLine::Get(),TEXT("ShooterDashSmoke"));
        bPickup=FParse::Param(FCommandLine::Get(),TEXT("ShooterPickupSmoke"));
        bPolish=FParse::Param(FCommandLine::Get(),TEXT("ShooterShotgunPolishSmoke"));
        Output=FPaths::ProjectSavedDir()/(bDash?TEXT("T17_AirDash/MapSmoke"):bPickup?TEXT("T15_PickupRing/MapSmoke"):(bPolish?TEXT("T14_Revision/AfterIK"):TEXT("T14/MapSmoke"))); IFileManager::Get().MakeDirectory(*Output,true);
        if(bMouseSettings) { Output=FPaths::ProjectSavedDir()/(FParse::Param(FCommandLine::Get(),TEXT("ShooterMouseSettingsVerify"))?TEXT("T19_Mouse/Verify"):TEXT("T19_Mouse/Write")); IFileManager::Get().MakeDirectory(*Output,true); }
        Deadline=FPlatformTime::Seconds()+180;
        TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UShooterWeaponSmokeSubsystem::Step),.01f);
    }
#endif
}
void UShooterWeaponSmokeSubsystem::Deinitialize() { FTSTicker::GetCoreTicker().RemoveTicker(TickHandle); Super::Deinitialize(); }
bool UShooterWeaponSmokeSubsystem::Check(bool bOK,const TCHAR* Message)
{
    if(!bOK) { Finish(false,Message); return false; }
    Results.Add(FString(TEXT("PASS: "))+Message); UE_LOG(LogShooterWeaponSmoke,Display,TEXT("PASS: %s"),Message); return true;
}
void UShooterWeaponSmokeSubsystem::Finish(bool bOK,const FString& Message)
{
    if(bFinished) { return; } bFinished=true;
    FFileHelper::SaveStringToFile((bOK?TEXT("PASS\n"):TEXT("FAIL\n"))+Message+TEXT("\n")+FString::Join(Results,TEXT("\n")),*(Output/TEXT("Result.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogShooterWeaponSmoke,Display,TEXT("%s: %s"),bOK?TEXT("PASS"):TEXT("FAIL"),*Message);
    FPlatformMisc::RequestExitWithStatus(false,bOK?0:1);
}
void UShooterWeaponSmokeSubsystem::Next(int32 NewPhase,float Delay) { Phase=NewPhase; NextTime=FPlatformTime::Seconds()+Delay; }
void UShooterWeaponSmokeSubsystem::Capture(const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Output/Name,true,false); }
bool UShooterWeaponSmokeSubsystem::Step(float DeltaTime)
{
    if(bMouseSettings) { return StepMouseSettings(DeltaTime); }
    if(bDash) { return StepDash(DeltaTime); }
    if(bPickup) { return StepPickup(DeltaTime); }
    if(bFinished) { return false; }
    const double Now=FPlatformTime::Seconds();
    if(Now>Deadline) { Finish(false,FString::Printf(TEXT("Timeout phase %d"),Phase)); return false; }
    if(Now<NextTime) { return true; }
    auto* World=GetWorld(); auto* PC=World?Cast<AShooterPlayerController>(World->GetFirstPlayerController()):nullptr;
    auto* Player=PC?Cast<AMyShooter>(PC->GetPawn()):nullptr;
    auto* GM=World?Cast<AShooterGameMode>(World->GetAuthGameMode()):nullptr;
    if(!World||World->bIsTearingDown||!World->HasBegunPlay()||!Player||!GM||!Player->IsGASInitialized()) { return true; }
    auto* W=Player->GetShooterWeapon(); auto* Aim=Player->GetShooterAim();
    auto* Presentation=Player->FindComponentByClass<UShooterWeaponPresentationComponent>();
    auto Press=[PC](FKey Key) { PC->InputKey(FInputKeyParams(Key,IE_Pressed,1.)); };
    auto Release=[PC](FKey Key) { PC->InputKey(FInputKeyParams(Key,IE_Released,0.)); };
    auto GripAlpha=[Player]()
    {
        auto* Anim=Player->GetMesh()->GetAnimInstance();
        auto* Property=FindFProperty<FFloatProperty>(Anim->GetClass(),TEXT("ShooterGripAlpha"));
        return Property?Property->GetPropertyValue_InContainer(Anim):-1.f;
    };
    auto CheckGrip=[&](const TCHAR* Stage)
    {
        if(!bPolish) { return true; }
        const auto* Definition=W->GetWeaponDefinition();
        if(!Check(Definition&&Definition->bUseLeftHandIK,TEXT("Shotgun has saved grip configuration"))) { return false; }
        const FVector Target=Player->GetMesh()->GetSocketTransform(TEXT("Right_Weapon")).TransformPosition(Definition->LeftHandGripLocation);
        const float Error=FVector::Distance(Target,Player->GetMesh()->GetSocketLocation(TEXT("b_LeftHand")));
        UE_LOG(LogShooterWeaponSmoke,Display,TEXT("GRIP stage=%s error=%.3fcm alpha=%.3f"),Stage,Error,GripAlpha());
        return Check(Error<1.0f&&GripAlpha()>.95f,Stage);
    };
    switch(Phase)
    {
    case 0:
        if(!Check(W->GetWeaponCount()>=1&&W->GetEquippedSlot()==0,TEXT("Saved player starts on rifle"))
            ||!Check(!W->EquipWeapon(1),TEXT("Menu blocks switching"))||!Check(GM->StartRound(),TEXT("Start round"))) { return false; }
        // T15 后默认只有步枪；旧武器专项显式授予霰弹枪，拾取路径由独立专项覆盖。
        if(W->GetWeaponCount()==1)
        {
            auto* Shotgun=LoadObject<UShooterWeaponDefinition>(nullptr,TEXT("/Game/Weapons/Data/DA_Shotgun.DA_Shotgun"));
            if(!Check(W->TryGrantWeapon(Shotgun),TEXT("Weapon-only smoke grants shotgun explicitly"))) { return false; }
        }
        for(TActorIterator<AShooterAIController> It(World);It;++It)
        { It->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(*It); if(auto* P=Cast<ACharacter>(It->GetPawn())) { P->GetCharacterMovement()->DisableMovement(); } }
        Player->GetCharacterMovement()->DisableMovement(); PC->SetControlRotation(FRotator(0,0,0)); Next(1,12.f); break;
    case 1:
        Capture(TEXT("01-Rifle.png")); Next(100,.25f); break;
    case 100: Press(EKeys::Two); Next(2); break;
    case 2:
    {
        Release(EKeys::Two);
        UStaticMeshComponent* Mesh=nullptr;
        TInlineComponentArray<UActorComponent*> AllComponents(Player);
        for(auto* C : AllComponents)
        {
            UE_LOG(LogShooterWeaponSmoke,Display,TEXT("COMPONENT %s class=%s begun=%d"),*C->GetName(),*C->GetClass()->GetName(),C->HasBegunPlay());
            if(auto* Static=Cast<UStaticMeshComponent>(C))
            {
                UE_LOG(LogShooterWeaponSmoke,Display,TEXT("STATIC mesh=%s visible=%d"),*GetNameSafe(Static->GetStaticMesh()),Static->IsVisible());
                if(C->GetFName()==TEXT("EquippedWeaponMesh")) { Mesh=Static; }
            }
        }
        if(!Check(W->GetEquippedSlot()==1&&W->GetCurrentAmmo()==8&&W->GetReserveAmmo()==32,TEXT("Actual key 2 selects shotgun 8/32"))
            ||!Check(Mesh&&Mesh->GetStaticMesh()==W->GetWeaponDefinition()->WeaponMesh&&Mesh->IsVisible(),TEXT("User imported FBX visible on player"))
            ||!Check(PC->GetAmmoWidget()&&PC->GetAmmoWidget()->GetDisplayedAmmo()==8,TEXT("HUD follows equipped slot"))) { return false; }
        auto* Camera=Player->FindComponentByClass<UCameraComponent>();
        TInlineComponentArray<USceneComponent*> Components(Player);
        for(auto* C : Components)
        { if(C->GetFName()==TEXT("Weapon_mesh")||C->GetFName()==TEXT("Muzzle")||C->GetFName()==TEXT("EquippedWeaponMesh"))
            { UE_LOG(LogShooterWeaponSmoke,Display,TEXT("HIP %s transform=%s cameraLocal=%s"),*C->GetName(),*C->GetRelativeTransform().ToString(),*Camera->GetComponentTransform().InverseTransformPosition(C->GetComponentLocation()).ToString()); } }
        if(!CheckGrip(TEXT("Idle hand reaches configured fore-end support"))) { return false; }
        Capture(TEXT("02-Shotgun.png")); Next(101,.25f); break;
    }
    case 101: Press(EKeys::RightMouseButton); Next(3,.5f); break;
    case 3:
        Release(EKeys::RightMouseButton);
        if(!Check(Aim->IsAiming()&&Aim->GetAimAlpha()>.99f,TEXT("Shotgun toggle ADS works"))) { return false; }
        Capture(TEXT("03-ShotgunADS.png")); Next(4); break;
    case 4:
        PitchBefore=FRotator::NormalizeAxis(PC->GetControlRotation().Pitch);
        Press(EKeys::LeftMouseButton); Next(5,.04f); break;
    case 5:
        if(!Check(W->GetCurrentAmmo()==7&&!W->IsFiring(),TEXT("Actual click costs one shotgun shell and ends semi-auto ability"))
            ||!Check(Player->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Aim->GetFireMontage()),TEXT("Data-configured aimed firing montage plays"))) { return false; }
        Capture(TEXT("04-ShotgunFire.png")); Next(6,1.f); break;
    case 6:
        if(bPolish&&!Check(FMath::IsNearlyEqual(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch)-PitchBefore,1.56f,.08f),TEXT("Actual ADS input raises viewpoint by one 1.56 degree kick"))) { return false; }
        if(!Check(W->GetCurrentAmmo()==7,TEXT("Held mouse does not repeat shotgun fire"))) { return false; }
        Release(EKeys::LeftMouseButton); Press(EKeys::R); Next(7); break;
    case 7:
        Release(EKeys::R);
        if(!Check(W->IsReloading()&&!Aim->IsAiming(),TEXT("Reload starts and clears ADS"))
            ||!Check(Player->GetMesh()->GetAnimInstance()->Montage_IsPlaying(W->GetReloadMontage()),TEXT("Configured reload montage plays via preserved blueprint"))) { return false; }
        if(bPolish&&!Check(Presentation&&Presentation->IsReloadSoundPlaying(),TEXT("Shotgun reload sound is playing on actual audio device"))) { return false; }
        if(bPolish&&!Check(GripAlpha()==0.f,TEXT("Reload releases left hand IK"))) { return false; }
        Capture(TEXT("05-ShotgunReload.png")); Next(102,.25f); break;
    case 102: Press(EKeys::One); Next(8); break;
    case 8:
        Release(EKeys::One);
        if(!Check(W->GetEquippedSlot()==0&&!W->IsReloading()&&!Aim->IsAiming(),TEXT("Key 1 cancels reload and selects rifle"))) { return false; }
        if(bPolish&&!Check(!Presentation->IsReloadSoundPlaying(),TEXT("Switch immediately stops shotgun reload sound"))) { return false; }
        if(bPolish&&!Check(GripAlpha()==0.f,TEXT("Rifle bypasses shotgun hand IK"))) { return false; }
        Capture(TEXT("06-BackToRifle.png")); Next(103,.25f); break;
    case 103: Press(EKeys::Two); Next(9,2.7f); break;
    case 9:
        Release(EKeys::Two);
        if(!Check(W->GetEquippedSlot()==1&&W->GetCurrentAmmo()==7&&W->GetReserveAmmo()==32,TEXT("Old reload callback cannot refill shotgun after switching back"))) { return false; }
        Press(EKeys::R); Next(10,.1f); break;
    case 10:
        Release(EKeys::R); Next(11,2.6f); break;
    case 11:
        if(!Check(W->GetCurrentAmmo()==8&&W->GetReserveAmmo()==31&&!W->IsReloading(),TEXT("New shotgun reload completes conserving ammo"))) { return false; }
        if(bPolish&&!Check(!Presentation->IsReloadSoundPlaying(),TEXT("Completed reload has no lingering sound"))) { return false; }
        GM->TogglePause();
        if(!Check(!W->EquipWeapon(0),TEXT("Pause blocks switch"))) { return false; }
        GM->TogglePause(); Press(EKeys::One); Next(12); break;
    case 12:
        Release(EKeys::One); Press(EKeys::LeftMouseButton); Next(13,.35f); break;
    case 13:
        if(!Check(W->IsFiring()&&W->GetCurrentAmmo()<29,TEXT("Rifle still supports held automatic fire"))) { return false; }
        AmmoBefore=W->GetCurrentAmmo(); Press(EKeys::Two); Next(14,.2f); break;
    case 14:
        Release(EKeys::Two); Release(EKeys::LeftMouseButton);
        if(!Check(W->GetEquippedSlot()==1&&!W->IsFiring()&&W->GetCurrentAmmo()==8,TEXT("Switch during held fire does not fire new gun"))) { return false; }
        Press(EKeys::One); Next(15,.7f); break;
    case 15:
        Release(EKeys::One);
        if(!Check(!W->IsFiring()&&W->GetCurrentAmmo()>=AmmoBefore-1,TEXT("Old rifle loop stopped after switch"))) { return false; }
        if(bPolish) { Press(EKeys::Two); Next(16,.3f); break; }
        Finish(true,TEXT("Saved assets, 1/2 input, HUD, shotgun semi-auto, ADS, configured animations, reload cancel/completion and held-fire switch passed.")); return false;
    // 霰弹枪扩展验收：真实按键、音频设备及 GAS 死亡，检查取消后的声音和弹药。
    case 16:
        Release(EKeys::Two);
        if(auto* Camera=Player->FindComponentByClass<UCameraComponent>())
        {
            CameraBefore=Camera->GetRelativeTransform(); SavedCameraFOV=Camera->FieldOfView;
            const FVector Center=Player->GetMesh()->GetSocketLocation(TEXT("Right_Weapon"))+Player->GetActorForwardVector()*20.f;
            Camera->bUsePawnControlRotation=false; Camera->SetFieldOfView(65.f);
            Camera->SetWorldLocation(Center+FVector(0,-110,22)); Camera->SetWorldRotation((Center-Camera->GetComponentLocation()).Rotation());
        }
        Capture(TEXT("07-SideGripIdle.png")); Next(161,.2f); break;
    case 161:
        Press(EKeys::LeftMouseButton); Next(17,.08f); break;
    case 17:
        Release(EKeys::LeftMouseButton);
        if(!Check(Player->GetMesh()->GetAnimInstance()->Montage_IsPlaying(W->GetWeaponDefinition()->FireMontage),TEXT("Independent shotgun hip-fire montage plays"))) { return false; }
        if(!CheckGrip(TEXT("Firing hand stays at fore-end support"))) { return false; }
        Capture(TEXT("08-SideGripFire.png")); Next(117,.2f); break;
    case 117:
        if(!CheckGrip(TEXT("Recovering hand stays at fore-end support"))) { return false; }
        Capture(TEXT("09-SideGripRecovery.png")); Next(118,.2f); break;
    case 118:
        if(auto* Camera=Player->FindComponentByClass<UCameraComponent>())
        { Camera->SetRelativeTransform(CameraBefore); Camera->bUsePawnControlRotation=true; Camera->SetFieldOfView(SavedCameraFOV); }
        Press(EKeys::One); Next(18,.12f); break;
    case 18:
    {
        Release(EKeys::One);
        auto* OldMontage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Animations/Shotgun/Correction/AM_ShotgunFire.AM_ShotgunFire"));
        if(!Check(OldMontage&&!Player->GetMesh()->GetAnimInstance()->Montage_IsPlaying(OldMontage),TEXT("Switch stops old shotgun recoil montage"))) { return false; }
        Press(EKeys::Two); Next(19,.3f); break;
    }
    case 19:
        Release(EKeys::Two); Press(EKeys::R); Next(20,.25f); break;
    case 20:
        Release(EKeys::R);
        if(!Check(W->IsReloading()&&Presentation->IsReloadSoundPlaying(),TEXT("Reload sound starts before pause"))) { return false; }
        AmmoBefore=W->GetCurrentAmmo(); GM->TogglePause(); Next(21,.15f); break;
    case 21:
        if(!Check(!W->IsReloading()&&!Presentation->IsReloadSoundPlaying(),TEXT("Pause cancels reload and stops its sound"))) { return false; }
        GM->TogglePause(); Next(22,2.6f); break;
    case 22:
        if(!Check(W->GetCurrentAmmo()==AmmoBefore,TEXT("Paused reload does not refill after resuming"))) { return false; }
        Press(EKeys::R); Next(23,.25f); break;
    case 23:
        Release(EKeys::R);
        if(!Check(W->IsReloading()&&Presentation->IsReloadSoundPlaying(),TEXT("Reload sound starts before lethal GAS damage"))) { return false; }
        UShooterDamageLibrary::ApplyGASDamage(GM,Player,1000,nullptr,FHitResult()); Next(24,.1f); break;
    case 24:
        if(!Check(Player->HasGASDeathStarted()&&!W->IsReloading()&&!Presentation->IsReloadSoundPlaying(),TEXT("Death cancels reload and stops its sound"))
            ||!Check(W->GetCurrentAmmo()==AmmoBefore,TEXT("Death cancellation does not grant ammo"))) { return false; }
        Finish(true,TEXT("Shotgun saved assets, input, hip/ADS montage, audio playback and switch/pause/death/completion cleanup passed.")); return false;
    }
    return true;
}
