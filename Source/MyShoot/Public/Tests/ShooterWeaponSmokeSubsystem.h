#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "ShooterWeaponSmokeSubsystem.generated.h"
/** 仅 -ShooterWeaponSmoke 启动真实地图输入/资产检查，正常游戏不执行。 */
UCLASS()
class MYSHOOT_API UShooterWeaponSmokeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    bool Step(float DeltaTime);
    bool StepPickup(float DeltaTime);
    bool bPickup=false;
    bool Check(bool bOK,const TCHAR* Message);
    void Finish(bool bOK,const FString& Message);
    void Next(int32 NewPhase,float Delay=.4f);
    void Capture(const TCHAR* Name);
    FTSTicker::FDelegateHandle TickHandle;
    FString Output;
    TArray<FString> Results;
    int32 Phase=0,AmmoBefore=0;
    double Deadline=0,NextTime=0;
    FTransform CameraBefore;
    float PitchBefore=0.f,SavedCameraFOV=90.f;
    bool bFinished=false;
    bool bPolish=false;
};
