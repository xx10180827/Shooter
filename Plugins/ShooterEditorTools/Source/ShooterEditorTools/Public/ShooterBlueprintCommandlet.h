#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterBlueprintCommandlet.generated.h"

/** 编辑器专用蓝图检查与迁移工具：默认只导出图结构，显式迁移命令才允许保存资产。 */
UCLASS()
class SHOOTEREDITORTOOLS_API UShooterBlueprintCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UShooterBlueprintCommandlet();
    virtual int32 Main(const FString& Params) override;
};
