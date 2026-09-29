#include "AI/ShooterPatrolComponent.h"
#include "AI/ShooterAIController.h"
#include "AI/ShooterPatrolRoute.h"
#include "Characters/ShooterCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

UShooterPatrolComponent::UShooterPatrolComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}
void UShooterPatrolComponent::InitializePatrol(AShooterAIController* Controller, AShooterCharacterBase* Character)
{
    SuspendPatrol();
    OwnerAI = Controller; PatrolCharacter = Character; Route.Reset();
    PointIndex = 0; Direction = 1; CompletedStops = 0; NextRouteSearchTime = 0;
    bHasStarted = false; bHasRandomGoal = false;
    PatrolOrigin = Character ? Character->GetActorLocation() : FVector::ZeroVector;
    // 每个控制器持有自己的随机流，不使用统一路线序号或统一启动节拍。
    PatrolRandom.Initialize(int32(GetTypeHash(FGuid::NewGuid())));
    PatrolRadius = FMath::IsFinite(PatrolRadius) ? FMath::Max(200.f, PatrolRadius) : 650.f;
    MinWaitDuration = FMath::IsFinite(MinWaitDuration) ? FMath::Max(0.2f, MinWaitDuration) : 0.8f;
    MaxWaitDuration = FMath::IsFinite(MaxWaitDuration) ? FMath::Max(MinWaitDuration, MaxWaitDuration) : FMath::Max(MinWaitDuration, 2.8f);
    if (Character) { OriginalWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed; }
    PatrolSpeed = FMath::IsFinite(PatrolSpeed) ? FMath::Max(10.f, PatrolSpeed) : 180.f;
    AcceptanceRadius = FMath::IsFinite(AcceptanceRadius) ? FMath::Clamp(AcceptanceRadius, 10.f, 200.f) : 60.f;
}
bool UShooterPatrolComponent::FindRoute()
{
    AShooterPatrolRoute* Best = nullptr;
    double BestDistance = TNumericLimits<double>::Max();
    bool bHasAssignedRoute = false;
    for (TActorIterator<AShooterPatrolRoute> It(GetWorld()); It; ++It)
    {
        if (It->GetPointCount() == 0 || (It->AssignedEnemy && It->AssignedEnemy != PatrolCharacter.Get())) { continue; }
        const bool bAssigned = It->AssignedEnemy == PatrolCharacter.Get();
        const double Distance = FVector::DistSquared(PatrolCharacter->GetActorLocation(), It->GetPointLocation(0));
        if ((bAssigned && !bHasAssignedRoute) || (bAssigned == bHasAssignedRoute && Distance < BestDistance))
        {
            Best = *It; BestDistance = Distance; bHasAssignedRoute = bAssigned;
        }
    }
    Route = Best;
    if (!Best) { return false; }
    // 首次从最近点开始，交战中断后保留尚未完成的点序号。
    BestDistance = TNumericLimits<double>::Max();
    for (int32 Index = 0; Index < Best->GetPointCount(); ++Index)
    {
        const double Distance = FVector::DistSquared(PatrolCharacter->GetActorLocation(), Best->GetPointLocation(Index));
        if (Distance < BestDistance) { PointIndex = Index; BestDistance = Distance; }
    }
    return true;
}
bool UShooterPatrolComponent::PickRandomGoal()
{
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) { return false; }
    // 在出生点附近采样可达位置，不会追随玩家把巡逻中心带到整个地图之外。
    for (int32 Attempt=0; Attempt<12; ++Attempt)
    {
        const float Angle = PatrolRandom.FRandRange(0.f, 360.f);
        const float Radius = FMath::Sqrt(PatrolRandom.FRand()) * PatrolRadius;
        FNavLocation Point;
        if (!Nav->ProjectPointToNavigation(PatrolOrigin + FRotator(0,Angle,0).Vector()*Radius,
            Point, FVector(120,120,400))) { continue; }
        if (FVector::Dist2D(Point.Location, PatrolOrigin)>PatrolRadius
            || FVector::Dist2D(Point.Location, PatrolCharacter->GetActorLocation())<200.f) { continue; }
        UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),
            PatrolCharacter->GetNavAgentLocation(), Point.Location, PatrolCharacter.Get());
        if (!Path || !Path->IsValid() || Path->IsPartial()) { continue; }
        RandomGoal = Point.Location; bHasRandomGoal = true;
        return true;
    }
    return false;
}
bool UShooterPatrolComponent::UpdatePatrol()
{
    if (!bPatrolEnabled || !OwnerAI.IsValid() || !PatrolCharacter.IsValid()) { SuspendPatrol(); return false; }
    // 空地图或导航尚未加载时保持待机；不把无法移动的状态显示为巡逻。
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (bRandomPatrol && (!Nav || !Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)))
    {
        SuspendPatrol();
        return false;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bRandomPatrol && !Route.IsValid())
    {
        SuspendPatrol();
        if (Now < NextRouteSearchTime) { return false; }
        NextRouteSearchTime = Now + 2.0;
        if (!FindRoute()) { return false; }
    }
    if (!bRandomPatrol && Route->GetPointCount() == 0) { SuspendPatrol(); Route.Reset(); return false; }
    if (!bActive)
    {
        bActive = true;
        OwnerAI->ClearFocus(EAIFocusPriority::Gameplay);
        PatrolCharacter->GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
        if (bRandomPatrol && !bHasStarted)
        {
            bHasStarted = true; bWaiting = true;
            WaitUntil = Now + PatrolRandom.FRandRange(0.2f, 2.5f);
        }
    }
    if (bWaiting && Now < WaitUntil) { return true; }
    bWaiting = false;
    if (ActiveRequest.IsValid()) { return true; }
    if (bRandomPatrol && !bHasRandomGoal && !PickRandomGoal())
    {
        bWaiting = true; WaitUntil = Now + 1.f;
        return true;
    }
    if (!bRandomPatrol) { PointIndex = FMath::Clamp(PointIndex, 0, Route->GetPointCount() - 1); }
    FAIMoveRequest Move;
    Move.SetGoalLocation(bRandomPatrol ? RandomGoal : Route->GetPointLocation(PointIndex));
    Move.SetUsePathfinding(true); Move.SetProjectGoalLocation(true);
    Move.SetAllowPartialPath(false); Move.SetAcceptanceRadius(AcceptanceRadius);
    Move.SetReachTestIncludesAgentRadius(false);
    const FPathFollowingRequestResult Result = OwnerAI->MoveTo(Move);
    if (Result.Code == EPathFollowingRequestResult::RequestSuccessful) { ActiveRequest = Result.MoveId; }
    else { FinishPoint(Result.Code == EPathFollowingRequestResult::AlreadyAtGoal); }
    return true;
}
void UShooterPatrolComponent::AdvancePoint()
{
    if (!Route.IsValid() || Route->GetPointCount() <= 1) { PointIndex = 0; return; }
    const int32 Count = Route->GetPointCount();
    if (Route->IsLooping()) { PointIndex = (PointIndex + 1) % Count; }
    else
    {
        // 非闭环路线到端点后折返。
        if (PointIndex + Direction >= Count || PointIndex + Direction < 0) { Direction *= -1; }
        PointIndex += Direction;
    }
}
void UShooterPatrolComponent::FinishPoint(bool bReached)
{
    ActiveRequest = FAIRequestID::InvalidRequest;
    if (bReached) { ++CompletedStops; }
    const float ConfiguredWait = bRandomPatrol ? PatrolRandom.FRandRange(MinWaitDuration, MaxWaitDuration)
        : (Route.IsValid() ? Route->WaitDuration : 1.5f);
    const float Wait = FMath::IsFinite(ConfiguredWait) ? FMath::Clamp(ConfiguredWait, 0.f, 30.f) : 1.5f;
    // 无法到达不计为完成，限频尝试下一点，避免刷请求或永久卡住。
    WaitUntil = GetWorld()->GetTimeSeconds() + (bReached ? FMath::Max(0.05f, Wait) : 1.f);
    bWaiting = true;
    if (bRandomPatrol) { bHasRandomGoal = false; } else { AdvancePoint(); }
}
void UShooterPatrolComponent::OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    if (!bActive || RequestID != ActiveRequest) { return; }
    FinishPoint(Result.IsSuccess());
}
void UShooterPatrolComponent::SuspendPatrol()
{
    const bool bWasActive = bActive;
    bActive = false; bWaiting = false; WaitUntil = 0;
    ActiveRequest = FAIRequestID::InvalidRequest;
    if (bWasActive)
    {
        if (PatrolCharacter.IsValid()) { PatrolCharacter->GetCharacterMovement()->MaxWalkSpeed = OriginalWalkSpeed; }
        if (OwnerAI.IsValid()) { OwnerAI->StopMovement(); }
    }
}
void UShooterPatrolComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    SuspendPatrol();
    Super::EndPlay(Reason);
}
