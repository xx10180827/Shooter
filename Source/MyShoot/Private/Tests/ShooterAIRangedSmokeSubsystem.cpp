#include "Tests/ShooterAIRangedSmokeSubsystem.h"
#include "AI/ShooterAIController.h"
#include "Characters/MyShooter.h"
#include "Game/ShooterGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAIRangedSmoke, Log, All);

void UShooterAIRangedSmokeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("ShooterAIRangedSmoke")))
    {
        Output = FPaths::ProjectSavedDir() / TEXT("T11/MapSmoke");
        IFileManager::Get().MakeDirectory(*Output, true);
        Deadline = FPlatformTime::Seconds() + 180;
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &UShooterAIRangedSmokeSubsystem::Step), 0.01f);
    }
#endif
}

void UShooterAIRangedSmokeSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    Super::Deinitialize();
}

void UShooterAIRangedSmokeSubsystem::Finish(bool bSuccess, const FString& Message)
{
    if (bFinished) { return; }
    bFinished = true;
    FFileHelper::SaveStringToFile((bSuccess ? TEXT("PASS\n") : TEXT("FAIL\n")) + Message,
        *(Output / TEXT("Result.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogShooterAIRangedSmoke, Display, TEXT("%s: %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), *Message);
    FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
}

bool UShooterAIRangedSmokeSubsystem::Step(float DeltaSeconds)
{
    if (bFinished) { return false; }
    if (FPlatformTime::Seconds() > Deadline)
    {
        Finish(false, FString::Printf(TEXT("Timeout at phase %d; navigation or firing did not complete"), Phase));
        return false;
    }
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    AMyShooter* Player = PC ? Cast<AMyShooter>(PC->GetPawn()) : nullptr;
    AShooterGameMode* GM = World ? Cast<AShooterGameMode>(World->GetAuthGameMode()) : nullptr;
    if (!Player || !GM || !World->HasBegunPlay() || !Player->IsGASInitialized()) { return true; }
    if (Phase == 0)
    {
        if (!GM->StartRound()) { Finish(false, TEXT("Could not start actual map round")); return false; }
        Player->GetCharacterMovement()->DisableMovement();
        // 仅测试模式暂停其他敌人，避免额外敌人的伤害干扰本次路径和单次射击检查。
        for (TActorIterator<AShooterAIController> It(World); It; ++It)
        {
            It->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(*It);
            AShooterCharacterBase* Pawn = Cast<AShooterCharacterBase>(It->GetPawn());
            if (!Pawn) { continue; }
            if (!TestAI.IsValid()) { TestAI = *It; TestEnemy = Pawn; }
            else { Pawn->GetCharacterMovement()->DisableMovement(); Pawn->SetActorHiddenInGame(true); Pawn->SetActorEnableCollision(false); }
        }
        UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if (!Nav || !TestEnemy.IsValid()) { Finish(false, TEXT("Actual map lacks navigation or enemy")); return false; }
        EnemyStart = TestEnemy->GetActorLocation();
        const FVector OriginalPlayer = Player->GetActorLocation();
        FVector Eye; FRotator EyeRotation;
        TestEnemy->GetActorEyesViewPoint(Eye, EyeRotation);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AIRangedSmoke), false, TestEnemy.Get()); Query.AddIgnoredActor(Player);
        bool bFound = false;
        // 使用地图现有 NavMesh，选取 14~20 米内可达且初始可见的落点，不生成临时导航数据。
        for (float Radius : {1800.f, 1600.f, 1400.f})
        {
            for (int32 Angle = 0; Angle < 360 && !bFound; Angle += 30)
            {
                const FVector Offset = FRotator(0, Angle, 0).Vector() * Radius;
                FNavLocation Point;
                if (!Nav->ProjectPointToNavigation(EnemyStart + Offset, Point, FVector(180,180,350))) { continue; }
                const FVector Candidate = Point.Location + FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
                const float Distance = FVector::Dist(EnemyStart, Candidate);
                if (Distance < 1300.f || Distance > 1950.f) { continue; }
                UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World,
                    TestEnemy->GetNavAgentLocation(), Point.Location, TestEnemy.Get());
                if (!Path || !Path->IsValid() || Path->IsPartial()) { continue; }
                Player->SetActorLocation(Candidate, false);
                FVector PlayerEye; Player->GetActorEyesViewPoint(PlayerEye, EyeRotation);
                if (World->LineTraceTestByChannel(Eye, PlayerEye, ECC_Visibility, Query)) { continue; }
                bFound = true;
                const FRotator Facing = (Candidate - EnemyStart).Rotation();
                TestEnemy->SetActorRotation(FRotator(0, Facing.Yaw, 0));
                TestAI->SetControlRotation(Facing);
                PC->SetControlRotation((Eye - PlayerEye).Rotation());
                UE_LOG(LogShooterAIRangedSmoke, Display, TEXT("Real map path: initialDistance=%.1f cm points=%d"), Distance, Path->PathPoints.Num());
            }
            if (bFound) { break; }
        }
        if (!bFound)
        {
            Player->SetActorLocation(OriginalPlayer);
            Finish(false, TEXT("No reachable visible 14-20m pair found on actual map NavMesh")); return false;
        }
        // 重新接管以恢复正常决策；不手动指定目标，必须由前方视野自动发现玩家。
        TestAI->UnPossess(); TestAI->Possess(TestEnemy.Get());
        TestEnemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        FScreenshotRequest::RequestScreenshot(Output / TEXT("01-LongRangeStart.png"), true, false);
        Phase = 1;
    }
    else if (Phase == 1)
    {
        if (World->GetTimeSeconds() >= NextDiagnosticTime)
        {
            NextDiagnosticTime = World->GetTimeSeconds() + 2.0;
            UE_LOG(LogShooterAIRangedSmoke, Display, TEXT("Navigation progress: state=%d move=%d target=%s enemy=%s player=%s velocity=%s"),
                int32(TestAI->GetCombatState()), int32(TestAI->GetMoveStatus()), *GetNameSafe(TestAI->GetCombatTarget()),
                *TestEnemy->GetActorLocation().ToString(), *Player->GetActorLocation().ToString(), *TestEnemy->GetVelocity().ToString());
        }
        bSawChase |= TestAI->GetCombatState() == EShooterAIState::Chasing;
        if (Player->GetGASHealth() < 100.f)
        {
            const float Distance = FVector::Dist(TestEnemy->GetActorLocation(), Player->GetActorLocation());
            const float Travel = FVector::Dist(EnemyStart, TestEnemy->GetActorLocation());
            if (!bSawChase || Travel < 100.f || Distance <= 800.f || Distance > 1205.f)
            {
                Finish(false, FString::Printf(TEXT("Unexpected ranged path: chase=%d travel=%.1f distance=%.1f"), bSawChase, Travel, Distance)); return false;
            }
            ShotHealth = Player->GetGASHealth(); ShotTime = World->GetTimeSeconds();
            EnemyStart = TestEnemy->GetActorLocation();
            UE_LOG(LogShooterAIRangedSmoke, Display, TEXT("First shot after NavMesh movement: travel=%.1f cm distance=%.1f cm health=%.1f"), Travel, Distance, ShotHealth);
            FScreenshotRequest::RequestScreenshot(Output / TEXT("02-RangedShot.png"), true, false);
            Phase = 2;
        }
    }
    else if (Phase == 2 && World->GetTimeSeconds() - ShotTime > 0.6f)
    {
        const float Drift = FVector::Dist(EnemyStart, TestEnemy->GetActorLocation());
        if (Drift < 50.f || Player->GetGASHealth() != ShotHealth)
        {
            Finish(false, FString::Printf(TEXT("AI must move while respecting firing cooldown: drift=%.1f health=%.1f"), Drift, Player->GetGASHealth())); return false;
        }
        Finish(true, FString::Printf(TEXT("Actual map auto-acquisition, NavMesh approach, ranged shot over 8m, moving cooldown passed; finalDistance=%.1f cm."),
            FVector::Dist(TestEnemy->GetActorLocation(), Player->GetActorLocation())));
        return false;
    }
    return true;
}