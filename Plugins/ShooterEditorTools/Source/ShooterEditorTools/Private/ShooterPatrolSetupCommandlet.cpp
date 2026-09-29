#include "ShooterPatrolSetupCommandlet.h"
#include "AI/ShooterPatrolRoute.h"
#include "AI/ShooterAIController.h"
#include "Characters/ShooterCharacterBase.h"
#include "Characters/MyShooter.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterPatrolSetup, Log, All);

UShooterPatrolSetupCommandlet::UShooterPatrolSetupCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UShooterPatrolSetupCommandlet::Main(const FString& Params)
{
    if (FParse::Param(*Params, TEXT("AnimationInspect")) || FParse::Param(*Params, TEXT("AnimationApply")))
    {
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/Boot_Shooter_AnimationBP.Boot_Shooter_AnimationBP"));
        if (!BP) { return 10; }
        const bool bApplyAnimation = FParse::Param(*Params, TEXT("AnimationApply"));
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs) for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UAnimGraphNode_BlendSpacePlayer* Player = Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
            {
                UBlendSpace* Blend = Player->Node.GetBlendSpace();
                if (bApplyAnimation && Graph->GetName()==TEXT("Alive") && Blend)
                {
                    const TCHAR* NewPath = TEXT("/Game/Animations/BS_AI_Locomotion");
                    UBlendSpace* MovementBlend = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Animations/BS_AI_Locomotion.BS_AI_Locomotion"), nullptr, LOAD_NoWarn);
                    if (!MovementBlend)
                    {
                        // 保留原 Boot_Shooter_BS，复制后为低速巡逻添加完整移动采样。
                        MovementBlend = DuplicateObject<UBlendSpace>(Blend, CreatePackage(NewPath), TEXT("BS_AI_Locomotion"));
                        MovementBlend->SetFlags(RF_Public | RF_Standalone);
                        UAnimSequence* Run = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Assets/Characters/TTP_Animations/Run_Fwd.Run_Fwd"));
                        if (!Run) { return 11; }
                        const int32 SampleIndex = MovementBlend->AddSample(Run, FVector(180,0,0));
                        if (SampleIndex==INDEX_NONE) { return 12; }
                        // 编辑器修改非 const 资产的采样数据，PostEditChange 后重建插值缓存。
                        FBlendSample& Sample = const_cast<FBlendSample&>(MovementBlend->GetBlendSample(SampleIndex));
                        Sample.RateScale = 0.45f;
                        MovementBlend->ValidateSampleData(); MovementBlend->ResampleData(); MovementBlend->PostEditChange();
                        FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
                        MovementBlend->MarkPackageDirty();
                        const FString File = FPackageName::LongPackageNameToFilename(NewPath, FPackageName::GetAssetPackageExtension());
                        if (!UPackage::SavePackage(MovementBlend->GetOutermost(), MovementBlend, *File, SaveArgs)) { return 13; }
                    }
                    Player->Node.SetBlendSpace(MovementBlend); Player->Node.SetLoop(true); Player->Node.SetPlayRate(1.f);
                    Player->NodeComment = TEXT("T12：0 速度待机，180 巡逻使用完整低速移动姿势，600 保留追踪跑步；原 BlendSpace 资源保留。");
                    Player->bCommentBubbleVisible = true;
                    Blend = MovementBlend;
                }
                UE_LOG(LogShooterPatrolSetup, Display, TEXT("BLEND %s loop=%d playRate=%.2f"),
                    *GetPathNameSafe(Blend), Player->Node.IsLooping(), Player->Node.GetPlayRate());
                if (!Blend) { continue; }
                for (int32 Axis=0; Axis<2; ++Axis)
                {
                    const FBlendParameter& Parameter = Blend->GetBlendParameter(Axis);
                    UE_LOG(LogShooterPatrolSetup, Display, TEXT("AXIS %d %s min=%.1f max=%.1f"), Axis, *Parameter.DisplayName, Parameter.Min, Parameter.Max);
                }
                for (const FBlendSample& Sample : Blend->GetBlendSamples())
                {
                    UE_LOG(LogShooterPatrolSetup, Display, TEXT("SAMPLE %s location=%s rate=%.2f length=%.3f"),
                        *GetNameSafe(Sample.Animation), *Sample.SampleValue.ToString(), Sample.RateScale,
                        Sample.Animation ? Sample.Animation->GetPlayLength() : 0.f);
                }
            }
        }
        if (bApplyAnimation)
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
            FCompilerResultsLog Log;
            FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
            if (Log.NumErrors>0) { return 14; }
            BP->MarkPackageDirty();
            FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
            const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
            if (!UPackage::SavePackage(BP->GetOutermost(), BP, *File, Args)) { return 15; }
        }
        return 0;
    }
    const FString PackageName = TEXT("/Game/Maps/bloodstrike");
    const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
    if (!FEditorFileUtils::LoadMap(Filename, false, true)) { return 1; }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!World || !Nav) { UE_LOG(LogShooterPatrolSetup, Error, TEXT("Map navigation missing")); return 2; }
    const auto Reachable = [World](AShooterCharacterBase* Enemy, const FVector& From, const FVector& To)
    {
        UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To, Enemy);
        return Path && Path->IsValid() && !Path->IsPartial();
    };
    const bool bApply = FParse::Param(*Params, TEXT("Apply"));
    int32 EnemyCount = 0, Created = 0;
    for (TActorIterator<AShooterCharacterBase> It(World); It; ++It)
    {
        AShooterCharacterBase* Enemy = *It;
        if (Enemy->IsA<AMyShooter>() || !Enemy->AIControllerClass
            || !Enemy->AIControllerClass->IsChildOf(AShooterAIController::StaticClass())) { continue; }
        ++EnemyCount;
        AShooterPatrolRoute* Existing = nullptr;
        for (TActorIterator<AShooterPatrolRoute> RouteIt(World); RouteIt; ++RouteIt)
        {
            if (RouteIt->AssignedEnemy == Enemy) { Existing = *RouteIt; break; }
        }
        TArray<FVector> Points;
        if (Existing)
        {
            // 重复运行只验证已有路线，保留用户手工调整的路线点和等待时间。
            for (int32 Index=0; Index<Existing->GetPointCount(); ++Index) { Points.Add(Existing->GetPointLocation(Index)); }
        }
        else
        {
            FNavLocation Start;
            if (!Nav->ProjectPointToNavigation(Enemy->GetActorLocation(), Start, FVector(200,200,400))) { return 3; }
            Points.Add(Start.Location);
            for (int32 Angle=0; Angle<360 && Points.Num()<3; Angle+=45)
            {
                FNavLocation Candidate;
                if (!Nav->ProjectPointToNavigation(Start.Location + FRotator(0,Angle,0).Vector()*500.f,
                    Candidate, FVector(150,150,300))) { continue; }
                bool bSeparated = true;
                for (const FVector& Point : Points) { bSeparated &= FVector::Dist2D(Point, Candidate.Location) >= 300.f; }
                if (!bSeparated || !Reachable(Enemy, Points.Last(), Candidate.Location)
                    || !Reachable(Enemy, Candidate.Location, Points[0])) { continue; }
                Points.Add(Candidate.Location);
            }
        }
        if (Points.Num()<2) { UE_LOG(LogShooterPatrolSetup, Error, TEXT("No route for %s"), *Enemy->GetActorLabel()); return 4; }
        for (int32 Index=0; Index<Points.Num(); ++Index)
        {
            const bool bNeedsNext = !Existing || Existing->IsLooping() || Index+1<Points.Num();
            if (bNeedsNext && !Reachable(Enemy, Points[Index], Points[(Index+1)%Points.Num()]))
            { UE_LOG(LogShooterPatrolSetup, Error, TEXT("Unreachable route segment")); return 5; }
            UE_LOG(LogShooterPatrolSetup, Display, TEXT("%s point[%d]=%s"), *Enemy->GetActorLabel(), Index, *Points[Index].ToString());
        }
        if (!Existing && bApply)
        {
            FActorSpawnParameters Spawn;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AShooterPatrolRoute* Route = World->SpawnActor<AShooterPatrolRoute>(Points[0], FRotator::ZeroRotator, Spawn);
            if (!Route) { return 6; }
            Route->SetActorLabel(TEXT("Patrol_") + Enemy->GetActorLabel());
            Route->AssignedEnemy = Enemy;
            Route->SetWorldPoints(Points, true);
            Route->WaitDuration = 1.5f;
            ++Created;
        }
        else if (!Existing)
        {
            UE_LOG(LogShooterPatrolSetup, Error, TEXT("Route not saved; run -Apply to create it"));
            return 7;
        }
    }
    if (EnemyCount==0) { return 8; }
    if (Created>0)
    {
        World->MarkPackageDirty();
        FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(World->GetOutermost(), World, *Filename, SaveArgs)) { return 9; }
    }
    UE_LOG(LogShooterPatrolSetup, Display, TEXT("PASS: enemies=%d created=%d; all route segments navigable"), EnemyCount, Created);
    return 0;
}