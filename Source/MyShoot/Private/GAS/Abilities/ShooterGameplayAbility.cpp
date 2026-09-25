#include "GAS/Abilities/ShooterGameplayAbility.h"
#include "GAS/ShooterGameplayTags.h"

UShooterGameplayAbility::UShooterGameplayAbility()
{
    // 每个角色有独立能力实例，才能安全持有定时器和临时状态。
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    // 所有派生战斗能力默认禁止在死亡状态下激活。
    ActivationBlockedTags.AddTag(ShooterGameplayTags::State_Dead);
}
