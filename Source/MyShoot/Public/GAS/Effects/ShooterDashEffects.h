#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShooterDashEffects.generated.h"
// 单次提交消耗、持续冷却和恢复数值分为独立 GE，避免在能力内直接修改 Attribute。
UCLASS()
class MYSHOOT_API UShooterDashCostEffect : public UGameplayEffect
{
    GENERATED_BODY()
public: UShooterDashCostEffect();
};
UCLASS()
class MYSHOOT_API UShooterDashCooldownEffect : public UGameplayEffect
{
    GENERATED_BODY()
public: UShooterDashCooldownEffect();
};
UCLASS()
class MYSHOOT_API UShooterStaminaRegenEffect : public UGameplayEffect
{
    GENERATED_BODY()
public: UShooterStaminaRegenEffect();
};