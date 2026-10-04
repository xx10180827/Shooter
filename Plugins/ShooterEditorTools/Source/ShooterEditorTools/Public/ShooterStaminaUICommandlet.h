#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterStaminaUICommandlet.generated.h"
/** 导入用户体力框衍生透明图，创建随 GAS 体力填充的 UI 材质，并核对实际玩家闪避距离。 */
UCLASS()
class UShooterStaminaUICommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterStaminaUICommandlet();
    virtual int32 Main(const FString& Params) override;
};