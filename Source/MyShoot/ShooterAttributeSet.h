#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ShooterAttributeSet.generated.h"

// Sole owner of GAS health. Gameplay reactions belong to the character.
UCLASS()
class MYSHOOT_API UShooterAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UShooterAttributeSet();

    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UShooterAttributeSet, Health)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(Health)
    GAMEPLAYATTRIBUTE_VALUE_SETTER(Health)
    GAMEPLAYATTRIBUTE_VALUE_INITTER(Health)

    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UShooterAttributeSet, MaxHealth)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth)
    GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth)
    GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth)

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Shooter|GAS")
    FGameplayAttributeData Health;

    UPROPERTY(BlueprintReadOnly, Category = "Shooter|GAS")
    FGameplayAttributeData MaxHealth;

private:
    void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
