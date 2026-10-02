#include "Weapons/ShooterWeaponPresentationComponent.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Characters/ShooterCharacterBase.h"
#include "Components/AudioComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UShooterWeaponPresentationComponent::UShooterWeaponPresentationComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UShooterWeaponPresentationComponent::BeginPlay()
{
    Super::BeginPlay();
    Weapon = GetOwner()->FindComponentByClass<UShooterWeaponComponent>();
    TInlineComponentArray<USceneComponent*> Components(GetOwner());
    for (auto* Component : Components)
    {
        if (Component->GetFName() == TEXT("Weapon_mesh")) { OriginalMesh = Cast<USkeletalMeshComponent>(Component); }
        if (Component->GetFName() == TEXT("Muzzle")) { Muzzle = Component; OriginalMuzzleLocation=Component->GetRelativeLocation(); }
    }
    if (OriginalMesh.IsValid())
    {
        bOriginalVisible=OriginalMesh->IsVisible();
        EquippedMesh=NewObject<UStaticMeshComponent>(GetOwner(), TEXT("EquippedWeaponMesh"));
        EquippedMesh->SetupAttachment(OriginalMesh.Get());
        EquippedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        EquippedMesh->SetGenerateOverlapEvents(false);
        EquippedMesh->SetCanEverAffectNavigation(false);
        EquippedMesh->SetCastShadow(false);
        EquippedMesh->SetOnlyOwnerSee(true);
        EquippedMesh->RegisterComponent();
    }
    if (Weapon.IsValid())
    {
        Weapon->OnWeaponChanged.AddUniqueDynamic(this,&UShooterWeaponPresentationComponent::HandleWeaponChanged);
        Weapon->OnReloadStarted.AddUniqueDynamic(this,&UShooterWeaponPresentationComponent::HandleReloadStarted);
        Weapon->OnReloadFinished.AddUniqueDynamic(this,&UShooterWeaponPresentationComponent::HandleReloadFinished);
        HandleWeaponChanged(INDEX_NONE,Weapon->GetEquippedSlot());
    }
    if(auto* Character=Cast<AShooterCharacterBase>(GetOwner())) { Character->OnGASDeathConfirmed.AddUniqueDynamic(this,&UShooterWeaponPresentationComponent::HandleDeath); }
    GameMode=Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
    if(GameMode.IsValid()) { GameMode->OnRoundChanged.AddUniqueDynamic(this,&UShooterWeaponPresentationComponent::HandleRoundChanged); }
}
void UShooterWeaponPresentationComponent::HandleWeaponChanged(int32 OldSlot, int32 NewSlot)
{
    StopReloadSound();
    StopFireMontages();
    const auto* Definition=Weapon.IsValid()?Weapon->GetWeaponDefinition():nullptr;
    PreviousFireMontage=Definition?Definition->FireMontage.Get():nullptr;
    PreviousAimFireMontage=Definition?Definition->AimFireMontage.Get():nullptr;
    const bool bAlternate = Definition && Definition->WeaponMesh;
    // 不向子节点传播隐藏：原枪口、静态替换模型及其附属表现仍可工作。
    if (OriginalMesh.IsValid()) { OriginalMesh->SetVisibility(bOriginalVisible && !bAlternate, false); }
    if (EquippedMesh)
    {
        EquippedMesh->SetStaticMesh(bAlternate?Definition->WeaponMesh.Get():nullptr);
        EquippedMesh->SetVisibility(bAlternate);
        EquippedMesh->SetRelativeTransform(bAlternate?Definition->MeshTransform:FTransform::Identity);
    }
    if (Muzzle.IsValid()) { Muzzle->SetRelativeLocation(bAlternate?Definition->MuzzleLocation:OriginalMuzzleLocation); }
}
void UShooterWeaponPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Weapon.IsValid()) { Weapon->OnWeaponChanged.RemoveDynamic(this,&UShooterWeaponPresentationComponent::HandleWeaponChanged); }
    StopReloadSound();
    if (Weapon.IsValid())
    {
        Weapon->OnReloadStarted.RemoveDynamic(this,&UShooterWeaponPresentationComponent::HandleReloadStarted);
        Weapon->OnReloadFinished.RemoveDynamic(this,&UShooterWeaponPresentationComponent::HandleReloadFinished);
    }
    if(auto* Character=Cast<AShooterCharacterBase>(GetOwner())) { Character->OnGASDeathConfirmed.RemoveDynamic(this,&UShooterWeaponPresentationComponent::HandleDeath); }
    if(GameMode.IsValid()) { GameMode->OnRoundChanged.RemoveDynamic(this,&UShooterWeaponPresentationComponent::HandleRoundChanged); }
    if (EquippedMesh) { EquippedMesh->DestroyComponent(); }
    Super::EndPlay(Reason);
}

void UShooterWeaponPresentationComponent::HandleReloadStarted(float Duration)
{
    StopReloadSound();
    const auto* Definition=Weapon.IsValid()?Weapon->GetWeaponDefinition():nullptr;
    if(!Definition||!Definition->ReloadSound||Duration<=0||!Weapon->IsReloading()||!AShooterGameMode::IsCombatAllowed(this)) { return; }
    // 先创建再播放，确保是随世界暂停的游戏声音，不作为 UI 音频跨暂停继续响。
    const float Pitch=FMath::Clamp(Definition->ReloadSound->GetDuration()/Duration,.1f,4.f);
    auto* Audio=UGameplayStatics::CreateSound2D(this,Definition->ReloadSound,.6f,Pitch,0,nullptr,false,true);
    if(Audio) { ReloadAudio=Audio; Audio->SetUISound(false); Audio->Play(); }
}
void UShooterWeaponPresentationComponent::HandleReloadFinished(bool bSucceeded) { StopReloadSound(); }
bool UShooterWeaponPresentationComponent::IsReloadSoundPlaying() const { return ReloadAudio.IsValid()&&ReloadAudio->IsPlaying(); }
void UShooterWeaponPresentationComponent::StopReloadSound()
{
    if(ReloadAudio.IsValid()) { ReloadAudio->Stop(); }
    ReloadAudio.Reset();
}
void UShooterWeaponPresentationComponent::StopFireMontages()
{
    auto* Character=Cast<AShooterCharacterBase>(GetOwner());
    auto* Anim=Character?Character->GetMesh()->GetAnimInstance():nullptr;
    if(!Anim) { return; }
    // 只停止自己记录的开火蒙太奇，不调用全局 StopAllMontages，保护死亡和其他动作。
    if(PreviousFireMontage.IsValid()) { Anim->Montage_Stop(.06f,PreviousFireMontage.Get()); }
    if(PreviousAimFireMontage.IsValid()) { Anim->Montage_Stop(.06f,PreviousAimFireMontage.Get()); }
}
void UShooterWeaponPresentationComponent::HandleDeath(AShooterCharacterBase* Character) { StopReloadSound(); StopFireMontages(); }
void UShooterWeaponPresentationComponent::HandleRoundChanged(EShooterRoundState State)
{
    if(State!=EShooterRoundState::Playing) { StopReloadSound(); StopFireMontages(); }
}
