#include "GAS/Abilities/ShooterDashAbility.h"
#include "GAS/Effects/ShooterDashEffects.h"
#include "GAS/ShooterGameplayTags.h"
#include "Movement/ShooterDashComponent.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterAimComponent.h"
#include "Weapons/ShooterRecoilComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
UShooterDashAbility::UShooterDashAbility()
{
    NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
    CostGameplayEffectClass=UShooterDashCostEffect::StaticClass();
    CooldownGameplayEffectClass=UShooterDashCooldownEffect::StaticClass();
    ActivationOwnedTags.AddTag(ShooterGameplayTags::State_Dashing);
    // 无敌随能力生命周期自动撤销，不延伸至冷却或暂停期间。
    ActivationOwnedTags.AddTag(ShooterGameplayTags::State_Invulnerable);
    ActivationBlockedTags.AddTag(ShooterGameplayTags::State_Dashing);
    ActivationBlockedTags.AddTag(ShooterGameplayTags::State_Reloading);
}
bool UShooterDashAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,const FGameplayTagContainer* SourceTags,const FGameplayTagContainer* TargetTags,FGameplayTagContainer* OptionalRelevantTags) const
{
    if(!Super::CanActivateAbility(Handle,ActorInfo,SourceTags,TargetTags,OptionalRelevantTags)) { return false; }
    const auto* Dash=Cast<UShooterDashComponent>(GetSourceObject(Handle,ActorInfo));
    return Dash&&Dash->GetOwner()==ActorInfo->AvatarActor.Get()&&Dash->CanStartDash();
}
void UShooterDashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilityActivationInfo ActivationInfo,const FGameplayEventData*)
{
    auto* Dash=Cast<UShooterDashComponent>(GetSourceObject(Handle,ActorInfo));
    auto* Player=Cast<AMyShooter>(ActorInfo->AvatarActor.Get());
    if(!IsActive()||!Dash||!Player||!Dash->CanStartDash()||!CommitAbility(Handle,ActorInfo,ActivationInfo))
    { EndAbility(Handle,ActorInfo,ActivationInfo,true,true); return; }
    // Commit 可能触发属性委托；回调取消/死亡后不能再创建位移任务。
    if(!IsActive()||!Dash->CanStartDash()) { if(IsActive()) { EndAbility(Handle,ActorInfo,ActivationInfo,true,true); } return; }
    Player->GetShooterWeapon()->StopFiring();
    if(auto* Aim=Player->GetShooterAim()) { Aim->ResetAiming(); }
    if(auto* Recoil=Player->FindComponentByClass<UShooterRecoilComponent>()) { Recoil->CancelPendingRecoil(); }
    if(!IsActive()||!Dash->CanStartDash()) { if(IsActive()) { EndAbility(Handle,ActorInfo,ActivationInfo,true,true); } return; }
    const float Distance=FMath::IsFinite(Dash->DashDistance)?FMath::Clamp(Dash->DashDistance,50.f,1000.f):480.f;
    const float Duration=FMath::IsFinite(Dash->DashDuration)?FMath::Clamp(Dash->DashDuration,.05f,.8f):.22f;
    // 地面/空中共用水平位移；只暂停重力，不切到飞行模式，也不关闭碰撞。
    auto* Movement=Player->GetCharacterMovement();
    SavedGravityScale=Movement->GravityScale; bGravityOverridden=true;
    Movement->GravityScale=0.f; Movement->Velocity.Z=0.f;
    // 同时覆盖 Root Motion 的 Z 速度，防止上升、下落或跳跃输入叠加。
    MotionTask=UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this,TEXT("DashMotion"),Dash->GetDashDirection(),Distance/Duration,Duration,false,nullptr,ERootMotionFinishVelocityMode::SetVelocity,FVector::ZeroVector,0.f,false);
    if(!MotionTask) { EndAbility(Handle,ActorInfo,ActivationInfo,true,true); return; }
    // 引擎恒定力默认执行完整结束帧。短闪避需要按剩余时长裁剪，防止卡顿时超过配置距离。
    if(auto Source=Player->GetCharacterMovement()->GetRootMotionSource(TEXT("DashMotion")))
    { Source->Settings.UnSetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick); }
    if(auto* Aim=Player->GetShooterAim()) { Aim->SetDashPresentation(true); }
    MotionTask->OnFinish.AddDynamic(this,&UShooterDashAbility::FinishDash); MotionTask->ReadyForActivation();
}
void UShooterDashAbility::FinishDash() { EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,false); }
void UShooterDashAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilityActivationInfo ActivationInfo,bool bReplicateEndAbility,bool bWasCancelled)
{
    if(!IsEndAbilityValid(Handle,ActorInfo)) { return; }
    if(ScopeLockCount>0)
    {
        WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this,&UShooterDashAbility::EndAbility,Handle,ActorInfo,ActivationInfo,bReplicateEndAbility,bWasCancelled)); return;
    }
    // 先移除 Root Motion 再清速度，避免暂停恢复或死亡后被旧任务重新推动。
    if(MotionTask) { MotionTask->OnFinish.RemoveAll(this); MotionTask->EndTask(); MotionTask=nullptr; }
    if(auto* Player=Cast<AMyShooter>(ActorInfo->AvatarActor.Get()))
    {
        auto* Movement=Player->GetCharacterMovement();
        if(bGravityOverridden) { Movement->GravityScale=SavedGravityScale; }
        // 空中从零竖直速度恢复下落；死亡若已 DisableMovement，不把角色改回可移动。
        Movement->StopMovementImmediately();
        if(auto* Aim=Player->GetShooterAim()) { Aim->SetDashPresentation(false,bWasCancelled); }
    }
    bGravityOverridden=false;
    Super::EndAbility(Handle,ActorInfo,ActivationInfo,bReplicateEndAbility,bWasCancelled);
}