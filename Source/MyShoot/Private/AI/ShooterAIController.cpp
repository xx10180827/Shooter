#include "AI/ShooterAIController.h"
#include "Game/ShooterGameMode.h"
#include "Characters/ShooterCharacterBase.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

AShooterAIController::AShooterAIController()
{
    // 保留引擎控制器 Tick 更新朝向；决策与寻路请求仍由低频定时器驱动。
    PrimaryActorTick.bCanEverTick = true;
}

void AShooterAIController::OnPossess(APawn* InPawn)
{
    StopCombat(false);
    Super::OnPossess(InPawn);
    ControlledCharacter = Cast<AShooterCharacterBase>(InPawn);
    if (!HasAuthority() || !ControlledCharacter.IsValid()) { return; }
    const auto Safe = [](float Value, float Min, float Fallback) { return FMath::IsFinite(Value) ? FMath::Max(Value, Min) : Fallback; };
    DetectionRange = Safe(DetectionRange, 1, 2000);
    LoseTargetRange = Safe(LoseTargetRange, DetectionRange, 3000);
    AttackRange = Safe(AttackRange, 1, 220);
    AttackDamage = Safe(AttackDamage, 0.01f, 10);
    DecisionInterval = Safe(DecisionInterval, 0.05f, 0.2f);
    AttackWindup = Safe(AttackWindup, 0.01f, 0.3f);
    AttackInterval = Safe(AttackInterval, AttackWindup, 1.25f);
    NextAttackTime = 0;
    NextMoveRequestTime = 0;
    ControlledCharacter->OnGASHealthChanged.AddUniqueDynamic(this, &AShooterAIController::HandleOwnerHealth);
    // OnPossess 可能早于 Pawn BeginPlay，首轮定时决策会等待 GAS 初始化。
    GetWorldTimerManager().SetTimer(DecisionTimer, this, &AShooterAIController::UpdateCombat, DecisionInterval, true);
}

bool AShooterAIController::IsOwnerAlive() const
{
    const AShooterCharacterBase* ControlledPawn = ControlledCharacter.Get();
    return HasAuthority() && IsValid(ControlledPawn) && !ControlledPawn->IsActorBeingDestroyed()
        && ControlledPawn->IsGASInitialized() && !ControlledPawn->HasGASDeathStarted() && ControlledPawn->GetGASHealth() > 0;
}

void AShooterAIController::SetCombatTarget(AShooterCharacterBase* Target)
{
    if (Target == ControlledCharacter.Get() || (Target && (!IsValid(Target) || Target->IsActorBeingDestroyed()
        || !Target->IsGASInitialized() || Target->HasGASDeathStarted() || Target->GetGASHealth() <= 0)))
    {
        Target = nullptr;
    }
    if (Target && CombatTarget.Get() == Target) { return; }
    CancelPendingAttack();
    if (AShooterCharacterBase* Previous = CombatTarget.Get())
    {
        Previous->OnGASHealthChanged.RemoveDynamic(this, &AShooterAIController::HandleTargetHealth);
        Previous->OnDestroyed.RemoveDynamic(this, &AShooterAIController::HandleTargetDestroyed);
    }
    CombatTarget = Target;
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    if (Target)
    {
        Target->OnGASHealthChanged.AddUniqueDynamic(this, &AShooterAIController::HandleTargetHealth);
        Target->OnDestroyed.AddUniqueDynamic(this, &AShooterAIController::HandleTargetDestroyed);
    }
    CombatState = IsOwnerAlive() ? (Target ? EShooterAIState::Chasing : EShooterAIState::Idle) : EShooterAIState::Idle;
}

bool AShooterAIController::HasClearShot(AShooterCharacterBase* Target) const
{
    if (!ControlledCharacter.IsValid() || !IsValid(Target)) { return false; }
    FVector Start; FRotator Rotation;
    ControlledCharacter->GetActorEyesViewPoint(Start, Rotation);
    FVector End; Target->GetActorEyesViewPoint(End, Rotation);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ShooterAIAttack), false);
    Params.AddIgnoredActor(ControlledCharacter.Get());
    Params.AddIgnoredActor(Target);
    // 忽略双方自身，仅检查两者之间是否有遮挡，不依赖玩家胶囊的默认通道响应。
    return !GetWorld()->LineTraceTestByChannel(Start, End, ECC_Visibility, Params);
}

