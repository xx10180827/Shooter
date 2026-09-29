#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterAIRangedSetupCommandlet.generated.h"

/** T11：读取实际 AI 蓝图配置；仅 -Apply 时保存远程交战参数，不改原图节点。 */
UCLASS()
class UShooterAIRangedSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterAIRangedSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};