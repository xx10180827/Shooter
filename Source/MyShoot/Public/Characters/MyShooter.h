#pragma once

#include "CoreMinimal.h"
#include "Characters/ShooterCharacterBase.h"
#include "MyShooter.generated.h"

class UShooterWeaponComponent;
class UShooterAimComponent;
class UShooterWeaponPresentationComponent;
class UShooterRecoilComponent;
class UShooterInteractionComponent;
class UShooterDashComponent;

/** 玩家角色：继承公共 GAS 与死亡流程；蓝图配置模型、AnimBP 和输入，武器组件集中管理射击。 */
UCLASS()
class MYSHOOT_API AMyShooter : public AShooterCharacterBase
{
    GENERATED_BODY()

public:
    AMyShooter();

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon")
    UShooterWeaponComponent* GetShooterWeapon() const { return ShooterWeapon; }

    UFUNCTION(BlueprintPure, Category="Shooter|Aim")
    UShooterAimComponent* GetShooterAim() const { return ShooterAim; }
protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void UnPossessed() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shooter|Aim")
    TObjectPtr<UShooterAimComponent> ShooterAim;
    /** 独立视角后坐力组件，与手臂动画分开配置。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shooter|Weapon")
    TObjectPtr<UShooterRecoilComponent> ShooterRecoil;
    /** 附近物品与准星检测独立于武器逻辑。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shooter|Interaction")
    TObjectPtr<UShooterInteractionComponent> ShooterInteraction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shooter|Dash") TObjectPtr<UShooterDashComponent> ShooterDash;
    void DashInput();
    void InteractInput();
    void ToggleAimInput();
    void EquipPrimaryInput();
    void EquipSecondaryInput();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Shooter|Weapon")
    TObjectPtr<UShooterWeaponPresentationComponent> WeaponPresentation;
    /** 原生默认组件会自动出现在 Shooter 蓝图中，无需再手动添加一个。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooter|Weapon")
    TObjectPtr<UShooterWeaponComponent> ShooterWeapon;
};
