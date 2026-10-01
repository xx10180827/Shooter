#include "AI/ShooterCombatMovementComponent.h"
#include "AI/ShooterAIController.h"
#include "Characters/ShooterCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

UShooterCombatMovementComponent::UShooterCombatMovementComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UShooterCombatMovementComponent::InitializeMovement(AShooterAIController* Controller, AShooterCharacterBase* Pawn)
{
    Stop(); OwnerAI = Controller; Character = Pawn;
    if (Pawn) { OriginalSpeed = Pawn->GetCharacterMovement()->MaxWalkSpeed; }
    CombatMoveSpeed = FMath::IsFinite(CombatMoveSpeed) ? FMath::Max(10.f, CombatMoveSpeed) : 220.f;
    HoldDistance = FMath::IsFinite(HoldDistance) ? FMath::Max(100.f, HoldDistance) : 450.f;
    ResumeDistance = FMath::IsFinite(ResumeDistance) ? FMath::Max(HoldDistance+50.f, ResumeDistance) : HoldDistance+150.f;
}
void UShooterCombatMovementComponent::UpdateMovement(AShooterCharacterBase* Target, float AttackRange)
{
    if (!OwnerAI.IsValid() || !Character.IsValid() || !IsValid(Target)) { Stop(); return; }
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!bEnableMovingFire || !Nav || !Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
    { Stop(); OwnerAI->StopMovement(); return; }
    if (!bActive)
    {
        bActive = true; bClosing = true; NextMoveTime = 0;
        Character->GetCharacterMovement()->MaxWalkSpeed = CombatMoveSpeed;
    }
    const float StopRange = FMath::Min(HoldDistance, AttackRange*0.6f);
    const float RestartRange = FMath::Min(ResumeDistance, AttackRange*0.85f);
    const float Distance = FVector::Dist(Character->GetActorLocation(), Target->GetActorLocation());
    if (Distance <= StopRange)
    {
        bClosing = false; OwnerAI->StopMovement();
    }
    else if (Distance >= RestartRange) { bClosing = true; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (bClosing && OwnerAI->GetMoveStatus() != EPathFollowingStatus::Moving && Now >= NextMoveTime)
    {
        // 导航容差仍保持小值，射击距离与停步距离由玩法独立判定，避免绕障路径提前结束。
        OwnerAI->MoveToActor(Target, 75.f, false, true, true, nullptr, false);
        NextMoveTime = Now + 0.75;
    }
}
void UShooterCombatMovementComponent::Stop()
{
    if (bActive)
    {
        if (Character.IsValid()) { Character->GetCharacterMovement()->MaxWalkSpeed = OriginalSpeed; }
        if (OwnerAI.IsValid()) { OwnerAI->StopMovement(); }
    }
    bActive = false; bClosing = false; NextMoveTime = 0;
}
void UShooterCombatMovementComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Stop(); Super::EndPlay(Reason);
}