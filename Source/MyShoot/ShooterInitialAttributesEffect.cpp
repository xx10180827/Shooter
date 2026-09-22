#include "ShooterInitialAttributesEffect.h"
#include "NativeGameplayTags.h"
#include "ShooterAttributeSet.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_InitialHealth, "Data.InitialHealth");

FGameplayTag UShooterInitialAttributesEffect::GetInitialHealthTag()
{
    return TAG_Data_InitialHealth;
}

UShooterInitialAttributesEffect::UShooterInitialAttributesEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;

    FSetByCallerFloat InitialHealth;
    InitialHealth.DataTag = GetInitialHealthTag();

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
}
