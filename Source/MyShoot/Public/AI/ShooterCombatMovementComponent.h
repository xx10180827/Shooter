#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShooterCombatMovementComponent.generated.h"
class AShooterAIController;
class AShooterCharacterBase;

/** 射程内边接近边开火，使用远近两个阈值避免原地反复启停；不负责伤害。 */
UCLASS(ClassGroup=(Shooter), meta=(BlueprintSpawnableComponent))
class MYSHOOT_API UShooterCombatMovementComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShooterCombatMovementComponent();
    void InitializeMovement(AShooterAIController* Controller, AShooterCharacterBase* Character);
    void UpdateMovement(AShooterCharacterBase* Target, float AttackRange);
    void Stop();
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|MovingFire")
    bool bEnableMovingFire = true;
    /** 射程内接近速度（厘米/秒）；追踪射程外目标仍使用角色原速度。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|MovingFire", meta=(ClampMin="10"))
    float CombatMoveSpeed = 220.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|MovingFire", meta=(ClampMin="100"))
    float HoldDistance = 450.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|MovingFire", meta=(ClampMin="100"))
    float ResumeDistance = 600.f;
private:
    TWeakObjectPtr<AShooterAIController> OwnerAI;
    TWeakObjectPtr<AShooterCharacterBase> Character;
    float OriginalSpeed = 600.f;
    double NextMoveTime = 0;
    bool bActive = false;
    bool bClosing = false;
};