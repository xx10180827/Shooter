#include "Weapons/ShooterBulletVisual.h"
#include "Components/StaticMeshComponent.h"

AShooterBulletVisual::AShooterBulletVisual()
{
    PrimaryActorTick.bCanEverTick = true;
    BulletMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh"));
    SetRootComponent(BulletMesh);
    BulletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BulletMesh->SetGenerateOverlapEvents(false);
    BulletMesh->SetCastShadow(false);
    BulletMesh->SetCanEverAffectNavigation(false);
}
void AShooterBulletVisual::Launch(const FVector& Destination, float Speed)
{
    EndPoint = Destination;
    TravelSpeed = FMath::IsFinite(Speed) ? FMath::Max(100.0f, Speed) : 18000.0f;
    // 极近距离至少保留约 0.06 秒飞行，便于看到模型；只改变表现，不延迟射线伤害。
    TravelSpeed = FMath::Min(TravelSpeed, FVector::Distance(GetActorLocation(), EndPoint) / 0.06f);
    SetActorRotation((EndPoint - GetActorLocation()).Rotation());
    SetLifeSpan(FMath::Clamp(FVector::Distance(GetActorLocation(), EndPoint) / FMath::Max(TravelSpeed, 1.0f) + 0.1f, 0.1f, 3.0f));
}
void AShooterBulletVisual::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FVector Offset = EndPoint - GetActorLocation();
    const float Step = TravelSpeed * DeltaSeconds;
    // 只移动到已确认的端点，不越过命中墙体；不触发伤害、碰撞或第二次命中特效。
    if (Offset.SizeSquared() <= FMath::Square(Step)) { SetActorLocation(EndPoint); Destroy(); return; }
    SetActorLocation(GetActorLocation() + Offset.GetSafeNormal() * Step);
}
