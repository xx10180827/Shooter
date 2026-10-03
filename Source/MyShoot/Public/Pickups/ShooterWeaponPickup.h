#pragma once
#include "CoreMinimal.h"
#include "Pickups/ShooterPickup.h"
#include "ShooterWeaponPickup.generated.h"
class UShooterWeaponDefinition;
/** 武器领取规则：只授予尚未拥有的配置，不替换当前武器或补充已拥有武器的弹药。 */
UCLASS()
class MYSHOOT_API AShooterWeaponPickup : public AShooterPickup
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup") TObjectPtr<UShooterWeaponDefinition> WeaponDefinition;
    virtual bool CanReceive(const UShooterWeaponComponent* Weapon,FText& Reason) const override;
protected:
    virtual bool Grant(UShooterWeaponComponent* Weapon) override;
};
