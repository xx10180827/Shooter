#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterRoundCommandlet.generated.h"
// T07 的可重复验证入口；迁移拒绝覆盖已有新资源。
UCLASS()
class SHOOTEREDITORTOOLS_API UShooterRoundCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterRoundCommandlet();
    virtual int32 Main(const FString& Params) override;
};
