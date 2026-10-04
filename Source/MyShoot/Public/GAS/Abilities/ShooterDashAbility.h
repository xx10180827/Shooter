#pragma once
#include "CoreMinimal.h"
#include "GAS/Abilities/ShooterGameplayAbility.h"
#include "ShooterDashAbility.generated.h"
class UAbilityTask_ApplyRootMotionConstantForce;
/** 一次按键完成短距离位移；CommitAbility 统一提交体力 GE 和冷却 GE。 */
UCLASS()
class MYSHOOT_API UShooterDashAbility : public UShooterGameplayAbility
{
    GENERATED_BODY()
public:
    UShooterDashAbility();
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,const FGameplayTagContainer* SourceTags,const FGameplayTagContainer* TargetTags,FGameplayTagContainer* OptionalRelevantTags) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilityActivationInfo ActivationInfo,const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilityActivationInfo ActivationInfo,bool bReplicateEndAbility,bool bWasCancelled) override;
private:
    UFUNCTION() void FinishDash();
    UPROPERTY(Transient) TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> MotionTask;
};