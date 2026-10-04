#include "GAS/Effects/ShooterDashEffects.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "GAS/ShooterGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
UShooterDashCostEffect::UShooterDashCostEffect()
{
    DurationPolicy=EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Cost;
    Cost.Attribute=UShooterAttributeSet::GetStaminaAttribute();
    Cost.ModifierOp=EGameplayModOp::Additive;
    Cost.ModifierMagnitude=FScalableFloat(-25.f); Modifiers.Add(Cost);
}
UShooterDashCooldownEffect::UShooterDashCooldownEffect()
{
    DurationPolicy=EGameplayEffectDurationType::HasDuration;
    DurationMagnitude=FScalableFloat(1.2f);
    FInheritedTagContainer Tags; Tags.AddTag(ShooterGameplayTags::Cooldown_Dash);
    // 原生 GE 构造期必须使用具名默认子对象，不能经 FindOrAddComponent 创建匿名 UObject。
    auto* GrantedTags=CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("DashCooldownTags"));
    GEComponents.Add(GrantedTags); GrantedTags->SetAndApplyTargetTagChanges(Tags);
}
UShooterStaminaRegenEffect::UShooterStaminaRegenEffect()
{
    DurationPolicy=EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Regen;
    Regen.Attribute=UShooterAttributeSet::GetStaminaAttribute();
    Regen.ModifierOp=EGameplayModOp::Additive;
    // 组件每 0.2 秒在允许恢复时应用一次，相当于每秒恢复 10 点。
    Regen.ModifierMagnitude=FScalableFloat(2.f); Modifiers.Add(Regen);
}