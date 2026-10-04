#include "GAS/Attributes/ShooterAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "GAS/ShooterGameplayTags.h"

UShooterAttributeSet::UShooterAttributeSet()
    : Stamina(100.f), MaxStamina(100.f), Health(100.0f), MaxHealth(100.0f)
{
}

// 所有属性入口共用相同约束，防止效果和直接写入出现不同边界行为。
void UShooterAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
    if(Attribute==GetMaxStaminaAttribute())
    { NewValue=FMath::IsFinite(NewValue)?FMath::Max(1.f,NewValue):100.f; }
    else if(Attribute==GetStaminaAttribute())
    { NewValue=FMath::IsFinite(NewValue)?FMath::Clamp(NewValue,0.f,GetMaxStamina()):0.f; }
    else if (Attribute == GetMaxHealthAttribute())
    {
        NewValue = FMath::IsFinite(NewValue) ? FMath::Max(1.0f, NewValue) : 100.0f;
    }
    else if (Attribute == GetHealthAttribute())
    {
        const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
        if (ASC && ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Dead))
        {
            // 当前没有复活系统：死亡后任何回血都保持为零，避免尸体恢复血量。
            NewValue = 0.0f;
            return;
        }
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

bool UShooterAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
    if(!Super::PreGameplayEffectExecute(Data)) { return false; }
    // 兜底拦截直接应用的伤害 GE；治疗、体力成本和恢复仍然正常结算。
    const auto* ASC=GetOwningAbilitySystemComponent();
    if(Data.EvaluatedData.Attribute==GetHealthAttribute()
        && Data.EvaluatedData.ModifierOp==EGameplayModOp::Additive && Data.EvaluatedData.Magnitude<0.f
        && ASC && ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Invulnerable)) { return false; }
    return true;
}

void UShooterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    if(Data.EvaluatedData.Attribute==GetStaminaAttribute()||Data.EvaluatedData.Attribute==GetMaxStaminaAttribute())
    { SetStamina(FMath::Clamp(GetStamina(),0.f,GetMaxStamina())); }

    if (Data.EvaluatedData.Attribute == GetHealthAttribute()
        || Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
}
