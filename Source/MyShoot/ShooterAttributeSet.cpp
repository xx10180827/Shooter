#include "ShooterAttributeSet.h"
#include "GameplayEffectExtension.h"

UShooterAttributeSet::UShooterAttributeSet()
    : Health(100.0f), MaxHealth(100.0f)
{
}

void UShooterAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
    if (Attribute == GetMaxHealthAttribute())
    {
        NewValue = FMath::IsFinite(NewValue) ? FMath::Max(1.0f, NewValue) : 100.0f;
    }
    else if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::IsFinite(NewValue) ? FMath::Clamp(NewValue, 0.0f, GetMaxHealth()) : 0.0f;
    }
}

void UShooterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    ClampAttribute(Attribute, NewValue);
}

void UShooterAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
    Super::PreAttributeBaseChange(Attribute, NewValue);
    ClampAttribute(Attribute, NewValue);
}

void UShooterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute()
        || Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
}
