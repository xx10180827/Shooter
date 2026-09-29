#include "Tests/ShooterAimSmokeSubsystem.h"
#include "Characters/MyShooter.h"
#include "AI/ShooterAIController.h"
#include "Weapons/ShooterAimComponent.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Animation/ShooterPlayerAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Game/ShooterGameMode.h"
#include "AbilitySystemComponent.h"
#include "GAS/ShooterGameplayTags.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Player/ShooterPlayerController.h"
#include "UI/ShooterAmmoWidget.h"
DEFINE_LOG_CATEGORY_STATIC(LogShooterAimSmoke,Log,All);

void UShooterAimSmokeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("ShooterAimSmoke")))
    {
        Output=FPaths::ProjectSavedDir()/TEXT("T10/Smoke");
        FParse::Value(FCommandLine::Get(),TEXT("ShooterAimSmokeOutput="),Output);
        IFileManager::Get().MakeDirectory(*Output,true); Deadline=FPlatformTime::Seconds()+180;
        TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UShooterAimSmokeSubsystem::Step),0.01f);
    }
#endif
}
void UShooterAimSmokeSubsystem::Deinitialize() { FTSTicker::GetCoreTicker().RemoveTicker(TickHandle); Super::Deinitialize(); }
bool UShooterAimSmokeSubsystem::Check(bool bOK,const TCHAR* Message)
{
    if(!bOK) { Finish(false,Message); return false; }
    Results.Add(FString(TEXT("PASS: "))+Message); UE_LOG(LogShooterAimSmoke,Display,TEXT("PASS: %s"),Message); return true;
}
void UShooterAimSmokeSubsystem::Finish(bool bOK,const FString& Message)
{
    if(bFinished) { return; } bFinished=true;
    FFileHelper::SaveStringToFile((bOK?TEXT("PASS\n"):TEXT("FAIL\n"))+Message+TEXT("\n")+FString::Join(Results,TEXT("\n")),*(Output/TEXT("AimSmoke.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogShooterAimSmoke,Display,TEXT("%s T10 smoke: %s"),bOK?TEXT("PASS"):TEXT("FAIL"),*Message);
    FPlatformMisc::RequestExitWithStatus(false,bOK?0:1);
}
void UShooterAimSmokeSubsystem::Next(int32 NewPhase,float Delay) { Phase=NewPhase; NextTime=FPlatformTime::Seconds()+Delay; }
void UShooterAimSmokeSubsystem::Capture(const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Output/Name,true,false); }
bool UShooterAimSmokeSubsystem::Step(float DeltaTime)
{
    if(bFinished) { return false; }
    const double Now=FPlatformTime::Seconds(); if(Now>Deadline) { Finish(false,FString::Printf(TEXT("Timeout phase %d"),Phase)); return false; }
    if(Now<NextTime) { return true; }
    UWorld* World=GetWorld();
    APlayerController* PC=World?World->GetFirstPlayerController():nullptr;
    AMyShooter* Player=PC?Cast<AMyShooter>(PC->GetPawn()):nullptr;
    AShooterGameMode* GM=World?Cast<AShooterGameMode>(World->GetAuthGameMode()):nullptr;
    if(!World||World->bIsTearingDown||!World->HasBegunPlay()||!Player||!GM||!Player->IsGASInitialized()) { return true; }
    UShooterAimComponent* Aim=Player->GetShooterAim(); UCameraComponent* Camera=Player->FindComponentByClass<UCameraComponent>();
    UShooterWeaponComponent* Weapon=Player->GetShooterWeapon(); UAbilitySystemComponent* ASC=Player->GetAbilitySystemComponent();
    auto Press=[PC]() { PC->InputKey(FInputKeyParams(EKeys::RightMouseButton,IE_Pressed,1.0)); };
    auto Release=[PC]() { PC->InputKey(FInputKeyParams(EKeys::RightMouseButton,IE_Released,0.0)); };
    switch(Phase)
    {
    case 0:
        if(!Check(Aim&&Camera&&!Aim->IsAiming(),TEXT("Fresh saved player has camera and idle aim component"))) { return false; }
        HipFOV=Camera->FieldOfView;
        if(!Check(!Aim->StartAiming(),TEXT("Actual menu blocks aiming"))||!Check(GM->StartRound(),TEXT("Start actual round"))) { return false; }
        for(TActorIterator<AShooterAIController> It(World);It;++It)
        {
            It->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(*It);
            if(AShooterCharacterBase* Candidate=Cast<AShooterCharacterBase>(It->GetPawn()))
            { Candidate->GetCharacterMovement()->DisableMovement(); if(!Enemy.IsValid()) { Enemy=Candidate; } }
        }
        Player->GetCharacterMovement()->DisableMovement(); Next(1,0.7f); break;
    case 1:
        {
            AShooterPlayerController* ShooterPC=Cast<AShooterPlayerController>(PC);
            if(!Check(ShooterPC && ShooterPC->GetAmmoWidget()
                && ShooterPC->GetAmmoWidget()->GetDisplayedAmmo()==Weapon->GetCurrentAmmo()
                && ShooterPC->GetAmmoWidget()->GetDisplayedReserve()==Weapon->GetReserveAmmo(),TEXT("Initial HUD matches initialized weapon ammo"))) { return false; }
        }
        Capture(TEXT("01-Hip.png")); Next(2); break;
    case 2:
        Press(); Next(3,0.5f); break;
    case 3:
        if(!Check(Aim->IsAiming()&&Aim->GetAimAlpha()>0.99f&&Camera->FieldOfView<HipFOV,TEXT("Right mouse input enters ADS with narrower camera FOV"))
            ||!Check(Player->GetMesh()->GetAnimInstance()->IsA<UShooterPlayerAnimInstance>(),TEXT("Saved player uses native aim-aware AnimBP"))) { return false; }
        {
            TInlineComponentArray<USceneComponent*> Components(Player);
            for (USceneComponent* C : Components)
            {
                if (C->GetFName()==TEXT("Muzzle") || C->GetFName()==TEXT("Weapon_mesh"))
                {
                    UE_LOG(LogShooterAimSmoke,Display,TEXT("ADS %s camera-local=%s rotation=%s"),*C->GetName(),
                        *Camera->GetComponentTransform().InverseTransformPosition(C->GetComponentLocation()).ToString(),
                        *C->GetComponentRotation().ToString());
                }
            }
            TInlineComponentArray<UShooterWeaponComponent*> Weapons(Player);
            for (UShooterWeaponComponent* W : Weapons)
            { UE_LOG(LogShooterAimSmoke,Display,TEXT("WEAPON %s ammo=%d pointerMatch=%d"),*W->GetPathName(),W->GetCurrentAmmo(),W==Weapon); }
            AShooterPlayerController* ShooterPC=Cast<AShooterPlayerController>(PC);
            UE_LOG(LogShooterAimSmoke,Display,TEXT("HUD initial=%d weapon=%d"),ShooterPC&&ShooterPC->GetAmmoWidget()?ShooterPC->GetAmmoWidget()->GetDisplayedAmmo():-1,Weapon->GetCurrentAmmo());
        }
        Capture(TEXT("02-ADS.png")); Release(); Next(4); break;
    case 4:
        if(!Check(Aim->IsAiming(),TEXT("Releasing right mouse keeps toggle ADS active"))) { return false; }
        Press(); Next(5); break;
    case 5:
        if(!Check(!Aim->IsAiming()&&FMath::IsNearlyEqual(Camera->FieldOfView,HipFOV),TEXT("Second right mouse press restores original view"))) { return false; }
        Release(); Next(6,0.1f); break;
    case 6:
        Press(); Next(7); break;
    case 7:
        Release(); AmmoBefore=Weapon->GetCurrentAmmo();
        if(!Check(Aim->IsAiming(),TEXT("Can enter ADS again"))||!Check(Weapon->StartFiring(),TEXT("Weapon fires while aiming"))) { return false; }
        Weapon->StopFiring();
        if(!Check(Weapon->GetCurrentAmmo()==AmmoBefore-1,TEXT("ADS consumes exactly one bullet"))
            ||!Check(Player->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Aim->GetFireMontage()),TEXT("ADS selects and plays aimed fire montage"))) { return false; }
        Capture(TEXT("03-AimFire.png")); Next(8); break;
    case 8:
        if(!Check(Weapon->StartReloading(),TEXT("Reload accepted from ADS"))
            ||!Check(!Aim->IsAiming()&&FMath::IsNearlyEqual(Camera->FieldOfView,HipFOV),TEXT("Reload immediately resets ADS and camera"))) { return false; }
        Next(9,Weapon->GetReloadDuration()+0.2f); break;
    case 9:
        if(!Check(!Aim->IsAiming()&&!Weapon->IsReloading(),TEXT("Reload completes without relatching ADS"))) { return false; }
        Aim->StartAiming(); GM->TogglePause();
        if(!Check(!Aim->IsAiming()&&!ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Aiming),TEXT("Pause clears aiming and GAS tag"))) { return false; }
        GM->TogglePause();
        if(!Check(!Aim->IsAiming(),TEXT("Resume keeps ADS off"))||!Check(Enemy.IsValid(),TEXT("Actual map has AI target"))) { return false; }
        Player->SetActorLocation(Enemy->GetActorLocation()+FVector(180,0,0),false);
        PC->SetControlRotation(FRotator(0,180,0)); Player->SetActorRotation(FRotator(0,180,0));
        if(AShooterAIController* AI=Cast<AShooterAIController>(Enemy->GetController()))
        {
            AI->UnPossess(); AI->Possess(Enemy.Get()); Enemy->GetCharacterMovement()->DisableMovement(); AI->SetCombatTarget(Player);
        }
        Next(10,0.01f); break;
    case 10:
        {
            AShooterBulletVisual* Found=nullptr;
            for(TActorIterator<AShooterBulletVisual> It(World);It;++It) { if(It->GetOwner()==Enemy.Get()) { Found=*It; break; } }
            if(!Found) { return true; }
            if(!Check(Found->GetBulletMesh()->GetCollisionEnabled()==ECollisionEnabled::NoCollision,TEXT("Actual AI spawns visible non-colliding bullet"))
                ||!Check(Player->GetGASHealth()<100.f,TEXT("AI bullet presentation accompanies existing GAS hit"))) { return false; }
            HealthAfterAIShot=Player->GetGASHealth();
            if(AShooterAIController* AI=Cast<AShooterAIController>(Enemy->GetController())) { AI->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(AI); }
            Capture(TEXT("04-AIBullet.png")); Next(11,0.7f);
        } break;
    case 11:
        if(!Check(Player->GetGASHealth()==HealthAfterAIShot,TEXT("AI bullet flight does not apply additional damage"))) { return false; }
        Aim->StartAiming(); Next(13); break;
    case 13:
        {
            TInlineComponentArray<USceneComponent*> Components(Player);
            for(USceneComponent* C:Components) { if(C->GetFName()==TEXT("Muzzle")) { FlatMuzzle=Camera->GetComponentTransform().InverseTransformPosition(C->GetComponentLocation()); } }
            UE_LOG(LogShooterAimSmoke,Display,TEXT("Player controller-pitch=%d camera-control=%d"),Player->bUseControllerRotationPitch,Camera->bUsePawnControlRotation);
            PC->SetControlRotation(FRotator(25,180,0)); Next(14);
        } break;
    case 14:
        {
            FVector PitchedMuzzle=FVector::ZeroVector;
            TInlineComponentArray<USceneComponent*> Components(Player);
            for(USceneComponent* C:Components) { if(C->GetFName()==TEXT("Muzzle")) { PitchedMuzzle=Camera->GetComponentTransform().InverseTransformPosition(C->GetComponentLocation()); } }
            if(!Check(FVector::Dist(FlatMuzzle,PitchedMuzzle)<4.f,TEXT("ADS weapon follows look pitch without drifting off screen"))) { return false; }
            Capture(TEXT("05-ADSPitch.png")); Next(12);
        } break;
    case 12:
        UShooterDamageLibrary::ApplyGASDamage(Enemy.Get(),Player,1000,Enemy.Get(),FHitResult());
        if(!Check(!Aim->IsAiming()&&FMath::IsNearlyEqual(Camera->FieldOfView,HipFOV)
            &&!ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Aiming),TEXT("Death restores view and clears ADS tag"))) { return false; }
        Finish(true,TEXT("Real right-button toggle, aimed animation/fire, reload/pause/death reset, AI cosmetic bullet and single damage passed.")); return false;
    }
    return true;
}
