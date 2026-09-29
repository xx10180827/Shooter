#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "ShooterAimSmokeSubsystem.generated.h"
class AShooterCharacterBase;

/** 仅显式 -ShooterAimSmoke 启动时验证真实关卡右键开镜和 AI 子弹；正常游戏不注册 Tick。 */
UCLASS()
class MYSHOOT_API UShooterAimSmokeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    bool Step(float DeltaTime);
    bool Check(bool bOK,const TCHAR* Message);
    void Finish(bool bOK,const FString& Message);
    void Next(int32 NewPhase,float Delay=0.35f);
    void Capture(const TCHAR* Name);
    FTSTicker::FDelegateHandle TickHandle;
    TWeakObjectPtr<AShooterCharacterBase> Enemy;
    TArray<FString> Results;
    FString Output;
    double Deadline=0,NextTime=0;
    FVector FlatMuzzle = FVector::ZeroVector;
    float HipFOV=90,HealthAfterAIShot=0;
    int32 Phase=0,AmmoBefore=0;
    bool bFinished=false;
};
