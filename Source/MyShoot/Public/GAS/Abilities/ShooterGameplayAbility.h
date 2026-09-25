#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ShooterGameplayAbility.generated.h"

// 项目战斗能力公共基类：开火、换弹和 AI 攻击继承后统一受到死亡标签限制。
UCLASS()
class MYSHOOT_API UShooterGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UShooterGameplayAbility();
};
