#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerInput.h"
#include "ShooterPlayerInput.generated.h"
/** 在鼠标轴处理层追加用户倍率，不缩放 Controller 的所有旋转，避免改变后坐力。 */
UCLASS()
class MYSHOOT_API UShooterPlayerInput : public UPlayerInput
{
    GENERATED_BODY()
public:
    virtual float MassageAxisInput(FKey Key,float RawValue) override;
};