#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterCombatPresentationCommandlet.generated.h"

/** T09：生成可替换的基础枪声/敌人后坐力资产，接入现有蓝图；-Verify 只读检查。 */
UCLASS()
class UShooterCombatPresentationCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterCombatPresentationCommandlet();
    virtual int32 Main(const FString& Params) override;
};
