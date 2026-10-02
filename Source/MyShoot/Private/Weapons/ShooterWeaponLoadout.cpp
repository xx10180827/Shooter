#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Weapons/ShooterAimComponent.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Characters/MyShooter.h"
#include "Animation/AnimMontage.h"
#include "Sound/SoundBase.h"


bool UShooterWeaponComponent::ConfigureLoadout(const TArray<UShooterWeaponDefinition*>& Definitions)
{
    if (HasBegunPlay() || Definitions.IsEmpty()) { return false; }
    for (const auto* Definition : Definitions) { if (!IsValid(Definition)) { return false; } }
    WeaponDefinitions.Reset();
    for (auto* Definition : Definitions) { WeaponDefinitions.Add(Definition); }
    return true;
}
const UShooterWeaponDefinition* UShooterWeaponComponent::GetWeaponDefinition() const
{
    return WeaponDefinitions.IsValidIndex(EquippedSlot) ? WeaponDefinitions[EquippedSlot].Get() : nullptr;
}
FText UShooterWeaponComponent::GetWeaponDisplayName() const
{
    const auto* Definition = GetWeaponDefinition();
    return Definition ? Definition->DisplayName : FText::FromString(TEXT("RIFLE"));
}
UAnimMontage* UShooterWeaponComponent::GetReloadMontage() const
{
    const auto* Definition = GetWeaponDefinition();
    return Definition ? Definition->ReloadMontage.Get() : nullptr;
}
void UShooterWeaponComponent::InitializeLoadout()
{
    // 整套无效时保持旧单武器行为，避免空项导致槽位与配置错位。
    for (const auto& Definition : WeaponDefinitions) { if (!IsValid(Definition)) { return; } }
    if (WeaponDefinitions.IsEmpty()) { return; }
    RuntimeSlots.SetNum(WeaponDefinitions.Num());
    for (int32 Index=0; Index<RuntimeSlots.Num(); ++Index)
    {
        RuntimeSlots[Index].Ammo = FMath::Max(1, WeaponDefinitions[Index]->MagazineCapacity);
        RuntimeSlots[Index].Reserve = FMath::Max(0, WeaponDefinitions[Index]->InitialReserveAmmo);
    }
    EquippedSlot = 0;
    ApplyDefinition(GetWeaponDefinition());
}
void UShooterWeaponComponent::ApplyDefinition(const UShooterWeaponDefinition* Definition)
{
    if (!Definition) { return; }
    auto Safe = [](float Value, float Min, float Fallback) { return FMath::IsFinite(Value) ? FMath::Max(Min, Value) : Fallback; };
    Damage = Safe(Definition->Damage, .01f, 25.f);
    FireInterval = Safe(Definition->FireInterval, .01f, .1f);
    Range = Safe(Definition->Range, 1.f, 10000.f);
    MagazineCapacity = FMath::Max(1, Definition->MagazineCapacity);
    InitialReserveAmmo = FMath::Max(0, Definition->InitialReserveAmmo);
    ReloadDuration = Safe(Definition->ReloadDuration, .01f, 1.5f);
    PelletCount = FMath::Clamp(Definition->PelletCount, 1, 32);
    SpreadHalfAngle = FMath::Clamp(Safe(Definition->SpreadHalfAngle, 0.f, 0.f), 0.f, 30.f);
    bAutomatic = Definition->bAutomatic;
    FireSound = Definition->FireSound;
    FireSoundVolume = FMath::Clamp(Safe(Definition->FireSoundVolume, 0.f, .65f), 0.f, 2.f);
    BulletVisualClass = Definition->BulletVisualClass;
    BulletVisualSpeed = Safe(Definition->BulletVisualSpeed, 100.f, 18000.f);
}
void UShooterWeaponComponent::SaveCurrentSlot()
{
    if (!RuntimeSlots.IsValidIndex(EquippedSlot)) { return; }
    auto& State = RuntimeSlots[EquippedSlot];
    State.Ammo = CurrentAmmo; State.Reserve = ReserveAmmo; State.NextShotTime = NextAllowedShotTime;
}
bool UShooterWeaponComponent::EquipWeapon(int32 Slot)
{
    if (!CanUseWeapon() || bResolvingShot || !RuntimeSlots.IsValidIndex(Slot) || Slot == EquippedSlot) { return false; }
    TGuardValue<bool> Guard(bSwitchingWeapon, true);
    // 取消回调只能观察旧武器；切换完成前不允许回调内再次开火、换弹或切枪。
    StopFiring();
    CancelReloading();
    // GAS 作用域锁可能延迟取消；此时拒绝换槽，避免把旧回调解释成新武器行为。
    if (IsFiring() || IsReloading() || bShuttingDown || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()) { return false; }
    if (AMyShooter* Player = Cast<AMyShooter>(GetOwner()))
    {
        if (Player->HasGASDeathStarted()) { return false; }
        if (Player->GetShooterAim()) { Player->GetShooterAim()->ResetAiming(); }
    }
    SaveCurrentSlot();
    const int32 OldSlot = EquippedSlot, OldAmmo = CurrentAmmo, OldReserve = ReserveAmmo;
    EquippedSlot = Slot;
    ApplyDefinition(GetWeaponDefinition());
    CurrentAmmo = RuntimeSlots[Slot].Ammo;
    ReserveAmmo = RuntimeSlots[Slot].Reserve;
    NextAllowedShotTime = RuntimeSlots[Slot].NextShotTime;
    OnWeaponChanged.Broadcast(OldSlot, Slot);
    OnAmmoChanged.Broadcast(OldAmmo, CurrentAmmo);
    OnReserveAmmoChanged.Broadcast(OldReserve, ReserveAmmo);
    return true;
}
