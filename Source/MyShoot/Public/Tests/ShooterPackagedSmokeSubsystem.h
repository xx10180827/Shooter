#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "ShooterPackagedSmokeSubsystem.generated.h"
class AShooterCharacterBase;

/** 仅 Development 显式 -ShooterSmoke 时运行：在真实打包关卡中验证 UI、战斗和跨关卡重开。 */
UCLASS()
class MYSHOOT_API UShooterPackagedSmokeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    bool Step(float DeltaSeconds);
    bool Check(bool bOK, const FString& Message);
    void Finish(bool bSucceeded, const FString& Message);
    void Next(int32 NewPhase, float Delay = 0.35f);
    void Capture(const TCHAR* Name);
    FTSTicker::FDelegateHandle TickHandle;
    TWeakObjectPtr<UWorld> PreviousWorld;
    TWeakObjectPtr<AShooterCharacterBase> TargetEnemy;
    FString OutputDirectory;
    TArray<FString> Results;
    double Deadline = 0;
    double NextTime = 0;
    float EnemyHealthBeforeShot = 0;
    int32 Phase = 0;
    int32 RestartCount = 0;
    bool bFinished = false;
};
