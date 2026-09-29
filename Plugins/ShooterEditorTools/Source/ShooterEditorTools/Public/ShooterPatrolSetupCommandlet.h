#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterPatrolSetupCommandlet.generated.h"

/** 为原地图配置可编辑巡逻路线；默认只检查，-Apply 才保存地图。 */
UCLASS()
class UShooterPatrolSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterPatrolSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};