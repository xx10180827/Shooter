#include "Tests/ShooterAwarenessSmokeSubsystem.h"
#include "AI/ShooterAIController.h"
#include "AI/ShooterPatrolComponent.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Game/ShooterGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAwarenessSmoke, Log, All);
void UShooterAwarenessSmokeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("ShooterAwarenessSmoke")))
    {
        Output = FPaths::ProjectSavedDir()/TEXT("T13/MapSmoke");
        IFileManager::Get().MakeDirectory(*Output, true);
        Deadline = FPlatformTime::Seconds()+180;
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,
            &UShooterAwarenessSmokeSubsystem::Step), 0.01f);
    }
#endif
}
void UShooterAwarenessSmokeSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    Super::Deinitialize();
}
void UShooterAwarenessSmokeSubsystem::Finish(bool bSuccess, const FString& Message)
{
    if (bFinished) { return; }
    bFinished = true;
    FFileHelper::SaveStringToFile((bSuccess ? TEXT("PASS\n") : TEXT("FAIL\n"))+Message,
        *(Output/TEXT("Result.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogShooterAwarenessSmoke, Display, TEXT("%s: %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), *Message);
    FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
}
bool UShooterAwarenessSmokeSubsystem::PlaceHiddenPlayer(AMyShooter* Player)
{
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Point;
    if (!Nav) { return false; }
    bool bFound = false;
    for (float Offset : {0.f,45.f,90.f,135.f,180.f,225.f,270.f,315.f})
    {
        const FVector Direction = FRotator(0, Enemy->GetActorRotation().Yaw+Offset, 0).Vector();
        if (!Nav->ProjectPointToNavigation(Enemy->GetActorLocation()+Direction*700.f, Point, FVector(200,200,400))) { continue; }
        UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),
            Enemy->GetNavAgentLocation(), Point.Location, Enemy.Get());
        if (Path && Path->IsValid() && !Path->IsPartial()) { bFound=true; break; }
    }
    if (!bFound) { return false; }
    EventPosition = Point.Location+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    Player->SetActorLocation(EventPosition);
    // 选择实际可达位置后让敌人背向玩家，避免关卡出生点背后恰好是墙导致测试无落点。
    const FRotator Away = (Enemy->GetActorLocation()-EventPosition).Rotation();
    Enemy->SetActorRotation(FRotator(0,Away.Yaw,0)); AI->SetControlRotation(FRotator(0,Away.Yaw,0));
    // 仅测试世界的可见性遮挡箱：不影响 NavMesh、角色碰撞，也不写回地图。
    if (!Cover.IsValid())
    {
        Cover = GetWorld()->SpawnActor<AActor>();
        UBoxComponent* Box = NewObject<UBoxComponent>(Cover.Get());
        Cover->SetRootComponent(Box); Box->SetBoxExtent(FVector(80,80,250));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Box->SetCanEverAffectNavigation(false); Box->RegisterComponent();
    }
    Cover->SetActorLocation(EventPosition);
    return true;
}
bool UShooterAwarenessSmokeSubsystem::PlaceVisiblePlayer(AMyShooter* Player)
{
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) { return false; }
    FVector Eye; FRotator Rotation;
    Enemy->GetActorEyesViewPoint(Eye, Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AwarenessSmoke), false, Enemy.Get()); Query.AddIgnoredActor(Player);
    for (float Radius : {1800.f,1600.f,1400.f})
    {
        for (int32 Angle=0; Angle<360; Angle+=30)
        {
            FNavLocation Point;
            if (!Nav->ProjectPointToNavigation(Enemy->GetActorLocation()+FRotator(0,Angle,0).Vector()*Radius,
                Point, FVector(150,150,350))) { continue; }
            const FVector Location = Point.Location+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            const float Distance = FVector::Dist(Location,Enemy->GetActorLocation());
            if (Distance<1300 || Distance>1950) { continue; }
            UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),
                Enemy->GetNavAgentLocation(), Point.Location, Enemy.Get());
            if (!Path || !Path->IsValid() || Path->IsPartial()) { continue; }
            Player->SetActorLocation(Location);
            FVector PlayerEye; Player->GetActorEyesViewPoint(PlayerEye,Rotation);
            if (GetWorld()->LineTraceTestByChannel(Eye,PlayerEye,ECC_Visibility,Query)) { continue; }
            const FRotator Facing = (PlayerEye-Eye).Rotation();
            Enemy->SetActorRotation(FRotator(0,Facing.Yaw,0)); AI->SetControlRotation(Facing);
            return true;
        }
    }
    return false;
}
void UShooterAwarenessSmokeSubsystem::ShootOnce(AMyShooter* Player)
{
    // 使用生产武器入口，朝天开枪，以枪声事件而非命中伤害触发听觉。
    Cast<APlayerController>(Player->GetController())->SetControlRotation(FRotator(85,0,0));
    const int32 Before = Player->GetShooterWeapon()->GetCurrentAmmo();
    Player->GetShooterWeapon()->StartFiring(); Player->GetShooterWeapon()->StopFiring();
    if (Player->GetShooterWeapon()->GetCurrentAmmo()!=Before-1) { Finish(false,TEXT("Production gun did not fire exactly once")); }
}
bool UShooterAwarenessSmokeSubsystem::Step(float Delta)
{
    if (bFinished) { return false; }
    if (FPlatformTime::Seconds()>Deadline) { Finish(false,FString::Printf(TEXT("Timeout phase=%d"),Phase)); return false; }
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    AMyShooter* Player = PC ? Cast<AMyShooter>(PC->GetPawn()) : nullptr;
    AShooterGameMode* GM = World ? Cast<AShooterGameMode>(World->GetAuthGameMode()) : nullptr;
    if (!Player || !GM || !World->HasBegunPlay() || !Player->IsGASInitialized()) { return true; }
    const double Now = World->GetTimeSeconds();
    if (Phase==0)
    {
        GM->StartRound(); Player->GetCharacterMovement()->DisableMovement();
        for (TActorIterator<AShooterAIController> It(World); It; ++It)
        {
            AShooterCharacterBase* Pawn = Cast<AShooterCharacterBase>(It->GetPawn());
            if (!Pawn) { continue; }
            if (!AI.IsValid()) { AI=*It; Enemy=Pawn; }
            else
            {
                It->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(*It);
                Pawn->GetCharacterMovement()->DisableMovement(); Pawn->SetActorEnableCollision(false);
            }
        }
        if (!AI.IsValid()) { Finish(false,TEXT("Missing map enemy")); return false; }
        FarPosition = Enemy->GetActorLocation()+FVector(10000,10000,1000);
        Player->SetActorLocation(FarPosition);
        PhaseTime=Now; Phase=1;
        return true;
    }
    if (Phase==12)
    {
        if (Now-PhaseTime<0.7) { return true; }
        if ((AI.IsValid() && AI->GetCombatState()!=EShooterAIState::Dead)
            || (Enemy.IsValid() && !Enemy->GetVelocity().IsNearlyZero()))
        { Finish(false,TEXT("Death did not stop activity")); return false; }
        Finish(true,TEXT("Real map: backside GAS hit awareness, gunshot hearing, snapshot investigation without hidden tracking, expiry patrol return, distant gun ignored, firing during movement, safe-distance hold and follow-up patrol/death passed."));
        return false;
    }
    if (!AI.IsValid() || !Enemy.IsValid()) { Finish(false,TEXT("Unexpected actor cleanup")); return false; }
    if (Now>=NextLogTime)
    {
        NextLogTime=Now+2;
        UE_LOG(LogShooterAwarenessSmoke,Display,TEXT("phase=%d state=%d target=%s health=%.1f speed=%.1f distance=%.1f"),
            Phase,int32(AI->GetCombatState()),*GetNameSafe(AI->GetCombatTarget()),Player->GetGASHealth(),
            Enemy->GetVelocity().Size2D(),FVector::Dist(Enemy->GetActorLocation(),Player->GetActorLocation()));
    }
    if (Phase==1 && Now-PhaseTime>0.6)
    {
        AI->SuspendCombat();
        if (!PlaceHiddenPlayer(Player)) { Finish(false,TEXT("Cannot place hidden player")); return false; }
        if (!UShooterDamageLibrary::ApplyGASDamage(Player,Enemy.Get(),5,Player,FHitResult()))
        { Finish(false,TEXT("GAS hit failed")); return false; }
        PhaseTime=Now; Phase=2;
    }
    else if (Phase==2 && AI->GetCombatState()==EShooterAIState::Investigating)
    {
        if (AI->GetCombatTarget() || !AI->GetInvestigationLocation().Equals(EventPosition,1.f))
        { Finish(false,TEXT("Damage leaked live target or lost event position")); return false; }
        UE_LOG(LogShooterAwarenessSmoke,Display,TEXT("Backside damage perception passed"));
        StartPosition=Enemy->GetActorLocation();
        Player->SetActorLocation(FarPosition); PhaseTime=Now; Phase=3;
    }
    else if (Phase==3 && Now-PhaseTime>4.5)
    {
        if (!AI->GetShooterPatrol()->IsPatrolling() || AI->GetCombatTarget())
        { Finish(false,TEXT("Damage investigation did not expire into patrol")); return false; }
        const float InvestigationTravel = FVector::Dist2D(StartPosition,Enemy->GetActorLocation());
        UE_LOG(LogShooterAwarenessSmoke,Display,TEXT("Damage investigation movement=%.1f cm"),InvestigationTravel);
        if (InvestigationTravel<50) { Finish(false,TEXT("Investigation did not navigate to reachable event")); return false; }
        AI->SuspendCombat();
        if (!PlaceHiddenPlayer(Player)) { Finish(false,TEXT("Cannot place gunshot source")); return false; }
        ShootOnce(Player); PhaseTime=Now; Phase=4;
    }
    else if (Phase==4 && AI->GetCombatState()==EShooterAIState::Investigating)
    {
        if (AI->GetCombatTarget() || !AI->GetInvestigationLocation().Equals(EventPosition,1.f))
        { Finish(false,TEXT("Gunshot did not preserve source snapshot")); return false; }
        Player->SetActorLocation(FarPosition); PhaseTime=Now; Phase=5;
    }
    else if (Phase==5 && Now-PhaseTime>1.f)
    {
        if (!AI->GetInvestigationLocation().Equals(EventPosition,1.f) || AI->GetCombatTarget())
        { Finish(false,TEXT("Hearing followed a hidden moving player")); return false; }
        UE_LOG(LogShooterAwarenessSmoke,Display,TEXT("Gunshot hearing and fixed-position investigation passed"));
        Phase=6;
    }
    else if (Phase==6 && Now-PhaseTime>4.5)
    {
        if (!AI->GetShooterPatrol()->IsPatrolling()) { Finish(false,TEXT("Hearing did not return to patrol")); return false; }
        ShootOnce(Player); PhaseTime=Now; Phase=7;
    }
    else if (Phase==7 && Now-PhaseTime>0.6)
    {
        if (AI->GetCombatState()==EShooterAIState::Investigating || AI->GetCombatTarget())
        { Finish(false,TEXT("Distant gunshot should be outside hearing range")); return false; }
        Cover->Destroy(); Cover.Reset(); AI->SuspendCombat();
        if (!PlaceVisiblePlayer(Player)) { Finish(false,TEXT("No visible navigation pair")); return false; }
        StartPosition=Enemy->GetActorLocation(); Phase=8;
    }
    else if (Phase==8 && Player->GetGASHealth()<100)
    {
        if (Enemy->GetVelocity().Size2D()<50 || FVector::Dist(StartPosition,Enemy->GetActorLocation())<100)
        { Finish(false,TEXT("AI stopped moving before ranged fire")); return false; }
        FirstShotHealth=Player->GetGASHealth(); ShotPosition=Enemy->GetActorLocation();
        UE_LOG(LogShooterAwarenessSmoke,Display,TEXT("First moving shot velocity=%.1f distance=%.1f"),
            Enemy->GetVelocity().Size2D(),FVector::Dist(Enemy->GetActorLocation(),Player->GetActorLocation()));
        Phase=9;
    }
    else if (Phase==9 && Player->GetGASHealth()<FirstShotHealth)
    {
        if (FVector::Dist(ShotPosition,Enemy->GetActorLocation())<50)
        { Finish(false,TEXT("No movement between consecutive shots")); return false; }
        Phase=10;
    }
    else if (Phase==10 && Enemy->GetVelocity().Size2D()<5
        && FVector::Dist(Enemy->GetActorLocation(),Player->GetActorLocation())<480)
    {
        HoldPosition=Enemy->GetActorLocation(); PhaseTime=Now; Phase=11;
    }
    else if (Phase==11 && Now-PhaseTime>0.4)
    {
        if (FVector::Dist(HoldPosition,Enemy->GetActorLocation())>10)
        { Finish(false,TEXT("AI failed to hold safe distance")); return false; }
        UE_LOG(LogShooterAwarenessSmoke,Display,TEXT("Moving fire and safe-distance stop passed"));
        Player->SetActorLocation(FarPosition);
        // 丢失目标回巡逻已在前面两种事件验证；此处再经过决策后验证死亡清理。
        Phase=13; PhaseTime=Now;
    }
    else if (Phase==13 && Now-PhaseTime>0.6)
    {
        if (!AI->GetShooterPatrol()->IsPatrolling()) { Finish(false,TEXT("Combat did not resume patrol")); return false; }
        UShooterDamageLibrary::ApplyGASDamage(Player,Enemy.Get(),10000,Player,FHitResult());
        Phase=12; PhaseTime=Now;
    }
    return true;
}