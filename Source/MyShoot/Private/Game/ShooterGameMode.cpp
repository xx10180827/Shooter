#include "Game/ShooterGameMode.h"
#include "Characters/ShooterCharacterBase.h"
#include "Characters/MyShooter.h"
#include "AI/ShooterAIController.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Player/ShooterPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AShooterGameMode::AShooterGameMode() { bPauseable = true; }

bool AShooterGameMode::IsCombatAllowed(const UObject* Context)
{
    const UWorld* World = Context ? Context->GetWorld() : nullptr;
    const AShooterGameMode* GM = World ? Cast<AShooterGameMode>(World->GetAuthGameMode()) : nullptr;
    return !GM || GM->RoundState == EShooterRoundState::Playing;
}
void AShooterGameMode::StartPlay()
{
    Super::StartPlay();
    // Actor BeginPlay 的登记和这里的补扫都幂等；收集完成前不会判零敌人胜利。
    for (TActorIterator<AShooterCharacterBase> It(GetWorld()); It; ++It) { RegisterCombatant(*It); }
    FrameEndHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &AShooterGameMode::ResolveAtFrameEnd);
    SetRoundState(EShooterRoundState::Menu);
    if (UGameplayStatics::HasOption(OptionsString, TEXT("AutoStart"))) { StartRound(); }
}
void AShooterGameMode::RegisterCombatant(AShooterCharacterBase* Character)
{
    if (!IsValid(Character) || RegisteredCharacters.Contains(Character)) { return; }
    const bool bPlayer = Character->IsA<AMyShooter>();
    const bool bEnemy = !bPlayer && Character->AIControllerClass
        && Character->AIControllerClass->IsChildOf(AShooterAIController::StaticClass());
    if (!bPlayer && !bEnemy) { return; }
    RegisteredCharacters.Add(Character);
    Character->OnGASDeathConfirmed.AddUniqueDynamic(this, &AShooterGameMode::HandleDeath);
    Character->OnDestroyed.AddUniqueDynamic(this, &AShooterGameMode::HandleDestroyed);
    if (bPlayer) { PlayerCharacter = Character; }
    else
    {
        bHadEnemies = true;
        if (!Character->HasGASDeathStarted()) { LivingEnemies.Add(Character); }
    }
}
void AShooterGameMode::HandleDeath(AShooterCharacterBase* Character)
{
    if (RoundState != EShooterRoundState::Playing) { return; }
    if (Character == PlayerCharacter.Get()) { bPlayerLost = true; }
    LivingEnemies.Remove(Character);
    bPendingOutcome = true;
}
void AShooterGameMode::HandleDestroyed(AActor* Actor)
{
    if (RoundState == EShooterRoundState::Playing && Actor == PlayerCharacter.Get())
    {
        bPlayerLost = true; bPendingOutcome = true;
    }
    // 敌人必须经过死亡入口才算击杀；直接删除活敌人不会伪造胜利。
}
void AShooterGameMode::ResolveAtFrameEnd(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
    if (World != GetWorld() || !bPendingOutcome || RoundState != EShooterRoundState::Playing) { return; }
    bPendingOutcome = false;
    // 所有本帧角色/定时器完成后统一判定；同帧最后一个敌人与玩家死亡时失败优先。
    if (bPlayerLost || !PlayerCharacter.IsValid() || PlayerCharacter->HasGASDeathStarted())
    {
        SetRoundState(EShooterRoundState::Lost);
    }
    else if (bHadEnemies && LivingEnemies.IsEmpty()) { SetRoundState(EShooterRoundState::Won); }
}
bool AShooterGameMode::StartRound()
{
    if (RoundState != EShooterRoundState::Menu || !PlayerCharacter.IsValid()
        || !PlayerCharacter->IsGASInitialized() || PlayerCharacter->HasGASDeathStarted()) { return false; }
    SetRoundState(EShooterRoundState::Playing);
    return true;
}
void AShooterGameMode::TogglePause()
{
    if (RoundState == EShooterRoundState::Playing) { SetRoundState(EShooterRoundState::Paused); }
    else if (RoundState == EShooterRoundState::Paused) { SetRoundState(EShooterRoundState::Playing); }
}
void AShooterGameMode::SetRoundState(EShooterRoundState NewState)
{
    RoundState = NewState;
    const bool bPlaying = NewState == EShooterRoundState::Playing;
    if (!bPlaying)
    {
        for (TActorIterator<AShooterCharacterBase> It(GetWorld()); It; ++It)
        {
            It->GetCharacterMovement()->StopMovementImmediately();
            if (UShooterWeaponComponent* Weapon = It->FindComponentByClass<UShooterWeaponComponent>())
            {
                Weapon->StopFiring(); Weapon->CancelReloading();
            }
        }
        for (TActorIterator<AShooterAIController> It(GetWorld()); It; ++It) { It->SuspendCombat(); }
    }
    OnRoundChanged.Broadcast(NewState);
}
void AShooterGameMode::ReloadLevel(bool bAutoStart)
{
    if (bTravelRequested) { return; }
    bTravelRequested = true;
    // 关卡重载重建角色、能力、UI、定时器；不尝试原地复活死亡 Pawn。
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) { PC->SetPause(false); }
    const FString Map = UGameplayStatics::GetCurrentLevelName(this, true);
    UGameplayStatics::OpenLevel(this, FName(*Map), true, bAutoStart ? TEXT("AutoStart=1") : TEXT(""));
}
void AShooterGameMode::RestartRound()
{
    if (RoundState == EShooterRoundState::Won || RoundState == EShooterRoundState::Lost) { ReloadLevel(true); }
}
void AShooterGameMode::ReturnToMenu()
{
    if (RoundState != EShooterRoundState::Menu) { ReloadLevel(false); }
}
void AShooterGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    FWorldDelegates::OnWorldPostActorTick.Remove(FrameEndHandle);
    for (const TWeakObjectPtr<AShooterCharacterBase>& Entry : RegisteredCharacters)
    {
        if (AShooterCharacterBase* Character = Entry.Get())
        {
            Character->OnGASDeathConfirmed.RemoveDynamic(this, &AShooterGameMode::HandleDeath);
            Character->OnDestroyed.RemoveDynamic(this, &AShooterGameMode::HandleDestroyed);
        }
    }
    RegisteredCharacters.Empty(); LivingEnemies.Empty();
    Super::EndPlay(Reason);
}
