#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterPickupSetupCommandlet.generated.h"
/** T15：配置初始所有权，创建可扩展拾取蓝图并增量放置关卡物品。 */
UCLASS()
class UShooterPickupSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterPickupSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
