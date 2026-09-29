#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AITypes.h"
#include "ShooterPatrolComponent.generated.h"
class AShooterAIController;
class AShooterCharacterBase;
class AShooterPatrolRoute;
struct FPathFollowingResult;

/** 管理路线、到点停留和巡逻速度；由 AI 低频决策调用，无额外 Tick/定时器。 */
UCLASS(ClassGroup=(Shooter), meta=(BlueprintSpawnableComponent))
class MYSHOOT_API UShooterPatrolComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShooterPatrolComponent();
    void InitializePatrol(AShooterAIController* Controller, AShooterCharacterBase* Character);
    bool UpdatePatrol();
    /** 先解除请求归属再停止移动，避免中断回调误算到点。 */
    void SuspendPatrol();
    void OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
    bool IsPatrolling() const { return bActive; }
    bool IsWaiting() const { return bWaiting; }
    int32 GetCompletedStops() const { return CompletedStops; }
    int32 GetPointIndex() const { return PointIndex; }
    AShooterPatrolRoute* GetRoute() const { return Route.Get(); }
    bool IsRandomPatrol() const { return bRandomPatrol; }
    FVector GetPatrolGoal() const { return RandomGoal; }
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Patrol")
    bool bPatrolEnabled = true;
    /** 默认在出生点附近随机巡逻；关闭后仍可使用原有固定路线对比。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Patrol")
    bool bRandomPatrol = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Patrol", meta=(ClampMin="200"))
    float PatrolRadius = 650.f;
    /** 每个敌人独立抽取停留时间，避免到点后同时启程。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Patrol", meta=(ClampMin="0.2"))
    float MinWaitDuration = 0.8f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Patrol", meta=(ClampMin="0.2"))
    float MaxWaitDuration = 2.8f;
    /** 厘米/秒；交战时恢复角色原来的移动速度。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Patrol", meta=(ClampMin="10"))
    float PatrolSpeed = 180.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Patrol", meta=(ClampMin="10", ClampMax="200"))
    float AcceptanceRadius = 60.f;
private:
    bool FindRoute();
    bool PickRandomGoal();
    void AdvancePoint();
    void FinishPoint(bool bReached);
    TWeakObjectPtr<AShooterAIController> OwnerAI;
    TWeakObjectPtr<AShooterCharacterBase> PatrolCharacter;
    TWeakObjectPtr<AShooterPatrolRoute> Route;
    FAIRequestID ActiveRequest = FAIRequestID::InvalidRequest;
    double WaitUntil = 0;
    double NextRouteSearchTime = 0;
    float OriginalWalkSpeed = 600.f;
    int32 PointIndex = 0;
    int32 Direction = 1;
    int32 CompletedStops = 0;
    FRandomStream PatrolRandom;
    FVector PatrolOrigin = FVector::ZeroVector;
    FVector RandomGoal = FVector::ZeroVector;
    bool bHasRandomGoal = false;
    bool bHasStarted = false;
    bool bActive = false;
    bool bWaiting = false;
};
