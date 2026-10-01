#include "Tests/ShooterPatrolSmokeSubsystem.h"
#include "AI/ShooterAIController.h"
#include "AI/ShooterPatrolComponent.h"
#include "AI/ShooterPatrolRoute.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Game/ShooterGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterPatrolSmoke, Log, All);

void UShooterPatrolSmokeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("ShooterPatrolSmoke")))
    {
        Output = FPaths::ProjectSavedDir() / TEXT("T12/RandomFix/MapSmoke");
        IFileManager::Get().MakeDirectory(*Output, true);
        Deadline = FPlatformTime::Seconds() + 180;
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &UShooterPatrolSmokeSubsystem::Step), 0.01f);
    }
#endif
}
void UShooterPatrolSmokeSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    Super::Deinitialize();
}
void UShooterPatrolSmokeSubsystem::Finish(bool bSuccess, const FString& Message)
{
    if (bFinished) { return; }
    bFinished = true;
    FFileHelper::SaveStringToFile((bSuccess ? TEXT("PASS\n") : TEXT("FAIL\n")) + Message,
        *(Output / TEXT("Result.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogShooterPatrolSmoke, Display, TEXT("%s: %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), *Message);
    FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
}
bool UShooterPatrolSmokeSubsystem::Step(float DeltaSeconds)
{
    if (bFinished) { return false; }
    if (FPlatformTime::Seconds() > Deadline)
    { Finish(false, FString::Printf(TEXT("Timeout phase=%d"), Phase)); return false; }
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    AMyShooter* Player = PC ? Cast<AMyShooter>(PC->GetPawn()) : nullptr;
    AShooterGameMode* GM = World ? Cast<AShooterGameMode>(World->GetAuthGameMode()) : nullptr;
    if (!Player || !GM || !World->HasBegunPlay() || !Player->IsGASInitialized()) { return true; }
    if (Phase == 0)
    {
        if (!GM->StartRound()) { Finish(false, TEXT("StartRound failed")); return false; }
        Player->GetCharacterMovement()->DisableMovement();
        // 本轮先同时观察所有敌人的独立随机目标、起步时间和真实腿部动画。
        for (TActorIterator<AShooterAIController> It(World); It; ++It)
        {

            AShooterCharacterBase* Enemy = Cast<AShooterCharacterBase>(It->GetPawn());
            if (!Enemy) { continue; }
            if (!TestAI.IsValid()) { TestAI = *It; TestEnemy = Enemy; }
            FEnemyObservation& Observation = Observations.AddDefaulted_GetRef();
            Observation.Enemy = Enemy; Observation.Start = Enemy->GetActorLocation();
            USkeletalMeshComponent* Mesh = Enemy->GetMesh();
            Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            TArray<FName> Bones; Mesh->GetBoneNames(Bones);
            for (const FName Bone : Bones)
            {
                const FString Name = Bone.ToString();
                if (Name.Contains(TEXT("Calf")) || Name.Contains(TEXT("Thigh")) || Name.Contains(TEXT("Leg")))
                {
                    Observation.LegIndex = Mesh->GetBoneIndex(Bone);
                    UE_LOG(LogShooterPatrolSmoke, Display, TEXT("Observe %s leg=%s"), *Enemy->GetName(), *Name);
                    break;
                }
            }
            if (Observation.LegIndex == INDEX_NONE) { Finish(false, TEXT("No leg bone for animation validation")); return false; }
        }
        if (!TestEnemy.IsValid()) { Finish(false, TEXT("No map enemy")); return false; }
        CombatSpeed = TestEnemy->GetCharacterMovement()->MaxWalkSpeed;
        FarPlayerPosition = TestEnemy->GetActorLocation() + FVector(10000,10000,1000);
        Player->SetActorLocation(FarPlayerPosition);
        LastPosition = TestEnemy->GetActorLocation();
        TestAI->UnPossess(); TestAI->Possess(TestEnemy.Get());
        Phase = 1;
        return true;
    }
    // 原死亡蓝图可销毁/解除控制器；验收清理后静止，不要求已经清理的对象继续存在。
    if (Phase == 6)
    {
        if (World->GetTimeSeconds()-PhaseTime <= 0.6) { return true; }
        const bool bControllerStopped = !TestAI.IsValid()
            || (!TestAI->GetShooterPatrol()->IsPatrolling() && TestAI->GetCombatState()==EShooterAIState::Dead);
        const bool bEnemyStopped = !TestEnemy.IsValid()
            || (TestEnemy->HasGASDeathStarted() && TestEnemy->GetVelocity().IsNearlyZero()
                && FVector::Dist2D(HoldPosition, TestEnemy->GetActorLocation())<=1);
        if (!bControllerStopped || !bEnemyStopped)
        { Finish(false, TEXT("Death failed to stop patrol")); return false; }
        Finish(true, FString::Printf(TEXT("Actual bloodstrike map: three independent random patrols and animated legs, NavMesh travel %.1f cm, %d random stop waits, pause/resume, auto-detected ranged combat, speed restore, target-loss patrol resume, death stop passed."), Travel, WaitChecks));
        return false;
    }
    if (!TestAI.IsValid() || !TestEnemy.IsValid())
    { Finish(false, TEXT("Unexpected enemy/controller destruction before death test")); return false; }
    UShooterPatrolComponent* Patrol = TestAI->GetShooterPatrol();
    const double Now = World->GetTimeSeconds();
    if (Now >= NextLogTime)
    {
        NextLogTime = Now + 3;
        UE_LOG(LogShooterPatrolSmoke, Display, TEXT("phase=%d state=%d stops=%d next=%d wait=%d travel=%.1f"),
            Phase, int32(TestAI->GetCombatState()), Patrol->GetCompletedStops(), Patrol->GetPointIndex(), Patrol->IsWaiting(), Travel);
    }
    if (Phase == 1)
    {
        for (FEnemyObservation& Observation : Observations)
        {
            AShooterCharacterBase* Enemy = Observation.Enemy.Get();
            if (!Enemy) { Finish(false, TEXT("Enemy missing during random patrol")); return false; }
            if (Enemy->GetVelocity().Size2D() > 100.f)
            {
                AShooterAIController* AI = Cast<AShooterAIController>(Enemy->GetController());
                if (!AI || !AI->GetShooterPatrol()->IsRandomPatrol()) { Finish(false, TEXT("Random patrol inactive")); return false; }
                if (Observation.FirstMoveTime < 0)
                {
                    Observation.FirstMoveTime = Now;
                    Observation.FirstGoalOffset = AI->GetShooterPatrol()->GetPatrolGoal()-Observation.Start;
                }
                const TArray<FTransform> Pose = Enemy->GetMesh()->GetBoneSpaceTransforms();
                if (Pose.IsValidIndex(Observation.LegIndex))
                {
                    const FQuat Rotation = Pose[Observation.LegIndex].GetRotation();
                    if (Observation.LastLegRotation.AngularDistance(Rotation)>0.002f) { ++Observation.PoseChanges; }
                    Observation.LastLegRotation = Rotation;
                }
            }
        }
        Travel += FVector::Dist2D(LastPosition, TestEnemy->GetActorLocation());
        LastPosition = TestEnemy->GetActorLocation();
        if (Patrol->IsWaiting())
        {
            if (WaitStarted < 0) { WaitStarted = Now; HoldPosition = LastPosition; }
            else if (FVector::Dist(HoldPosition, LastPosition)>10)
            { Finish(false, TEXT("Enemy moved during patrol wait")); return false; }
        }
        else if (WaitStarted >= 0)
        {
            if (Now-WaitStarted < 0.15)
            { Finish(false, TEXT("Patrol skipped configured stop duration")); return false; }
            ++WaitChecks; WaitStarted = -1;
        }
        if (Patrol->GetCompletedStops() >= 4 && WaitChecks >= 3 && Travel > 600 && !Patrol->IsWaiting())
        {
            if (!Patrol->IsRandomPatrol() || !FMath::IsNearlyEqual(TestEnemy->GetCharacterMovement()->MaxWalkSpeed, 180.f))
            { Finish(false, TEXT("Wrong patrol mode or speed")); return false; }
            bool bDifferentGoals = false, bDifferentStarts = false;
            if (Observations.Num()<3) { Finish(false, TEXT("Need three map enemies")); return false; }
            for (const FEnemyObservation& Observation : Observations)
            {
                UE_LOG(LogShooterPatrolSmoke, Display, TEXT("Random actor=%s firstMove=%.3f offset=%s movingLegChanges=%d"),
                    *GetNameSafe(Observation.Enemy.Get()), Observation.FirstMoveTime, *Observation.FirstGoalOffset.ToString(), Observation.PoseChanges);
                if (Observation.FirstMoveTime<0 || Observation.PoseChanges<20)
                { Finish(false, TEXT("An enemy did not patrol with continuously animated legs")); return false; }
                bDifferentGoals |= FVector::Dist2D(Observation.FirstGoalOffset, Observations[0].FirstGoalOffset)>50.f;
                bDifferentStarts |= FMath::Abs(Observation.FirstMoveTime-Observations[0].FirstMoveTime)>0.15;
            }
            if (!bDifferentGoals || !bDifferentStarts)
            { Finish(false, TEXT("Enemy patrols remained synchronized")); return false; }
            // 多敌人随机移动检查完成后再隔离其他攻击者，精确检查原单次战斗切换。
            for (const FEnemyObservation& Observation : Observations)
            {
                AShooterCharacterBase* Enemy = Observation.Enemy.Get();
                if (Enemy == TestEnemy.Get()) { continue; }
                if (AShooterAIController* AI = Cast<AShooterAIController>(Enemy->GetController()))
                { AI->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(AI); }
                Enemy->GetCharacterMovement()->DisableMovement(); Enemy->SetActorEnableCollision(false);
            }
            UE_LOG(LogShooterPatrolSmoke, Display, TEXT("Independent random patrol and moving animation passed: travel=%.1f waits=%d"), Travel, WaitChecks);
            GM->TogglePause(); HoldPosition = TestEnemy->GetActorLocation();
            PhaseTime = FPlatformTime::Seconds(); Phase = 2;
        }
    }
    else if (Phase == 2 && FPlatformTime::Seconds()-PhaseTime > 1.0)
    {
        if (GM->GetRoundState() != EShooterRoundState::Paused || Patrol->IsPatrolling()
            || FVector::Dist(HoldPosition, TestEnemy->GetActorLocation())>1)
        { Finish(false, TEXT("Pause did not suspend patrol")); return false; }
        GM->TogglePause(); Phase = 3;
    }
    else if (Phase == 3 && Patrol->IsPatrolling() && !Patrol->IsWaiting()
        && FVector::Dist2D(HoldPosition, TestEnemy->GetActorLocation())>100)
    {
        UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if (!Nav) { Finish(false, TEXT("Missing NavMesh")); return false; }
        FVector EnemyEye; FRotator Rotation;
        TestEnemy->GetActorEyesViewPoint(EnemyEye, Rotation);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PatrolSmoke), false, TestEnemy.Get()); Query.AddIgnoredActor(Player);
        bool bFound = false;
        for (int32 Angle=0; Angle<360 && !bFound; Angle+=30)
        {
            FNavLocation Point;
            if (!Nav->ProjectPointToNavigation(TestEnemy->GetActorLocation()+FRotator(0,Angle,0).Vector()*800.f,
                Point, FVector(100,100,300))) { continue; }
            Player->SetActorLocation(Point.Location+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
            FVector PlayerEye; Player->GetActorEyesViewPoint(PlayerEye, Rotation);
            if (World->LineTraceTestByChannel(EnemyEye, PlayerEye, ECC_Visibility, Query)) { continue; }
            const FRotator Facing = (PlayerEye-EnemyEye).Rotation();
            TestEnemy->SetActorRotation(FRotator(0,Facing.Yaw,0)); TestAI->SetControlRotation(Facing);
            bFound = true;
        }
        if (!bFound) { Finish(false, TEXT("No clear combat test position")); return false; }
        StopsBeforeCombat = Patrol->GetCompletedStops(); GoalBeforeCombat = Patrol->GetPatrolGoal();
        Phase = 4;
    }
    else if (Phase == 4 && Player->GetGASHealth() < 100)
    {
        if (Patrol->IsPatrolling() || Patrol->GetCompletedStops()!=StopsBeforeCombat
            || !Patrol->GetPatrolGoal().Equals(GoalBeforeCombat, 1.f)
            || !FMath::IsNearlyEqual(TestEnemy->GetCharacterMovement()->MaxWalkSpeed, 220.f))
        { Finish(false, TEXT("Combat failed to suspend route or restore speed")); return false; }
        UE_LOG(LogShooterPatrolSmoke, Display, TEXT("Auto-detection interrupted patrol; ranged damage=%.1f speed=%.1f"),
            100-Player->GetGASHealth(), TestEnemy->GetCharacterMovement()->MaxWalkSpeed);
        Player->SetActorLocation(FarPlayerPosition);
        HoldPosition = TestEnemy->GetActorLocation(); Phase = 5;
    }
    else if (Phase == 5 && Patrol->IsPatrolling()
        && FVector::Dist2D(HoldPosition, TestEnemy->GetActorLocation()) > 100)
    {
        UE_LOG(LogShooterPatrolSmoke, Display, TEXT("Patrol resumed after losing target"));
        UShooterDamageLibrary::ApplyGASDamage(Player, TestEnemy.Get(), 10000, Player, FHitResult());
        HoldPosition = TestEnemy->GetActorLocation(); PhaseTime = Now; Phase = 6;
    }
    return true;
}