#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/ShooterGameMode.h"
#include "ShooterRecoilComponent.generated.h"
class AMyShooter;
class APlayerController;
class UShooterWeaponComponent;

/** 真实视角后坐力：成功发射后逐帧增加 ControlRotation，后续射线使用抬高后的视角。 */
UCLASS(ClassGroup=(Shooter))
class MYSHOOT_API UShooterRecoilComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShooterRecoilComponent();
    /** 只取消尚未施加的后坐力，不强行恢复旧朝向，避免覆盖玩家压枪输入。 */
    void CancelPendingRecoil();
    float GetPendingPitch() const { return PendingPitch; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
private:
    UFUNCTION() void HandleShot(bool bBlockingHit,const FHitResult& Hit);
    UFUNCTION() void HandleWeaponChanged(int32 OldSlot,int32 NewSlot);
    UFUNCTION() void HandleReloadStarted(float Duration);
    UFUNCTION() void HandleDeath(AShooterCharacterBase* Character);
    UFUNCTION() void HandleRoundChanged(EShooterRoundState State);
    TWeakObjectPtr<AMyShooter> Character;
    TWeakObjectPtr<UShooterWeaponComponent> Weapon;
    TWeakObjectPtr<APlayerController> KickController;
    TWeakObjectPtr<AShooterGameMode> GameMode;
    float PendingPitch=0.f;
    float RemainingTime=0.f;
};
