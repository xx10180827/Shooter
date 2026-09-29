#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/ShooterGameMode.h"
#include "ShooterAimComponent.generated.h"
class AMyShooter;
class UCameraComponent;
class UAnimMontage;
class UShooterWeaponComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShooterAimingChanged, bool, bIsAiming);

/** 玩家开镜：统一维护切换状态、GAS 标签、视野和动画混合；不参与伤害结算。 */
UCLASS(ClassGroup=(Shooter), meta=(BlueprintSpawnableComponent))
class MYSHOOT_API UShooterAimComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShooterAimComponent();
    /** 状态实际改变时通知 UI；开镜/退出共用此入口，避免准星与玩法状态脱节。 */
    UPROPERTY(BlueprintAssignable, Category="Shooter|Aim")
    FShooterAimingChanged OnAimingChanged;
    /** 右键按下切换开镜；重复输入由当前状态决定，不创建计时器或额外摄像机。 */
    UFUNCTION(BlueprintCallable, Category="Shooter|Aim")
    void ToggleAiming();
    UFUNCTION(BlueprintCallable, Category="Shooter|Aim")
    bool StartAiming();
    UFUNCTION(BlueprintCallable, Category="Shooter|Aim")
    void StopAiming();
    /** 死亡、换弹或菜单立即复位，避免世界暂停后过渡停在半途。 */
    UFUNCTION(BlueprintCallable, Category="Shooter|Aim")
    void ResetAiming();
    UFUNCTION(BlueprintPure, Category="Shooter|Aim")
    bool IsAiming() const { return bAiming; }
    UFUNCTION(BlueprintPure, Category="Shooter|Aim")
    float GetAimAlpha() const { return AimAlpha; }
    UFUNCTION(BlueprintPure, Category="Shooter|Aim")
    UAnimMontage* GetFireMontage() const;
    bool CanAim() const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    /** 原始 FOV 在 BeginPlay 从现有 Camera 读取，退出瞄准精确恢复。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Aim", meta=(ClampMin="20", ClampMax="110"))
    float AimFieldOfView = 65.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Aim", meta=(ClampMin="0.05", ClampMax="1"))
    float AimTransitionDuration = 0.18f;
    /** 相对角色 Mesh 的瞄准微调，单位厘米；原位置独立缓存，反复切换不累积偏移。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Aim")
    FVector AimMeshOffset = FVector::ZeroVector;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Aim")
    TObjectPtr<UAnimMontage> HipFireMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Aim")
    TObjectPtr<UAnimMontage> AimFireMontage;
private:
    void SetAiming(bool bNewAiming);
    void ApplyView();
    UFUNCTION()
    void HandleDeath(AShooterCharacterBase* DeadCharacter);
    UFUNCTION()
    void HandleReloadStarted(float Duration);
    UFUNCTION()
    void HandleRoundChanged(EShooterRoundState State);
    TWeakObjectPtr<AMyShooter> OwnerCharacter;
    TWeakObjectPtr<UCameraComponent> Camera;
    TWeakObjectPtr<UShooterWeaponComponent> Weapon;
    TWeakObjectPtr<AShooterGameMode> GameMode;
    FVector HipMeshLocation = FVector::ZeroVector;
    float HipFieldOfView = 90.f;
    float AimAlpha = 0.f;
    bool bAiming = false;
    bool bViewCached = false;
};
