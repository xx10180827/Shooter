#include "Pickups/ShooterWeaponPickup.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
bool AShooterWeaponPickup::CanReceive(const UShooterWeaponComponent* Weapon,FText& Reason) const
{
    if(!Weapon||!WeaponDefinition) { Reason=FText::FromString(TEXT("物品未配置")); return false; }
    if(Weapon->OwnsWeapon(WeaponDefinition)) { Reason=FText::FromString(TEXT("已拥有该武器")); return false; }
    Reason=FText::GetEmpty(); return true;
}
bool AShooterWeaponPickup::Grant(UShooterWeaponComponent* Weapon) { return Weapon->TryGrantWeapon(WeaponDefinition); }
