#include "AI/ShooterAIController.h"
#include "AI/ShooterPatrolComponent.h"
#include "AI/ShooterAwarenessComponent.h"
#include "AI/ShooterCombatMovementComponent.h"
#include "Game/ShooterGameMode.h"
#include "Characters/ShooterCharacterBase.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Components/SceneComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAI, Log, All);

AShooterAIController::AShooterAIController()
{
    // 保留引擎控制器 Tick 更新朝向；决策与寻路请求仍由低频定时器驱动。
    PrimaryActorTick.bCanEverTick = true;
    ShooterPatrol = CreateDefaultSubobject<UShooterPatrolComponent>(TEXT("ShooterPatrol"));
    ShooterAwareness = CreateDefaultSubobject<UShooterAwarenessComponent>(TEXT("ShooterAwareness"));
    SetPerceptionComponent(*ShooterAwareness);
    CombatMovement = CreateDefaultSubobject<UShooterCombatMovementComponent>(TEXT("CombatMovement"));
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
    AttackRange = FMath::Min(Safe(AttackRange, 1, 1200), LoseTargetRange);
    SightHalfAngle = FMath::Clamp(Safe(SightHalfAngle, 1, 60), 1.f, 180.f);
    FireHalfAngle = FMath::Clamp(Safe(FireHalfAngle, 1, 15), 1.f, SightHalfAngle);
    SightMemorySeconds = Safe(SightMemorySeconds, 0, 3);
    MoveAcceptanceRadius = FMath::Clamp(Safe(MoveAcceptanceRadius, 10.f, 75.f), 10.f, 200.f);
    AttackDamage = Safe(AttackDamage, 0.01f, 10);
    DecisionInterval = Safe(DecisionInterval, 0.05f, 0.2f);
    AttackWindup = Safe(AttackWindup, 0.01f, 0.3f);
    AttackInterval = Safe(AttackInterval, AttackWindup, 1.25f);
    ShooterPatrol->InitializePatrol(this, ControlledCharacter.Get());
    CombatMovement->InitializeMovement(this, ControlledCharacter.Get());
    ShooterAwareness->RefreshListener();
    NextAttackTime = 0;
    NextMoveRequestTime = 0;
    ControlledCharacter->OnGASDeathConfirmed.AddUniqueDynamic(this, &AShooterAIController::HandleOwnerDeath);
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
    ShooterPatrol->SuspendPatrol();
    CombatMovement->Stop();
    InvestigationUntil = -1.0;
    if (AShooterCharacterBase* Previous = CombatTarget.Get())
    {
        Previous->OnGASHealthChanged.RemoveDynamic(this, &AShooterAIController::HandleTargetHealth);
        Previous->OnDestroyed.RemoveDynamic(this, &AShooterAIController::HandleTargetDestroyed);
    }
    CombatTarget = Target;
    LastSeenTime = -1.0;
    LastSeenLocation = FVector::ZeroVector;
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
        && IsWithinView(CombatTarget.Get(), FireHalfAngle) && HasClearShot(CombatTarget.Get());
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
                && CanSeeTarget(Candidate, DetectionRange))
            {
                SetCombatTarget(Candidate);
                Target = Candidate;
                break;
            }
        }
        if (!Target)
        {
            if (UpdateInvestigation()) { return; }
            CombatState = ShooterPatrol->UpdatePatrol() ? EShooterAIState::Patrolling : EShooterAIState::Idle;
            if (CombatState == EShooterAIState::Idle) { StopMovement(); }
            return;
        }
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!CanSeeTarget(Target, LoseTargetRange))
    {
        SearchLastSeenLocation(Now);
        return;
    }

    if (CombatState == EShooterAIState::Searching)
    {
        // 重新看见目标后撤销旧的固定点路径，允许立刻重新规划可见目标的追踪路径。
        StopMovement();
        NextMoveRequestTime = 0;
    }
    // 只有真正看见时才更新记忆；墙后移动不会泄露新的目标位置。
    LastSeenLocation = Target->GetActorLocation();
    LastSeenTime = Now;
    SetFocus(Target);
    const bool bWithinRange = FVector::DistSquared(ControlledCharacter->GetActorLocation(), Target->GetActorLocation())
        <= FMath::Square(AttackRange);
    if (!bWithinRange)
    {
        CancelPendingAttack();
        CombatMovement->Stop();
        ChaseVisibleTarget(Target);
        return;
    }

    // 射程内一边接近一边攻击；近距离保持安全间隔，不再每次决策都取消路径。
    CombatMovement->UpdateMovement(Target, AttackRange);
    CombatState = EShooterAIState::Attacking;
    if (!CanDamageTarget())
    {
        CancelPendingAttack();
        return;
    }
    if (!bAttackPending && Now >= NextAttackTime)
    {
        bAttackPending = true;
        NextAttackTime = Now + AttackInterval;
        GetWorldTimerManager().SetTimer(AttackTimer, this, &AShooterAIController::FinishAttack, AttackWindup, false);
        OnAttackStarted(Target, AttackWindup);
    }
}

