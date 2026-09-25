#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "ShooterWeaponComponent.generated.h"

class UAbilitySystemComponent;
class UShooterFireAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterAmmoChanged, int32, OldAmmo, int32, NewAmmo);  //多播代理
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterShotFired, bool, bBlockingHit, const FHitResult&, HitResult);

/**
 * 单武器的数据与执行层：集中保存配置、弹药及射速时间戳。
 * 输入通过 StartFiring / StopFiring 交给 GAS；蓝图通过事件播放表现，不重复扣弹或结算伤害。
 */
UCLASS(ClassGroup = (Shooter), meta = (BlueprintSpawnableComponent))
class MYSHOOT_API UShooterWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UShooterWeaponComponent();

    /** 请求激活射击能力；重复调用不会叠加连射循环。返回值表示请求被接受。 */
    UFUNCTION(BlueprintCallable, Category = "Shooter|Weapon")
    bool StartFiring();

    /** 松开输入时调用；通过取消能力统一清除射击定时器。 */
    UFUNCTION(BlueprintCallable, Category = "Shooter|Weapon")
    void StopFiring();

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon")
    int32 GetCurrentAmmo() const { return CurrentAmmo; }

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon")
    int32 GetMagazineCapacity() const { return MagazineCapacity; }

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon")
    bool IsFiring() const;

    /** 每次有效发射扣弹后通知 HUD；初始化值通过 GetCurrentAmmo 主动读取。 */
    UPROPERTY(BlueprintAssignable, Category = "Shooter|Weapon")
    FShooterAmmoChanged OnAmmoChanged;

    /** 有弹药且实际发射时通知；打空也会触发。命中结果用于命中特效，不能再次施加伤害。 */
    UPROPERTY(BlueprintAssignable, Category = "Shooter|Weapon")
    FShooterShotFired OnShotFired;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (ClampMin = "0.01"))
    float Damage = 25.0f;

    /** 两次发射的最短间隔，单位秒；不在 Tick 或蓝图里另开射击计时器。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (ClampMin = "0.01"))
    float FireInterval = 0.1f;

    /** 视线射程，单位为 UE 厘米。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (ClampMin = "1.0"))
    float Range = 10000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (ClampMin = "1"))
    int32 MagazineCapacity = 30;

    /** 默认使用 Visibility；目标必须阻挡这个通道才会被命中。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
    // 射击规则只供 GA_Fire 使用，不向蓝图暴露可绕过能力的单发接口。
    friend class UShooterFireAbility;
    bool CanFire() const;
    float GetTimeUntilNextShot() const;
    bool TryFireOneShot();
    void HandleReloadTagChanged(const FGameplayTag Tag, int32 NewCount);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (AllowPrivateAccess = "true"))
    int32 CurrentAmmo = 0;

    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
    FGameplayAbilitySpecHandle FireAbilityHandle;
    FDelegateHandle ReloadTagChangedHandle;

    // 该时间戳跨能力激活保留，防止快速点按通过重启能力绕过射速。
    double NextAllowedShotTime = 0.0;
};