bool AShooterAIController::CanDamageTarget() const
{
    const AShooterCharacterBase* Target = CombatTarget.Get();
    return AShooterGameMode::IsCombatAllowed(this) && IsOwnerAlive() && IsValid(Target) && !Target->IsActorBeingDestroyed()
        && Target->IsGASInitialized() && !Target->HasGASDeathStarted() && Target->GetGASHealth() > 0
        && FVector::DistSquared(ControlledCharacter->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(AttackRange)
        && HasClearShot(CombatTarget.Get());
}

void AShooterAIController::SuspendCombat()
{
    SetCombatTarget(nullptr);
}

void AShooterAIController::UpdateCombat()
{
    if (!IsOwnerAlive())
    {
        if (ControlledCharacter.IsValid() && ControlledCharacter->HasGASDeathStarted()) { StopCombat(true); }
        return;
    }
    if (!AShooterGameMode::IsCombatAllowed(this)) { SuspendCombat(); return; }
    AShooterCharacterBase* Target = CombatTarget.Get();
    if (Target && (!IsValid(Target) || Target->IsActorBeingDestroyed() || Target->HasGASDeathStarted()
        || Target->GetGASHealth() <= 0 || FVector::DistSquared(Target->GetActorLocation(), GetPawn()->GetActorLocation()) > FMath::Square(LoseTargetRange)))
    {
        SetCombatTarget(nullptr);
        Target = nullptr;
    }
    if (!Target)
    {
        // 当前单机版本只从玩家控制器选目标，不把其他敌人当作玩家。
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            AShooterCharacterBase* Candidate = PC ? Cast<AShooterCharacterBase>(PC->GetPawn()) : nullptr;
            if (Candidate && Candidate != GetPawn() && Candidate->IsGASInitialized() && !Candidate->HasGASDeathStarted()
                && Candidate->GetGASHealth() > 0
                && FVector::DistSquared(Candidate->GetActorLocation(), GetPawn()->GetActorLocation()) <= FMath::Square(DetectionRange)
                && HasClearShot(Candidate))
            {
                SetCombatTarget(Candidate);
                Target = Candidate;
                break;
            }
        }
        if (!Target) { CombatState = EShooterAIState::Idle; StopMovement(); return; }
    }
    SetFocus(Target);
    const double Now = GetWorld()->GetTimeSeconds();
    if (CanDamageTarget())
    {
        StopMovement();
        if (!bAttackPending && Now >= NextAttackTime)
        {
            CombatState = EShooterAIState::Attacking;
            bAttackPending = true;
            NextAttackTime = Now + AttackInterval;
            GetWorldTimerManager().SetTimer(AttackTimer, this, &AShooterAIController::FinishAttack, AttackWindup, false);
            OnAttackStarted(Target, AttackWindup);
        }
    }
    else
    {
        CancelPendingAttack();
        CombatState = EShooterAIState::Chasing;
        // 活跃 MoveToActor 自行跟踪目标；失败/结束后限频重试，避免每次决策重新寻路。
        if (GetMoveStatus() != EPathFollowingStatus::Moving && Now >= NextMoveRequestTime)
        {
            MoveToActor(Target, FMath::Max(10.0f, AttackRange * 0.6f), false, true, true, nullptr, true);
            NextMoveRequestTime = Now + 0.75;
        }
    }
}

void AShooterAIController::FinishAttack()
{
    bAttackPending = false;
    // 前摇结束重新检查死亡、距离和视线，目标躲开或进入墙后都不能继续扣血。
    if (CanDamageTarget())
    {
        AShooterCharacterBase* Target = CombatTarget.Get();
        UShooterDamageLibrary::ApplyGASDamage(ControlledCharacter.Get(), Target, AttackDamage,
            ControlledCharacter.Get(), FHitResult());
    }
    if (IsOwnerAlive())
    {
        CombatState = CombatTarget.IsValid() ? EShooterAIState::Chasing : EShooterAIState::Idle;
    }
}

void AShooterAIController::CancelPendingAttack()
{
    GetWorldTimerManager().ClearTimer(AttackTimer);
    bAttackPending = false;
}

void AShooterAIController::StopCombat(bool bDead)
{
    GetWorldTimerManager().ClearTimer(DecisionTimer);
    CancelPendingAttack();
    SetCombatTarget(nullptr);
    if (AShooterCharacterBase* ControlledPawn = ControlledCharacter.Get())
    {
        ControlledPawn->OnGASHealthChanged.RemoveDynamic(this, &AShooterAIController::HandleOwnerHealth);
    }
    ControlledCharacter.Reset();
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    CombatState = bDead ? EShooterAIState::Dead : EShooterAIState::Idle;
}

void AShooterAIController::HandleOwnerHealth(float OldHealth, float NewHealth)
{
    if (NewHealth <= 0) { StopCombat(true); }
}
void AShooterAIController::HandleTargetHealth(float OldHealth, float NewHealth)
{
    if (NewHealth <= 0) { SetCombatTarget(nullptr); }
}
void AShooterAIController::HandleTargetDestroyed(AActor* Actor) { SetCombatTarget(nullptr); }
void AShooterAIController::OnUnPossess()
{
    const bool bDead = ControlledCharacter.IsValid() && ControlledCharacter->HasGASDeathStarted();
    StopCombat(bDead);
    Super::OnUnPossess();
}
void AShooterAIController::EndPlay(const EEndPlayReason::Type Reason)
{
    StopCombat(CombatState == EShooterAIState::Dead);
    Super::EndPlay(Reason);
}
