#pragma once
#include "CoreMinimal.h"
#include "Pickups/ShooterPickup.h"
#include "ShooterAmmoPickup.generated.h"
class UShooterWeaponDefinition;
/** 弹药按武器配置匹配，一次补满弹匣与备用；至少一项实际增加才消耗。 */
UCLASS()
class MYSHOOT_API AShooterAmmoPickup : public AShooterPickup
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup") TObjectPtr<UShooterWeaponDefinition> WeaponDefinition;
    // 仅保留旧资产字段以便加载历史版本；当前补满规则不再使用数量，编辑器不显示。
    UPROPERTY() int32 Amount=30;
    virtual bool CanReceive(const UShooterWeaponComponent* Weapon,FText& Reason) const override;
protected:
    virtual bool Grant(UShooterWeaponComponent* Weapon) override;
};
