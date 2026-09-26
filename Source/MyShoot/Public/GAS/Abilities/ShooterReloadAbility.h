#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/ShooterGameplayAbility.h"
#include "ShooterReloadAbility.generated.h"

class UShooterWeaponComponent;

/** GA_Reload：能力持有换弹标签，单次定时器到期后统一补弹；所有取消路径都不补弹。 */
UCLASS()
class MYSHOOT_API UShooterReloadAbility : public UShooterGameplayAbility
{
    GENERATED_BODY()

public:
    UShooterReloadAbility();

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    void CompleteReload();

    TWeakObjectPtr<UShooterWeaponComponent> ActiveWeapon;
    FTimerHandle ReloadTimerHandle;
    bool bStarted = false;
    bool bAmmoCommitted = false;
    int32 OldAmmo = 0;
    int32 OldReserve = 0;
};
