#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "ShooterAttributeSet.generated.h"

// 唯一的生命值数据源：负责数值范围约束；死亡与表现由角色处理。
UCLASS()
class MYSHOOT_API UShooterAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UShooterAttributeSet();
    // 体力与生命共用 ASC 数值系统，消耗和恢复通过 GE 提交。
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UShooterAttributeSet, Stamina)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(Stamina)
    GAMEPLAYATTRIBUTE_VALUE_SETTER(Stamina)
    GAMEPLAYATTRIBUTE_VALUE_INITTER(Stamina)
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UShooterAttributeSet, MaxStamina)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxStamina)
    GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxStamina)
    GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxStamina)

    // GAS 标准宏生成属性描述、读值、写值和初始化接口；血量仍由此属性集统一持有。
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UShooterAttributeSet, Health)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(Health)
    GAMEPLAYATTRIBUTE_VALUE_SETTER(Health)
    GAMEPLAYATTRIBUTE_VALUE_INITTER(Health)

    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UShooterAttributeSet, MaxHealth)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth)
    GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth)
    GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth)

    // 同时约束当前值和基础值，效果执行后再处理最大血量改变带来的裁剪。
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
    virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
    UPROPERTY(BlueprintReadOnly, Category="Shooter|GAS") FGameplayAttributeData Stamina;
    UPROPERTY(BlueprintReadOnly, Category="Shooter|GAS") FGameplayAttributeData MaxStamina;
    UPROPERTY(BlueprintReadOnly, Category = "Shooter|GAS")
    FGameplayAttributeData Health;

    UPROPERTY(BlueprintReadOnly, Category = "Shooter|GAS")
    FGameplayAttributeData MaxHealth;

private:
    void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
