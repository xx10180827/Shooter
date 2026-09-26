#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
#include "ShooterWeaponComponent.generated.h"

class UAbilitySystemComponent;
class UShooterFireAbility;
class UShooterReloadAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterAmmoChanged, int32, OldAmmo, int32, NewAmmo);  //多播代理
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterShotFired, bool, bBlockingHit, const FHitResult&, HitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShooterReloadStarted, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShooterReloadFinished, bool, bSucceeded);

/**
 * 单武器的数据与执行层：集中保存配置、弹匣、备用弹药及射速时间戳。
 * 输入通过组件接口交给 GAS；蓝图通过事件播放表现，不重复扣弹或结算伤害。
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

    /** 按 R 时请求换弹。满弹、无备用弹药、死亡或已在换弹时返回 false。 */
    UFUNCTION(BlueprintCallable, Category = "Shooter|Weapon|Reload")
    bool StartReloading();

    /** 取消不会补弹；后续武器切换等行为可复用，死亡由 GAS 自动取消。 */
    UFUNCTION(BlueprintCallable, Category = "Shooter|Weapon|Reload")
    void CancelReloading();

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon")
    int32 GetCurrentAmmo() const { return CurrentAmmo; }

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon")
    int32 GetMagazineCapacity() const { return MagazineCapacity; }

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon")
    int32 GetReserveAmmo() const { return ReserveAmmo; }

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon|Reload")
    float GetReloadDuration() const { return ReloadDuration; }

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon")
    bool IsFiring() const;

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon|Reload")
    bool IsReloading() const;

    UFUNCTION(BlueprintPure, Category = "Shooter|Weapon|Reload")
    bool CanReload() const;

    /** 弹匣变化通知；新角色的初始值通过 GetCurrentAmmo 主动读取。 */
    UPROPERTY(BlueprintAssignable, Category = "Shooter|Weapon")
    FShooterAmmoChanged OnAmmoChanged;

    UPROPERTY(BlueprintAssignable, Category = "Shooter|Weapon")
    FShooterAmmoChanged OnReserveAmmoChanged;

    /** 实际发射才通知，打空也会触发；不能在事件里再次施加伤害。 */
    UPROPERTY(BlueprintAssignable, Category = "Shooter|Weapon")
    FShooterShotFired OnShotFired;

    /** 只在成功开始换弹时广播一次，Duration 用于让动画播放时长匹配规则。 */
    UPROPERTY(BlueprintAssignable, Category = "Shooter|Weapon|Reload")
    FShooterReloadStarted OnReloadStarted;

    /** true 表示已完成弹药转移；false 表示取消。此时本能力的换弹标签已经移除。 */
    UPROPERTY(BlueprintAssignable, Category = "Shooter|Weapon|Reload")
    FShooterReloadFinished OnReloadFinished;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (ClampMin = "0.01"))
    float Damage = 25.0f;

    /** 两次发射的最短间隔，单位秒。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (ClampMin = "0.01"))
    float FireInterval = 0.1f;

    /** 视线射程，单位为 UE 厘米。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (ClampMin = "1.0"))
    float Range = 10000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (ClampMin = "1"))
    int32 MagazineCapacity = 30;

    /** 备用弹药独立于弹匣，初始 90 发；每个新角色重新初始化。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon|Reload", meta = (ClampMin = "0"))
    int32 InitialReserveAmmo = 90;

    /** 唯一补弹计时依据，单位秒；动画通知不修改弹药。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon|Reload", meta = (ClampMin = "0.01"))
    float ReloadDuration = 1.5f;

    /** 默认使用 Visibility；目标必须阻挡这个通道才会被命中。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapon")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
    // 规则只供对应能力使用，蓝图不能绕过能力直接扣弹、补弹。
    friend class UShooterFireAbility;
    friend class UShooterReloadAbility;
    bool CanUseWeapon() const;
    bool CanFire() const;
    bool CanCompleteReload() const;
    bool CommitReloadAmmo(int32& OutOldAmmo, int32& OutOldReserve);
    void PublishReloadEnd(bool bSucceeded, bool bNotify, int32 OldAmmo, int32 OldReserve);
    float GetTimeUntilNextShot() const;
    bool TryFireOneShot();
    void HandleReloadTagChanged(const FGameplayTag Tag, int32 NewCount);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (AllowPrivateAccess = "true"))
    int32 CurrentAmmo = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Weapon", meta = (AllowPrivateAccess = "true"))
    int32 ReserveAmmo = 0;

    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
    FGameplayAbilitySpecHandle FireAbilityHandle;
    FGameplayAbilitySpecHandle ReloadAbilityHandle;
    FDelegateHandle ReloadTagChangedHandle;

    // 状态移除和弹药通知期间暂不接受新输入，避免回调重入打乱同一次原子补弹。
    bool bReloadEnding = false;
    bool bShuttingDown = false;

    // 跨能力激活保留，防止快速点按通过重启能力绕过射速。
    double NextAllowedShotTime = 0.0;
};
