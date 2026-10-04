#include "GAS/Effects/ShooterInitialAttributesEffect.h"
#include "NativeGameplayTags.h"
#include "GAS/Attributes/ShooterAttributeSet.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_InitialHealth, "Data.InitialHealth");

FGameplayTag UShooterInitialAttributesEffect::GetInitialHealthTag()
{
    return TAG_Data_InitialHealth;
}

UShooterInitialAttributesEffect::UShooterInitialAttributesEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    // 实际初始值由角色通过 Data.InitialHealth 传入，效果类不写死角色血量。
    FSetByCallerFloat InitialHealth;
    InitialHealth.DataTag = GetInitialHealthTag();

    // 顺序不能颠倒：先提高上限，避免当前血量被旧上限裁剪。
    FGameplayModifierInfo MaxHealthModifier;
    MaxHealthModifier.Attribute = UShooterAttributeSet::GetMaxHealthAttribute();
    MaxHealthModifier.ModifierOp = EGameplayModOp::Override;
    MaxHealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(InitialHealth);
    Modifiers.Add(MaxHealthModifier);

    FGameplayModifierInfo HealthModifier;
    HealthModifier.Attribute = UShooterAttributeSet::GetHealthAttribute();
    HealthModifier.ModifierOp = EGameplayModOp::Override;
    HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(InitialHealth);
    Modifiers.Add(HealthModifier);
    FGameplayModifierInfo MaxStamina;
    MaxStamina.Attribute=UShooterAttributeSet::GetMaxStaminaAttribute(); MaxStamina.ModifierOp=EGameplayModOp::Override;
    MaxStamina.ModifierMagnitude=FScalableFloat(100.f); Modifiers.Add(MaxStamina);
    FGameplayModifierInfo Stamina=MaxStamina; Stamina.Attribute=UShooterAttributeSet::GetStaminaAttribute(); Modifiers.Add(Stamina);
}
