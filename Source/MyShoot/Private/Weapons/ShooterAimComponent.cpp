#include "Weapons/ShooterAimComponent.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "AbilitySystemComponent.h"
#include "GAS/ShooterGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/ShooterPlayerAnimInstance.h"

UShooterAimComponent::UShooterAimComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}
void UShooterAimComponent::BeginPlay()
{
    Super::BeginPlay();
    AMyShooter* Owner = Cast<AMyShooter>(GetOwner()); OwnerCharacter = Owner;
    if (!Owner) { return; }
    Camera = Owner->FindComponentByClass<UCameraComponent>();
    Weapon = Owner->GetShooterWeapon();
    if (Camera.IsValid())
    {
        HipFieldOfView = Camera->FieldOfView;
        HipMeshLocation = Owner->GetMesh()->GetRelativeLocation();
        bViewCached = true;
    }
    AimFieldOfView = FMath::IsFinite(AimFieldOfView) ? FMath::Clamp(AimFieldOfView, 20.f, HipFieldOfView) : 65.f;
    AimTransitionDuration = FMath::IsFinite(AimTransitionDuration) ? FMath::Max(0.05f, AimTransitionDuration) : 0.18f;
    Owner->GetMesh()->AddTickPrerequisiteComponent(this);
    Owner->OnGASDeathConfirmed.AddUniqueDynamic(this, &UShooterAimComponent::HandleDeath);
    if (Weapon.IsValid()) { Weapon->OnReloadStarted.AddUniqueDynamic(this, &UShooterAimComponent::HandleReloadStarted); }
    GameMode = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
    if (GameMode.IsValid()) { GameMode->OnRoundChanged.AddUniqueDynamic(this, &UShooterAimComponent::HandleRoundChanged); }
}
bool UShooterAimComponent::CanAim() const
{
    const AMyShooter* Owner = OwnerCharacter.Get();
    return bViewCached && Camera.IsValid() && IsValid(Owner) && !Owner->IsActorBeingDestroyed()
        && Owner->IsGASInitialized() && !Owner->HasGASDeathStarted() && Owner->GetGASHealth() > 0
        && !Owner->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShooterGameplayTags::State_Dashing) && Weapon.IsValid() && !Weapon->IsReloading() && AShooterGameMode::IsCombatAllowed(this);
}
void UShooterAimComponent::ToggleAiming()
{
    if (bAiming) { StopAiming(); } else { StartAiming(); }
}
bool UShooterAimComponent::StartAiming()
{
    if (!CanAim()) { return false; }
    SetAiming(true); SetComponentTickEnabled(true); return true;
}
void UShooterAimComponent::StopAiming()
{
    SetAiming(false);
    SetComponentTickEnabled(AimAlpha > 0.f);
}
void UShooterAimComponent::SetAiming(bool bNewAiming)
{
    if (bAiming == bNewAiming) { return; }
    bAiming = bNewAiming;
    if (AMyShooter* Owner = OwnerCharacter.Get())
    {
        // 本组件只维护自己增加的一次标签计数，不修改死亡/换弹等其他系统的标签。
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            if (bAiming) { ASC->AddLooseGameplayTag(ShooterGameplayTags::State_Aiming); }
            else { ASC->RemoveLooseGameplayTag(ShooterGameplayTags::State_Aiming); }
        }
    }
    OnAimingChanged.Broadcast(bAiming);
}
void UShooterAimComponent::ApplyView()
{
    if (!bViewCached) { return; }
    if (Camera.IsValid()) { Camera->SetFieldOfView(FMath::Lerp(HipFieldOfView, AimFieldOfView, AimAlpha)+7.f*DashAlpha); }
    if (AMyShooter* Owner = OwnerCharacter.Get())
    {
        Owner->GetMesh()->SetRelativeLocation(HipMeshLocation + AimMeshOffset * AimAlpha + FVector(-8.f,0,-12.f)*DashAlpha);
        if (UShooterPlayerAnimInstance* Anim = Cast<UShooterPlayerAnimInstance>(Owner->GetMesh()->GetAnimInstance()))
        {
            // 暂停前也同步到动画实例，恢复帧不会沿用开镜权重。
            Anim->SetShooterAimAlpha(AimAlpha);
        }
    }
}
void UShooterAimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (bAiming && !CanAim()) { ResetAiming(); return; }
    DashAlpha=FMath::FInterpConstantTo(DashAlpha,bDashPresentation?1.f:0.f,DeltaTime,10.f);
    AimAlpha = FMath::FInterpConstantTo(AimAlpha, bAiming ? 1.f : 0.f, DeltaTime, 1.f / AimTransitionDuration);
    ApplyView();
    if (!bAiming && AimAlpha <= 0.f && !bDashPresentation && DashAlpha<=0.f) { SetComponentTickEnabled(false); }
}
void UShooterAimComponent::SetDashPresentation(bool bActive,bool bImmediate)
{
    bDashPresentation=bActive;
    if(bImmediate) { DashAlpha=bActive?1.f:0.f; ApplyView(); }
    SetComponentTickEnabled(bActive||DashAlpha>0.f||bAiming||AimAlpha>0.f);
}
void UShooterAimComponent::ResetAiming()
{
    SetAiming(false); AimAlpha = 0.f; bDashPresentation=false; DashAlpha=0.f; ApplyView(); SetComponentTickEnabled(false);
}
UAnimMontage* UShooterAimComponent::GetFireMontage() const
{
    if (const auto* Definition=Weapon.IsValid()?Weapon->GetWeaponDefinition():nullptr)
    {
        if (bAiming && Definition->AimFireMontage) { return Definition->AimFireMontage; }
        if (Definition->FireMontage) { return Definition->FireMontage; }
    }
    return bAiming && AimFireMontage ? AimFireMontage.Get() : HipFireMontage.Get();
}
void UShooterAimComponent::HandleDeath(AShooterCharacterBase* DeadCharacter) { ResetAiming(); }
void UShooterAimComponent::HandleReloadStarted(float Duration) { ResetAiming(); }
void UShooterAimComponent::HandleRoundChanged(EShooterRoundState State)
{
    if (State != EShooterRoundState::Playing) { ResetAiming(); }
}
void UShooterAimComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ResetAiming();
    if (OwnerCharacter.IsValid())
    {
        OwnerCharacter->OnGASDeathConfirmed.RemoveDynamic(this, &UShooterAimComponent::HandleDeath);
        OwnerCharacter->GetMesh()->RemoveTickPrerequisiteComponent(this);
    }
    if (Weapon.IsValid()) { Weapon->OnReloadStarted.RemoveDynamic(this, &UShooterAimComponent::HandleReloadStarted); }
    if (GameMode.IsValid()) { GameMode->OnRoundChanged.RemoveDynamic(this, &UShooterAimComponent::HandleRoundChanged); }
    Super::EndPlay(Reason);
}
