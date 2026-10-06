#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ShooterFeedbackSetupCommandlet.generated.h"
/** 仅创建打击感专用资源，不保存或修改已有角色、地图和蓝图。 */
UCLASS()
class UShooterFeedbackSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UShooterFeedbackSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
