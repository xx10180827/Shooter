#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterAIHUDCommandlet.generated.h"

/** T06 一次性资源接入与验证工具，只编译进 Editor 模块。 */
UCLASS()
class SHOOTEREDITORTOOLS_API UShooterAIHUDCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterAIHUDCommandlet();
    virtual int32 Main(const FString& Params) override;
};
