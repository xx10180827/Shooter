#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "ShooterAIController.generated.h"
class AShooterCharacterBase;

UENUM(BlueprintType)
enum class EShooterAIState : uint8 { Idle, Chasing, Attacking, Dead };

/** 基础单机 AI：低频决策、NavMesh 追踪、攻击前摇和视线检查，伤害统一进入 GAS。 */
UCLASS()
class MYSHOOT_API AShooterAIController : public AAIController
{
    GENERATED_BODY()
public:
    AShooterAIController();
    UFUNCTION(BlueprintPure, Category="Shooter|AI")
    EShooterAIState GetCombatState() const { return CombatState; }
    UFUNCTION(BlueprintPure, Category="Shooter|AI")
    AShooterCharacterBase* GetCombatTarget() const { return CombatTarget.Get(); }
    /** 更换目标会撤销旧攻击；默认自动寻找本地玩家，测试或后续感知模块可显式设置。 */
    UFUNCTION(BlueprintCallable, Category="Shooter|AI")
    void SetCombatTarget(AShooterCharacterBase* Target);
    // 菜单/结算时立即撤销前摇与移动；保留低频定时器以便继续游戏。
    void SuspendCombat();
protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** 距离单位为厘米，检测是全向的；攻击还必须通过 Visibility 视线检测。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="1"))
    float DetectionRange = 2000.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="1"))
    float LoseTargetRange = 3000.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="1"))
    float AttackRange = 220.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="0.01"))
    float AttackDamage = 10.0f;
    /** 秒；间隔从开始攻击计，换目标不会绕过冷却。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="0.05"))
    float AttackInterval = 1.25f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="0.01"))
    float AttackWindup = 0.3f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="0.05"))
    float DecisionInterval = 0.2f;
    /** 表现接口不结算伤害；可在控制器蓝图中给受控角色播放攻击蒙太奇。 */
    UFUNCTION(BlueprintImplementableEvent, Category="Shooter|AI")
    void OnAttackStarted(AShooterCharacterBase* Target, float Windup);
private:
    void UpdateCombat();
    void FinishAttack();
    void CancelPendingAttack();
    void StopCombat(bool bDead);
    bool IsOwnerAlive() const;
    bool CanDamageTarget() const;
    bool HasClearShot(AShooterCharacterBase* Target) const;
    UFUNCTION()
    void HandleOwnerHealth(float OldHealth, float NewHealth);
    UFUNCTION()
    void HandleTargetHealth(float OldHealth, float NewHealth);
    UFUNCTION()
    void HandleTargetDestroyed(AActor* Actor);
    TWeakObjectPtr<AShooterCharacterBase> ControlledCharacter;
    TWeakObjectPtr<AShooterCharacterBase> CombatTarget;
    FTimerHandle DecisionTimer;
    FTimerHandle AttackTimer;
    double NextAttackTime = 0.0;
    double NextMoveRequestTime = 0.0;
    bool bAttackPending = false;
    UPROPERTY(VisibleInstanceOnly, Category="Shooter|AI")
    EShooterAIState CombatState = EShooterAIState::Idle;
};
