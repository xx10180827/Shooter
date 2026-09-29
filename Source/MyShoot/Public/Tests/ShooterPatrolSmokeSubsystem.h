#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "ShooterPatrolSmokeSubsystem.generated.h"
class AShooterAIController;
class AShooterCharacterBase;

/** 仅显式传入 -ShooterPatrolSmoke 时运行真实地图验收，普通游戏不注册 Tick。 */
UCLASS()
class MYSHOOT_API UShooterPatrolSmokeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    bool Step(float DeltaSeconds);
    void Finish(bool bSuccess, const FString& Message);
    FTSTicker::FDelegateHandle TickHandle;
    TWeakObjectPtr<AShooterAIController> TestAI;
    TWeakObjectPtr<AShooterCharacterBase> TestEnemy;
    FString Output;
    FVector LastPosition = FVector::ZeroVector;
    FVector HoldPosition = FVector::ZeroVector;
    FVector FarPlayerPosition = FVector::ZeroVector;
    double Deadline = 0, PhaseTime = 0, WaitStarted = -1, NextLogTime = 0;
    float Travel = 0, CombatSpeed = 0;
    int32 Phase = 0, WaitChecks = 0, StopsBeforeCombat = 0, IndexBeforeCombat = 0;
    struct FEnemyObservation
    {
        TWeakObjectPtr<AShooterCharacterBase> Enemy;
        FVector Start = FVector::ZeroVector, FirstGoalOffset = FVector::ZeroVector;
        FQuat LastLegRotation = FQuat::Identity;
        double FirstMoveTime = -1;
        int32 LegIndex = INDEX_NONE, PoseChanges = 0;
    };
    TArray<FEnemyObservation> Observations;
    FVector GoalBeforeCombat = FVector::ZeroVector;
    bool bFinished = false;
};