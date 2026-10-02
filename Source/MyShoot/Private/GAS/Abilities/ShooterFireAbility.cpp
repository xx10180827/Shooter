#include "GAS/Abilities/ShooterFireAbility.h"
#include "GAS/ShooterGameplayTags.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UShooterFireAbility::UShooterFireAbility()
{
    // 当前只在权威端执行单机玩法；尚未提供联机输入转发和客户端预测。
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationBlockedTags.AddTag(ShooterGameplayTags::State_Reloading);
}

bool UShooterFireAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }
    const UShooterWeaponComponent* Weapon = Cast<UShooterWeaponComponent>(GetSourceObject(Handle, ActorInfo));
    return IsValid(Weapon) && Weapon->GetOwner() == ActorInfo->AvatarActor.Get() && Weapon->CanFire();
}

void UShooterFireAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ActiveWeapon = Cast<UShooterWeaponComponent>(GetSourceObject(Handle, ActorInfo));
    if (!ActiveWeapon.IsValid() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    FireOrWait();
}

void UShooterFireAbility::FireOrWait()
{
    if (!IsActive())
    {
        return;
    }

    UShooterWeaponComponent* Weapon = ActiveWeapon.Get();
    if (!Weapon || !Weapon->CanFire())
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
        return;
    }

    // 重新按下时可能仍处于上一发间隔内：等待剩余时间，不重置武器的射速时间戳。
    if (Weapon->GetTimeUntilNextShot() <= 0.0f)
    {
        const bool bFired = Weapon->TryFireOneShot();
        if (bFired && IsActive() && !Weapon->bAutomatic)
        {
            // 半自动武器一轮按下最多发射一发；下一次按下仍遵守该槽的冷却。
            EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
            return;
        }
    }

    // 命中、死亡或表现回调可能同步取消能力；取消后绝不能再安排下一发。
    if (!IsActive())
    {
        return;
    }
    Weapon = ActiveWeapon.Get();
    if (!Weapon || !Weapon->CanFire())
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
        return;
    }

    // 每次只设置一个单次定时器；帧率下降时不补发一帧内积攒的多枚子弹。
    GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &UShooterFireAbility::FireOrWait,
        FMath::Max(Weapon->GetTimeUntilNextShot(), 0.001f), false);
}

void UShooterFireAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FireTimerHandle);
    }
    ActiveWeapon.Reset();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
