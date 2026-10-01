#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "ShooterAwarenessSmokeSubsystem.generated.h"
class AShooterAIController;
class AShooterCharacterBase;
class AMyShooter;

/** 仅 -ShooterAwarenessSmoke 启用：真实地图验证受击、枪声位置记忆与移动射击。 */
UCLASS()
class MYSHOOT_API UShooterAwarenessSmokeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    bool Step(float Delta);
    void Finish(bool bSuccess, const FString& Message);
    bool PlaceHiddenPlayer(AMyShooter* Player);
    bool PlaceVisiblePlayer(AMyShooter* Player);
    void ShootOnce(AMyShooter* Player);
    FTSTicker::FDelegateHandle TickHandle;
    TWeakObjectPtr<AShooterAIController> AI;
    TWeakObjectPtr<AShooterCharacterBase> Enemy;
    TWeakObjectPtr<AActor> Cover;
    FVector FarPosition, EventPosition, StartPosition, ShotPosition, HoldPosition;
    FString Output;
    double Deadline = 0, PhaseTime = 0, NextLogTime = 0;
    int32 Phase = 0;
    float FirstShotHealth = 0;
    bool bFinished = false;
};