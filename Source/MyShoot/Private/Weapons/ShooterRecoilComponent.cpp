#include "Weapons/ShooterRecoilComponent.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Weapons/ShooterAimComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

UShooterRecoilComponent::UShooterRecoilComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bStartWithTickEnabled=false;
}
void UShooterRecoilComponent::BeginPlay()
{
    Super::BeginPlay();
    Character=Cast<AMyShooter>(GetOwner());
    if(!Character.IsValid()) { return; }
    Weapon=Character->GetShooterWeapon();
    if(Weapon.IsValid())
    {
        // 每发只广播一次；霰弹的八颗弹丸不重复增加八次后坐力。
        Weapon->OnShotFired.AddUniqueDynamic(this,&UShooterRecoilComponent::HandleShot);
        Weapon->OnWeaponChanged.AddUniqueDynamic(this,&UShooterRecoilComponent::HandleWeaponChanged);
        Weapon->OnReloadStarted.AddUniqueDynamic(this,&UShooterRecoilComponent::HandleReloadStarted);
    }
    Character->OnGASDeathConfirmed.AddUniqueDynamic(this,&UShooterRecoilComponent::HandleDeath);
    GameMode=Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
    if(GameMode.IsValid()) { GameMode->OnRoundChanged.AddUniqueDynamic(this,&UShooterRecoilComponent::HandleRoundChanged); }
}
void UShooterRecoilComponent::HandleShot(bool bBlockingHit,const FHitResult& Hit)
{
    auto* PC=Character.IsValid()?Cast<APlayerController>(Character->GetController()):nullptr;
    const auto* Definition=Weapon.IsValid()?Weapon->GetWeaponDefinition():nullptr;
    if(!PC||!PC->IsLocalController()||!Definition||Character->HasGASDeathStarted()||!AShooterGameMode::IsCombatAllowed(this)) { return; }
    const float Pitch=FMath::IsFinite(Definition->RecoilPitch)?FMath::Clamp(Definition->RecoilPitch,0.f,15.f):0.f;
    const bool bAiming=Character->GetShooterAim()&&Character->GetShooterAim()->IsAiming();
    const float Scale=bAiming&&FMath::IsFinite(Definition->AimRecoilMultiplier)?FMath::Clamp(Definition->AimRecoilMultiplier,0.f,1.f):1.f;
    PendingPitch=FMath::Min(PendingPitch+Pitch*Scale,20.f);
    RemainingTime=FMath::IsFinite(Definition->RecoilKickDuration)?FMath::Clamp(Definition->RecoilKickDuration,.01f,.2f):.06f;
    KickController=PC;
    SetComponentTickEnabled(PendingPitch>KINDA_SMALL_NUMBER);
}
void UShooterRecoilComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    auto* PC=KickController.Get();
    if(!PC||!Character.IsValid()||Character->GetController()!=PC||Character->HasGASDeathStarted()
        ||!AShooterGameMode::IsCombatAllowed(this)) { CancelPendingRecoil(); return; }
    const float Step=FMath::Min(FMath::Max(DeltaTime,0.f),RemainingTime);
    const float Delta=RemainingTime>SMALL_NUMBER?PendingPitch*Step/RemainingTime:PendingPitch;
    // 叠加到此刻朝向，不缓存开火前的绝对朝向；鼠标下压与上抬可以正常抵消。
    FRotator Rotation=PC->GetControlRotation();
    const float MinPitch=PC->PlayerCameraManager?PC->PlayerCameraManager->ViewPitchMin:-89.f;
    const float MaxPitch=PC->PlayerCameraManager?PC->PlayerCameraManager->ViewPitchMax:89.f;
    Rotation.Pitch=FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch)+Delta,MinPitch,MaxPitch);
    PC->SetControlRotation(Rotation);
    PendingPitch=FMath::Max(0.f,PendingPitch-Delta);
    RemainingTime=FMath::Max(0.f,RemainingTime-Step);
    if(PendingPitch<=KINDA_SMALL_NUMBER||RemainingTime<=SMALL_NUMBER) { CancelPendingRecoil(); }
}
void UShooterRecoilComponent::CancelPendingRecoil()
{
    PendingPitch=0; RemainingTime=0; KickController.Reset(); SetComponentTickEnabled(false);
}
void UShooterRecoilComponent::HandleWeaponChanged(int32 OldSlot,int32 NewSlot) { CancelPendingRecoil(); }
void UShooterRecoilComponent::HandleReloadStarted(float Duration) { CancelPendingRecoil(); }
void UShooterRecoilComponent::HandleDeath(AShooterCharacterBase* DeadCharacter) { CancelPendingRecoil(); }
void UShooterRecoilComponent::HandleRoundChanged(EShooterRoundState State)
{
    if(State!=EShooterRoundState::Playing) { CancelPendingRecoil(); }
}
void UShooterRecoilComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelPendingRecoil();
    if(Weapon.IsValid())
    {
        Weapon->OnShotFired.RemoveDynamic(this,&UShooterRecoilComponent::HandleShot);
        Weapon->OnWeaponChanged.RemoveDynamic(this,&UShooterRecoilComponent::HandleWeaponChanged);
        Weapon->OnReloadStarted.RemoveDynamic(this,&UShooterRecoilComponent::HandleReloadStarted);
    }
    if(Character.IsValid()) { Character->OnGASDeathConfirmed.RemoveDynamic(this,&UShooterRecoilComponent::HandleDeath); }
    if(GameMode.IsValid()) { GameMode->OnRoundChanged.RemoveDynamic(this,&UShooterRecoilComponent::HandleRoundChanged); }
    Super::EndPlay(Reason);
}
