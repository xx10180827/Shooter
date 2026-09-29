#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "ShooterAIRangedSmokeSubsystem.generated.h"
class AShooterAIController;
class AShooterCharacterBase;

/** 仅 -ShooterAIRangedSmoke 显式启动时检查实际地图导航和远程射击，普通游戏不运行。 */
UCLASS()
class MYSHOOT_API UShooterAIRangedSmokeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    bool Step(float DeltaSeconds);
    void Finish(bool bSuccess, const FString& Message);
    FTSTicker::FDelegateHandle TickHandle;
    TWeakObjectPtr<AShooterAIController> TestAI;
    TWeakObjectPtr<AShooterCharacterBase> TestEnemy;
    FVector EnemyStart = FVector::ZeroVector;
    FString Output;
    double Deadline = 0;
    double ShotTime = 0;
    double NextDiagnosticTime = 0;
    float ShotHealth = 100;
    int32 Phase = 0;
    bool bSawChase = false;
    bool bFinished = false;
};