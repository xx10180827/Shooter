#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShooterDamageEffect.generated.h"

// 伤害瞬时效果：叠加血量差值，正伤害转负差值统一由伤害入口负责。
UCLASS()
class MYSHOOT_API UShooterDamageEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UShooterDamageEffect();
    static FGameplayTag GetHealthDeltaTag();
};