FVector AShooterAIController::GetFocalPointOnActor(const AActor* Actor) const
{
    if (const AShooterCharacterBase* Target = Cast<AShooterCharacterBase>(Actor))
    {
        FVector EyeLocation; FRotator EyeRotation;
        Target->GetActorEyesViewPoint(EyeLocation, EyeRotation);
        return EyeLocation;
    }
    return Super::GetFocalPointOnActor(Actor);
}

bool AShooterAIController::IsWithinView(AShooterCharacterBase* Target, float HalfAngle) const
{
    if (!ControlledCharacter.IsValid() || !IsValid(Target)) { return false; }
    FVector EyeLocation; FRotator EyeRotation;
    ControlledCharacter->GetActorEyesViewPoint(EyeLocation, EyeRotation);
    FVector TargetEye; FRotator UnusedRotation;
    Target->GetActorEyesViewPoint(TargetEye, UnusedRotation);
    const FVector Direction = (TargetEye - EyeLocation).GetSafeNormal();
    return Direction.IsNearlyZero() || FVector::DotProduct(EyeRotation.Vector(), Direction)
        >= FMath::Cos(FMath::DegreesToRadians(HalfAngle));
}

bool AShooterAIController::CanSeeTarget(AShooterCharacterBase* Target, float Range) const
{
    return ControlledCharacter.IsValid() && IsValid(Target)
        && FVector::DistSquared(ControlledCharacter->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(Range)
        && IsWithinView(Target, SightHalfAngle) && HasClearShot(Target);
}

void AShooterAIController::ChaseVisibleTarget(AShooterCharacterBase* Target)
{
    CombatState = EShooterAIState::Chasing;
    const double Now = GetWorld()->GetTimeSeconds();
    // MoveToActor 自行跟随可见目标，只在请求完成或失败后限频重试；不逐帧重建路径。
    if (GetMoveStatus() != EPathFollowingStatus::Moving && Now >= NextMoveRequestTime)
    {
        const EPathFollowingRequestResult::Type Result = MoveToActor(Target, MoveAcceptanceRadius, false, true, true, nullptr, true);
        // 按需打开 LogShooterAI Verbose 排查地图导航，默认不刷日志。
        UE_LOG(LogShooterAI, Verbose, TEXT("Move request=%d distance=%.1f navSource=%s navTarget=%s"),
            int32(Result), FVector::Dist(GetPawn()->GetActorLocation(), Target->GetActorLocation()),
            *GetPawn()->GetNavAgentLocation().ToString(), *Target->GetNavAgentLocation().ToString());
        NextMoveRequestTime = Now + 0.75;
    }
}

void AShooterAIController::SearchLastSeenLocation(double Now)
{
    CombatMovement->Stop();
    CancelPendingAttack();
    if (LastSeenTime < 0 || Now - LastSeenTime >= SightMemorySeconds)
    {
        SetCombatTarget(nullptr);
        return;
    }
    if (CombatState != EShooterAIState::Searching)
    {
        // 撤销跟踪 Actor 的路径和 Focus，改为固定的最后可见点。
        StopMovement();
        ClearFocus(EAIFocusPriority::Gameplay);
        NextMoveRequestTime = 0;
    }
    CombatState = EShooterAIState::Searching;
    SetFocalPoint(LastSeenLocation);
    if (GetMoveStatus() != EPathFollowingStatus::Moving && Now >= NextMoveRequestTime)
    {
        MoveToLocation(LastSeenLocation, 75.f, false, true, true, true, nullptr, true);
        NextMoveRequestTime = Now + 0.75;
    }
}
void AShooterAIController::FinishAttack()
{
    bAttackPending = false;
    // 前摇结束重新检查死亡、距离和视线，目标躲开或进入墙后都不能继续扣血。
    if (CanDamageTarget())
    {
        AShooterCharacterBase* Target = CombatTarget.Get();
        // 实际开火与伤害处于同一结算点，躲开前摇时不会误播枪声。
        PlayAttackPresentation();
        UShooterDamageLibrary::ApplyGASDamage(ControlledCharacter.Get(), Target, AttackDamage,
            ControlledCharacter.Get(), FHitResult());
    }
    if (IsOwnerAlive())
    {
        CombatState = CanDamageTarget() ? EShooterAIState::Attacking : (CombatTarget.IsValid() ? EShooterAIState::Chasing : EShooterAIState::Idle);
    }
}

void AShooterAIController::CancelPendingAttack()
{
    StopAttackPresentation();
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
        ControlledPawn->OnGASDeathConfirmed.RemoveDynamic(this, &AShooterAIController::HandleOwnerDeath);
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
void AShooterAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);
    if (ShooterPatrol) { ShooterPatrol->OnMoveFinished(RequestID, Result); }
}

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

