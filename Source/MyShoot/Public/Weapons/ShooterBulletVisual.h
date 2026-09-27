#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterBulletVisual.generated.h"
class UStaticMeshComponent;

/** 无碰撞、无伤害的飞行子弹表现；伤害已由武器射线唯一结算。 */
UCLASS()
class MYSHOOT_API AShooterBulletVisual : public AActor
{
    GENERATED_BODY()
public:
    AShooterBulletVisual();
    void Launch(const FVector& Destination, float Speed);
    virtual void Tick(float DeltaSeconds) override;
    UStaticMeshComponent* GetBulletMesh() const { return BulletMesh; }
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shooter|Visual")
    TObjectPtr<UStaticMeshComponent> BulletMesh;
private:
    FVector EndPoint = FVector::ZeroVector;
    float TravelSpeed = 18000.0f;
};
