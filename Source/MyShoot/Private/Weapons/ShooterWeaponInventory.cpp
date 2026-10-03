#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Characters/ShooterCharacterBase.h"
#include "Game/ShooterGameMode.h"

// 所有权和弹药属于角色实例；DataAsset 仅提供可共享配置，不被运行时领取修改。
int32 UShooterWeaponComponent::GetWeaponCount() const
{
    int32 Count=0; for(const auto& Slot:RuntimeSlots) { if(Slot.bOwned) { ++Count; } } return Count;
}
bool UShooterWeaponComponent::OwnsWeapon(const UShooterWeaponDefinition* Definition) const
{
    const int32 Slot=WeaponDefinitions.IndexOfByKey(Definition);
    return Definition&&RuntimeSlots.IsValidIndex(Slot)&&RuntimeSlots[Slot].bOwned;
}
int32 UShooterWeaponComponent::GetReserveForWeapon(const UShooterWeaponDefinition* Definition) const
{
    if(!OwnsWeapon(Definition)) { return 0; }
    const int32 Slot=WeaponDefinitions.IndexOfByKey(Definition);
    return Slot==EquippedSlot?ReserveAmmo:RuntimeSlots[Slot].Reserve;
}
bool UShooterWeaponComponent::TryGrantWeapon(UShooterWeaponDefinition* Definition)
{
    if(!IsValid(Definition)||!CanUseWeapon()||OwnsWeapon(Definition)||RuntimeSlots.IsEmpty()) { return false; }
    TGuardValue<bool> Guard(bSwitchingWeapon,true);
    int32 Slot=WeaponDefinitions.IndexOfByKey(Definition);
    if(Slot==INDEX_NONE) { Slot=WeaponDefinitions.Add(Definition); RuntimeSlots.AddDefaulted(); }
    if(!RuntimeSlots.IsValidIndex(Slot)) { return false; }
    auto& State=RuntimeSlots[Slot]; State.bOwned=true;
    State.Ammo=FMath::Max(1,Definition->MagazineCapacity);
    State.Reserve=FMath::Clamp(Definition->InitialReserveAmmo,0,FMath::Max(0,Definition->MaxReserveAmmo));
    State.NextShotTime=0;
    // 领取不自动切枪：当前连射/换弹仍属于原武器，按槽位键再切换。
    OnInventoryChanged.Broadcast(); return true;
}
int32 UShooterWeaponComponent::TryAddReserveAmmo(const UShooterWeaponDefinition* Definition,int32 Amount)
{
    if(Amount<=0||!CanUseWeapon()||!OwnsWeapon(Definition)) { return 0; }
    const int32 Slot=WeaponDefinitions.IndexOfByKey(Definition);
    const int32 OldReserve=GetReserveForWeapon(Definition);
    const int32 Capacity=FMath::Max(0,Definition->MaxReserveAmmo);
    const int32 Added=FMath::Min(Amount,FMath::Max(0,Capacity-OldReserve));
    if(Added<=0) { return 0; }
    TGuardValue<bool> Guard(bSwitchingWeapon,true);
    RuntimeSlots[Slot].Reserve=OldReserve+Added;
    if(Slot==EquippedSlot) { ReserveAmmo=OldReserve+Added; OnReserveAmmoChanged.Broadcast(OldReserve,ReserveAmmo); }
    OnInventoryChanged.Broadcast(); return Added;
}

int32 UShooterWeaponComponent::GetMagazineForWeapon(const UShooterWeaponDefinition* Definition) const
{
    if(!OwnsWeapon(Definition)) { return 0; }
    const int32 Slot=WeaponDefinitions.IndexOfByKey(Definition);
    return Slot==EquippedSlot?CurrentAmmo:RuntimeSlots[Slot].Ammo;
}
bool UShooterWeaponComponent::NeedsAmmoRefill(const UShooterWeaponDefinition* Definition) const
{
    return OwnsWeapon(Definition)
        &&(GetMagazineForWeapon(Definition)<FMath::Max(1,Definition->MagazineCapacity)
            ||GetReserveForWeapon(Definition)<FMath::Max(0,Definition->MaxReserveAmmo));
}
bool UShooterWeaponComponent::TryRefillWeaponAmmo(const UShooterWeaponDefinition* Definition)
{
    if(!CanUseWeapon()||!NeedsAmmoRefill(Definition)) { return false; }
    const int32 Slot=WeaponDefinitions.IndexOfByKey(Definition);
    TGuardValue<bool> Guard(bSwitchingWeapon,true);
    if(Slot==EquippedSlot)
    {
        // 拾取直接补满，不留下旧换弹定时器再扣备用；回调期间禁止切枪和再次领取。
        StopFiring(); CancelReloading();
        if(IsFiring()||IsReloading()) { return false; } // GAS 作用域锁延迟取消时，物品必须保留。
    }
    const auto* Character=Cast<AShooterCharacterBase>(GetOwner());
    if(bShuttingDown||!IsValid(Character)||Character->IsActorBeingDestroyed()||Character->HasGASDeathStarted()
        ||Character->GetGASHealth()<=0||!AShooterGameMode::IsCombatAllowed(this)) { return false; }
    const int32 OldAmmo=GetMagazineForWeapon(Definition),OldReserve=GetReserveForWeapon(Definition);
    auto& State=RuntimeSlots[Slot];
    State.Ammo=FMath::Max(1,Definition->MagazineCapacity); State.Reserve=FMath::Max(0,Definition->MaxReserveAmmo);
    if(Slot==EquippedSlot)
    {
        CurrentAmmo=State.Ammo; ReserveAmmo=State.Reserve;
        // 两项都写完才通知 HUD，监听任意一个事件时读到的都是同一次补满后的状态。
        OnAmmoChanged.Broadcast(OldAmmo,CurrentAmmo);
        OnReserveAmmoChanged.Broadcast(OldReserve,ReserveAmmo);
    }
    OnInventoryChanged.Broadcast(); return true;
}
