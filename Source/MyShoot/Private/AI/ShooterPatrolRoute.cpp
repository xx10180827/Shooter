#include "AI/ShooterPatrolRoute.h"
#include "Components/SplineComponent.h"

AShooterPatrolRoute::AShooterPatrolRoute()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);
    PatrolSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PatrolSpline"));
    SetRootComponent(PatrolSpline);
    PatrolSpline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PatrolSpline->SetCanEverAffectNavigation(false);
    PatrolSpline->SetHiddenInGame(true);
    PatrolSpline->ClearSplinePoints(false);
    for (const FVector& Point : {FVector::ZeroVector, FVector(350,0,0), FVector(350,350,0)})
    {
        const int32 Index = PatrolSpline->GetNumberOfSplinePoints();
        PatrolSpline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
        PatrolSpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    PatrolSpline->SetClosedLoop(true, true);
}
int32 AShooterPatrolRoute::GetPointCount() const { return PatrolSpline->GetNumberOfSplinePoints(); }
FVector AShooterPatrolRoute::GetPointLocation(int32 Index) const
{
    return PatrolSpline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
}
bool AShooterPatrolRoute::IsLooping() const { return PatrolSpline->IsClosedLoop(); }
void AShooterPatrolRoute::SetWorldPoints(const TArray<FVector>& Points, bool bLoop)
{
    PatrolSpline->ClearSplinePoints(false);
    for (const FVector& Point : Points)
    {
        const int32 Index = PatrolSpline->GetNumberOfSplinePoints();
        PatrolSpline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
        PatrolSpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    PatrolSpline->SetClosedLoop(bLoop, true);
}
