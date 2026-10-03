#include "Pickups/ShooterAmmoPickup.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
bool AShooterAmmoPickup::CanReceive(const UShooterWeaponComponent* Weapon,FText& Reason) const
{
    if(!Weapon||!WeaponDefinition) { Reason=FText::FromString(TEXT("物品未配置")); return false; }
    if(!Weapon->OwnsWeapon(WeaponDefinition)) { Reason=FText::FromString(TEXT("尚未拥有对应武器")); return false; }
    if(!Weapon->NeedsAmmoRefill(WeaponDefinition))
    { Reason=FText::FromString(TEXT("弹匣与备用弹药已满")); return false; }
    Reason=FText::GetEmpty(); return true;
}
bool AShooterAmmoPickup::Grant(UShooterWeaponComponent* Weapon) { return Weapon->TryRefillWeaponAmmo(WeaponDefinition); }
