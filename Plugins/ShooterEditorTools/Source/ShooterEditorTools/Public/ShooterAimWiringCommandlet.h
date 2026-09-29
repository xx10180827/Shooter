#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterAimWiringCommandlet.generated.h"

/** T10 编辑器接线：-Inspect 检查现有组件，-Verify 校验，默认迁移；不进入游戏模块。 */
UCLASS()
class UShooterAimWiringCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterAimWiringCommandlet();
    virtual int32 Main(const FString& Params) override;
};
