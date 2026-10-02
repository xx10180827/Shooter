#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterShotgunPolishCommandlet.generated.h"
/** 复用现有第一人称动画制作霰弹枪副本与合成音效，只更新霰弹枪配置。 */
UCLASS()
class UShooterShotgunPolishCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterShotgunPolishCommandlet();
    virtual int32 Main(const FString& Params) override;
};
