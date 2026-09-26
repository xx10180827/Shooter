#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"
class UShooterHealthWidget;

/** 本地玩家 HUD 生命周期：只添加到屏幕一次，换 Pawn 重新订阅，退出时解绑移除。 */
UCLASS()
class MYSHOOT_API AShooterPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Shooter|HUD")
    UShooterHealthWidget* GetHealthWidget() const { return HealthWidget; }
protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|HUD")
    TSubclassOf<UShooterHealthWidget> HealthWidgetClass;
private:
    void RefreshHUD();
    UPROPERTY(Transient)
    TObjectPtr<UShooterHealthWidget> HealthWidget;
};