void AShooterAIController::PlayAttackPresentation()
{
    AShooterCharacterBase* ControlledPawn = ControlledCharacter.Get();
    if (!ControlledPawn || GetNetMode() == NM_DedicatedServer) { return; }
    SpawnAttackBullet();
    if (AttackMontage) { ControlledPawn->PlayAnimMontage(AttackMontage); }
    if (AttackSound)
    {
        // 单次非循环音效，世界暂停时随游戏音频暂停，不在 Tick 中反复触发。
        UGameplayStatics::PlaySoundAtLocation(this, AttackSound, ControlledPawn->GetActorLocation(),
            AttackSoundVolume, 1.0f, 0.0f, AttackSoundAttenuation);
    }
}

void AShooterAIController::StopAttackPresentation()
{
    // 只停止本控制器的攻击蒙太奇，不影响蓝图死亡动画或其他蒙太奇。
    AShooterCharacterBase* ControlledPawn = ControlledCharacter.Get();
    UAnimInstance* Anim = ControlledPawn && ControlledPawn->GetMesh() ? ControlledPawn->GetMesh()->GetAnimInstance() : nullptr;
    if (Anim && AttackMontage && Anim->Montage_IsActive(AttackMontage))
    {
        Anim->Montage_Stop(0.06f, AttackMontage);
    }
}

void AShooterAIController::HandleOwnerDeath(AShooterCharacterBase* DeadCharacter)
{
    // 在 OnGASDeathStarted 蓝图事件之前结束攻击，避免覆盖死亡姿势。
    StopCombat(true);
}
void AShooterAIController::SpawnAttackBullet()
{
    AShooterCharacterBase* ControlledPawn = ControlledCharacter.Get();
    AShooterCharacterBase* Target = CombatTarget.Get();
    if (!BulletVisualClass || !ControlledPawn || !IsValid(Target)) { return; }
    FVector Start; FRotator EyeRotation;
    ControlledPawn->GetActorEyesViewPoint(Start, EyeRotation);
    TInlineComponentArray<USceneComponent*> Components(ControlledPawn);
    for (USceneComponent* Component : Components)
    {
        if (Component->GetFName() == MuzzleComponentName) { Start = Component->GetComponentLocation(); break; }
    }
    FVector End; Target->GetActorEyesViewPoint(End, EyeRotation);
    // 当前 AI 的扣血仍由 FinishAttack 唯一结算；枪口路径裁剪只防止可见模型穿墙。
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ShooterAIBulletVisual), false, ControlledPawn);
    Query.AddIgnoredActor(Target);
    FHitResult Obstruction;
    if (GetWorld()->LineTraceSingleByChannel(Obstruction, Start, End, ECC_Visibility, Query)) { End = Obstruction.ImpactPoint; }
    FActorSpawnParameters Spawn; Spawn.Owner = ControlledPawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (AShooterBulletVisual* Bullet = GetWorld()->SpawnActor<AShooterBulletVisual>(BulletVisualClass, Start, (End-Start).Rotation(), Spawn))
    {
        Bullet->SetActorScale3D(FVector(FMath::IsFinite(BulletVisualScale) ? FMath::Clamp(BulletVisualScale, 0.1f, 5.f) : 1.5f));
        Bullet->Launch(End, BulletVisualSpeed);
    }
}

// 声音和受击只提供快照。重复枪声更新期限，不在每一发时重启同一条路径。
void AShooterAIController::InvestigateLocation(const FVector& Location, float Duration)
{
    if (!IsOwnerAlive() || !AShooterGameMode::IsCombatAllowed(this) || Location.ContainsNaN()) { return; }
    if (CombatTarget.IsValid() && CanSeeTarget(CombatTarget.Get(), LoseTargetRange)) { return; }
    const bool bRestartPath = CombatState != EShooterAIState::Investigating
        || FVector::DistSquared(Location, InvestigationLocation)>FMath::Square(100.f);
    if (CombatTarget.IsValid()) { SetCombatTarget(nullptr); }
    CancelPendingAttack();
    ShooterPatrol->SuspendPatrol();
    CombatMovement->Stop();
    if (bRestartPath) { StopMovement(); NextMoveRequestTime = 0; }
    InvestigationLocation = Location;
    InvestigationUntil = GetWorld()->GetTimeSeconds() + FMath::Clamp(Duration, 0.5f, 20.f);
    CombatState = EShooterAIState::Investigating;
    SetFocalPoint(InvestigationLocation);
}
bool AShooterAIController::UpdateInvestigation()
{
    if (InvestigationUntil < 0) { return false; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= InvestigationUntil)
    {
        SetCombatTarget(nullptr);
        return false;
    }
    CombatState = EShooterAIState::Investigating;
    SetFocalPoint(InvestigationLocation);
    if (GetMoveStatus() != EPathFollowingStatus::Moving && Now >= NextMoveRequestTime)
    {
        MoveToLocation(InvestigationLocation, 100.f, false, true, true, true, nullptr, false);
        NextMoveRequestTime = Now + 0.75;
    }
    return true;
}