#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "ShooterAIController.generated.h"
class AShooterCharacterBase;
class UAnimMontage;
class USoundBase;
class USoundAttenuation;
class AShooterBulletVisual;
class UShooterPatrolComponent;

UENUM(BlueprintType)
enum class EShooterAIState : uint8 { Idle, Chasing, Attacking, Dead, Searching, Patrolling };

/** 基础单机 AI：低频决策、NavMesh 追踪、攻击前摇和视线检查，伤害统一进入 GAS。 */
UCLASS()
class MYSHOOT_API AShooterAIController : public AAIController
{
    GENERATED_BODY()
public:
    AShooterAIController();
    UShooterPatrolComponent* GetShooterPatrol() const { return ShooterPatrol; }
    UFUNCTION(BlueprintPure, Category="Shooter|AI")
    EShooterAIState GetCombatState() const { return CombatState; }
    UFUNCTION(BlueprintPure, Category="Shooter|AI")
    AShooterCharacterBase* GetCombatTarget() const { return CombatTarget.Get(); }
    /** 更换目标会撤销旧攻击；默认自动寻找本地玩家，测试或后续感知模块可显式设置。 */
    UFUNCTION(BlueprintCallable, Category="Shooter|AI")
    void SetCombatTarget(AShooterCharacterBase* Target);
    // 菜单/结算时立即撤销前摇与移动；保留低频定时器以便继续游戏。
    void SuspendCombat();
    /** 与视线/射击检测统一瞄准目标眼睛，避免近距离 Focus 指向脚下或角色中心。 */
    virtual FVector GetFocalPointOnActor(const AActor* Actor) const override;
protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shooter|Patrol")
    TObjectPtr<UShooterPatrolComponent> ShooterPatrol;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** 距离单位为厘米；首次发现还需满足前方视野角和无遮挡条件。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="1"))
    float DetectionRange = 2000.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="1"))
    float LoseTargetRange = 3000.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="1"))
    float AttackRange = 1200.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="0.01"))
    float AttackDamage = 10.0f;
    /** 单侧视野角，默认左右各 60 度，合计 120 度；使用角色眼睛方向。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Sight", meta=(ClampMin="1", ClampMax="180"))
    float SightHalfAngle = 60.f;
    /** 进入射程后先转向目标，面向误差小于此角度才开火。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Sight", meta=(ClampMin="1", ClampMax="90"))
    float FireHalfAngle = 15.f;
    /** 丢失视线后仅记住最后位置若干秒，不继续读取目标的隐蔽移动位置。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Sight", meta=(ClampMin="0"))
    float SightMemorySeconds = 3.f;
    /** 导航到达容差（厘米）。采用小容差保留绕障路径，战斗逻辑独立在射程内停步。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="10", ClampMax="200"))
    float MoveAcceptanceRadius = 75.f;
    /** 秒；间隔从开始攻击计，换目标不会绕过冷却。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="0.05"))
    float AttackInterval = 1.25f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="0.01"))
    float AttackWindup = 0.3f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI", meta=(ClampMin="0.05"))
    float DecisionInterval = 0.2f;
    /** 前摇结束且目标仍有效时播放；仅表现，不通过动画通知追加伤害。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Presentation")
    TObjectPtr<UAnimMontage> AttackMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Presentation")
    TObjectPtr<USoundBase> AttackSound;
    /** 敌人枪声使用空间衰减；玩家本地枪声由武器组件独立播放。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Presentation")
    TObjectPtr<USoundAttenuation> AttackSoundAttenuation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Presentation", meta=(ClampMin="0", ClampMax="2"))
    float AttackSoundVolume = 0.65f;
    /** 复用玩家的无碰撞子弹模型；只表现飞行，不再次结算伤害。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Presentation")
    TSubclassOf<AShooterBulletVisual> BulletVisualClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Presentation", meta=(ClampMin="100"))
    float BulletVisualSpeed = 6000.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Presentation", meta=(ClampMin="0.1", ClampMax="5"))
    float BulletVisualScale = 1.5f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|AI|Presentation")
    FName MuzzleComponentName = TEXT("Muzzle");
    /** 前摇表现扩展接口；开火蒙太奇和枪声统一在有效攻击结算时播放。 */
    UFUNCTION(BlueprintImplementableEvent, Category="Shooter|AI")
    void OnAttackStarted(AShooterCharacterBase* Target, float Windup);
private:
    void UpdateCombat();
    void FinishAttack();
    void CancelPendingAttack();
    void PlayAttackPresentation();
    void StopAttackPresentation();
    void SpawnAttackBullet();
    UFUNCTION()
    void HandleOwnerDeath(AShooterCharacterBase* DeadCharacter);
    void StopCombat(bool bDead);
    bool IsOwnerAlive() const;
    bool CanDamageTarget() const;
    bool HasClearShot(AShooterCharacterBase* Target) const;
    bool IsWithinView(AShooterCharacterBase* Target, float HalfAngle) const;
    bool CanSeeTarget(AShooterCharacterBase* Target, float Range) const;
    void ChaseVisibleTarget(AShooterCharacterBase* Target);
    void SearchLastSeenLocation(double Now);
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
    FVector LastSeenLocation = FVector::ZeroVector;
    double LastSeenTime = -1.0;
    double NextAttackTime = 0.0;
    double NextMoveRequestTime = 0.0;
    bool bAttackPending = false;
    UPROPERTY(VisibleInstanceOnly, Category="Shooter|AI")
    EShooterAIState CombatState = EShooterAIState::Idle;
};
