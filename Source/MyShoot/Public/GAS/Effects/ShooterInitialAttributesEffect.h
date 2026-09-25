#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShooterInitialAttributesEffect.generated.h"

// 初始属性瞬时效果：先写最大血量，再写当前血量，统一使用角色配置值。
UCLASS()
class MYSHOOT_API UShooterInitialAttributesEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UShooterInitialAttributesEffect();
    static FGameplayTag GetInitialHealthTag();
};
