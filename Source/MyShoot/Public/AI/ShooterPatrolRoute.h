#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterPatrolRoute.generated.h"
class USplineComponent;
class AShooterCharacterBase;

/** 可编辑巡逻路线：样条点提供目的地，实际移动使用 NavMesh。 */
UCLASS()
class MYSHOOT_API AShooterPatrolRoute : public AActor
{
    GENERATED_BODY()
public:
    AShooterPatrolRoute();
    int32 GetPointCount() const;
    FVector GetPointLocation(int32 Index) const;
    bool IsLooping() const;
    void SetWorldPoints(const TArray<FVector>& Points, bool bLoop);
    /** 指定敌人优先使用；留空时作为附近敌人可选的公共路线。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Shooter|Patrol")
    TObjectPtr<AShooterCharacterBase> AssignedEnemy;
    /** 到点停留秒数；停留期间仍检测玩家。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shooter|Patrol", meta=(ClampMin="0", ClampMax="30"))
    float WaitDuration = 1.5f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shooter|Patrol")
    TObjectPtr<USplineComponent> PatrolSpline;
};
