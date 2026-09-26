#include "GAS/Abilities/ShooterReloadAbility.h"
#include "GAS/ShooterGameplayTags.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UShooterReloadAbility::UShooterReloadAbility()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    // GAS 自动添加/移除标签；射击组件监听标签出现并停止正在进行的连射。
    ActivationOwnedTags.AddTag(ShooterGameplayTags::State_Reloading);
    ActivationBlockedTags.AddTag(ShooterGameplayTags::State_Reloading);
}

bool UShooterReloadAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }
    const UShooterWeaponComponent* Weapon = Cast<UShooterWeaponComponent>(GetSourceObject(Handle, ActorInfo));
    return IsValid(Weapon) && Weapon->GetOwner() == ActorInfo->AvatarActor.Get() && Weapon->CanReload();
}

void UShooterReloadAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    ActiveWeapon = Cast<UShooterWeaponComponent>(GetSourceObject(Handle, ActorInfo));
    bStarted = false;
    bAmmoCommitted = false;
    OldAmmo = 0;
    OldReserve = 0;
    UShooterWeaponComponent* Weapon = ActiveWeapon.Get();

    // 标签变化可能触发其他回调，因此激活时再次确认角色仍存活且确实需要补弹。
    if (!IsActive() || !Weapon || !Weapon->CanCompleteReload()
        || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    bStarted = true;
    GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this,
        &UShooterReloadAbility::CompleteReload, Weapon->GetReloadDuration(), false);

    // 先安排定时器，再通知表现；若蓝图同步取消或角色死亡，EndAbility 会清理它。
    Weapon->OnReloadStarted.Broadcast(Weapon->GetReloadDuration());
}

void UShooterReloadAbility::CompleteReload()
{
    if (!IsActive())
    {
        return;
    }
    UShooterWeaponComponent* Weapon = ActiveWeapon.Get();
    // 原子转移本身不广播外部事件，完成标记先落定，避免通知回调导致重复结算。
    bAmmoCommitted = Weapon && Weapon->CommitReloadAmmo(OldAmmo, OldReserve);
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, !bAmmoCommitted);
}

void UShooterReloadAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    if (!IsEndAbilityValid(Handle, ActorInfo))
    {
        return;
    }
    // 遵循 GAS 的作用域锁：延迟结束时，清理与通知也必须一起延迟。
    if (ScopeLockCount > 0)
    {
        WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UShooterReloadAbility::EndAbility,
            Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
        return;
    }
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReloadTimerHandle);
    }

    const TWeakObjectPtr<UShooterWeaponComponent> FinishedWeapon = ActiveWeapon;
    const bool bNotify = bStarted;
    const bool bSucceeded = bAmmoCommitted;
    const int32 PreviousAmmo = OldAmmo;
    const int32 PreviousReserve = OldReserve;
    if (UShooterWeaponComponent* Weapon = FinishedWeapon.Get())
    {
        Weapon->bReloadEnding = true;
    }
    // 先清除实例状态，防止结束通知内启动下一轮时被旧一轮覆盖。
    ActiveWeapon.Reset();
    bStarted = false;
    bAmmoCommitted = false;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

    if (UShooterWeaponComponent* Weapon = FinishedWeapon.Get())
    {
        Weapon->PublishReloadEnd(bSucceeded, bNotify, PreviousAmmo, PreviousReserve);
    }
}
