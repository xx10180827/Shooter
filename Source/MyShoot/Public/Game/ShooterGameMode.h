#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterGameMode.generated.h"
class AShooterCharacterBase;

UENUM(BlueprintType)
enum class EShooterRoundState : uint8 { Menu, Playing, Paused, Won, Lost };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShooterRoundChanged, EShooterRoundState, State);

/** 单机对局规则：登记敌人、帧末统一判定胜负，界面只发送请求。 */
UCLASS()
class MYSHOOT_API AShooterGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AShooterGameMode();
    virtual void StartPlay() override;
    UFUNCTION(BlueprintPure, Category="Shooter|Round")
    EShooterRoundState GetRoundState() const { return RoundState; }
    UFUNCTION(BlueprintPure, Category="Shooter|Round")
    int32 GetRemainingEnemies() const { return LivingEnemies.Num(); }
    UFUNCTION(BlueprintCallable, Category="Shooter|Round")
    bool StartRound();
    UFUNCTION(BlueprintCallable, Category="Shooter|Round")
    void TogglePause();
    UFUNCTION(BlueprintCallable, Category="Shooter|Round")
    void RestartRound();
    UFUNCTION(BlueprintCallable, Category="Shooter|Round")
    void ReturnToMenu();
    void RegisterCombatant(AShooterCharacterBase* Character);
    // 没有本 GameMode 的独立测试世界仍沿用此前行为。
    static bool IsCombatAllowed(const UObject* Context);
    UPROPERTY(BlueprintAssignable, Category="Shooter|Round")
    FShooterRoundChanged OnRoundChanged;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void SetRoundState(EShooterRoundState NewState);
    void ReloadLevel(bool bAutoStart);
    void ResolveAtFrameEnd(UWorld* World, ELevelTick TickType, float DeltaSeconds);
    UFUNCTION()
    void HandleDeath(AShooterCharacterBase* Character);
    UFUNCTION()
    void HandleDestroyed(AActor* Actor);
    UPROPERTY(VisibleInstanceOnly, Category="Shooter|Round")
    EShooterRoundState RoundState = EShooterRoundState::Menu;
    TWeakObjectPtr<AShooterCharacterBase> PlayerCharacter;
    TSet<TWeakObjectPtr<AShooterCharacterBase>> RegisteredCharacters;
    TSet<TWeakObjectPtr<AShooterCharacterBase>> LivingEnemies;
    FDelegateHandle FrameEndHandle;
    bool bHadEnemies = false;
    bool bPendingOutcome = false;
    bool bPlayerLost = false;
    bool bTravelRequested = false;
};
