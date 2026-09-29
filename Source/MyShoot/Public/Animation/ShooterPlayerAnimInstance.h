#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ShooterPlayerAnimInstance.generated.h"

/** 为原 Shooter_idle 蓝图提供开镜混合权重；移动图和原 Slot 继续留在蓝图中。 */
UCLASS()
class MYSHOOT_API UShooterPlayerAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    void SetShooterAimAlpha(float Value) { ShooterAimAlpha = FMath::Clamp(Value, 0.f, 1.f); }
protected:
    /** 在游戏线程读取组件，动画图只消费缓存值，预览没有 Pawn 时为零。 */
    UPROPERTY(BlueprintReadOnly, Transient, Category="Shooter|Aim")
    float ShooterAimAlpha = 0.f;
};
