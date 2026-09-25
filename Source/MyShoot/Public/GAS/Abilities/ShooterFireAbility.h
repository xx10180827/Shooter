#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/ShooterGameplayAbility.h"
#include "ShooterFireAbility.generated.h"

class UShooterWeaponComponent;

/** GA_Fire 的原生实现：管理激活、连射和取消生命周期，武器组件管理数据与单发执行。 */
UCLASS()
class MYSHOOT_API UShooterFireAbility : public UShooterGameplayAbility
{
    GENERATED_BODY()

public:
    UShooterFireAbility();

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    /** 正常结束、松开输入、换弹和死亡取消，都在这里清理同一个定时器。 */
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    void FireOrWait();
    TWeakObjectPtr<UShooterWeaponComponent> ActiveWeapon;
    FTimerHandle FireTimerHandle;
};
