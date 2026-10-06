#pragma once
#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "ShooterCombatResult.generated.h"

/** 一次目标伤害的真实结算结果；表现层不重新计算伤害。 */
USTRUCT(BlueprintType)
struct MYSHOOT_API FShooterDamageResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) float ActualDamage = 0.f;
    UPROPERTY(BlueprintReadOnly) bool bKilled = false;
    UPROPERTY(BlueprintReadOnly) FHitResult Hit;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Target;
    bool WasDamaged() const { return ActualDamage > 0.f; }
};

/** 单发武器结果：霰弹枪同一目标先合并结算，再按一枪汇总 HUD/音效。 */
USTRUCT(BlueprintType)
struct MYSHOOT_API FShooterShotResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FShooterDamageResult> Targets;
    UPROPERTY(BlueprintReadOnly) int32 ShotId = 0;
    int32 GetKillCount() const
    {
        int32 Count = 0;
        for (const auto& Result : Targets) { if (Result.WasDamaged() && Result.bKilled) { ++Count; } }
        return Count;
    }
};
