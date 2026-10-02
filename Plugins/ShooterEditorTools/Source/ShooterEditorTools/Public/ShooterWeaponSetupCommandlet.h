#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterWeaponSetupCommandlet.generated.h"
/** T14 导入用户 FBX 并生成武器数据资产；只修改指定新资源与 Shooter 蓝图，不保存地图。 */
UCLASS()
class UShooterWeaponSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterWeaponSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
