#include "GAS/Effects/ShooterDamageEffect.h"
#include "NativeGameplayTags.h"
#include "GAS/Attributes/ShooterAttributeSet.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_HealthDelta, "Data.HealthDelta");

FGameplayTag UShooterDamageEffect::GetHealthDeltaTag()
{
    return TAG_Data_HealthDelta;
}

UShooterDamageEffect::UShooterDamageEffect()
{
    // 瞬时加法效果接收负血量差值；属性集负责归零和上限约束。
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FSetByCallerFloat HealthDelta;
    HealthDelta.DataTag = GetHealthDeltaTag();

    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UShooterAttributeSet::GetHealthAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealthDelta);
    Modifiers.Add(Modifier);
}
