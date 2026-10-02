#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/ShooterGameMode.h"
#include "ShooterWeaponPresentationComponent.generated.h"
class UShooterWeaponComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USceneComponent;
class UAudioComponent;
class UAnimMontage;
class AShooterCharacterBase;

/** 武器外观适配层：保留原步枪与蓝图节点，仅在切换时显示配置模型并同步枪口。 */
UCLASS(ClassGroup=(Shooter))
class MYSHOOT_API UShooterWeaponPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShooterWeaponPresentationComponent();
    bool IsReloadSoundPlaying() const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION()
    void HandleWeaponChanged(int32 OldSlot, int32 NewSlot);
    UFUNCTION()
    void HandleReloadStarted(float Duration);
    UFUNCTION()
    void HandleReloadFinished(bool bSucceeded);
    UFUNCTION()
    void HandleDeath(AShooterCharacterBase* Character);
    UFUNCTION()
    void HandleRoundChanged(EShooterRoundState State);
    void StopReloadSound();
    void StopFireMontages();
    TWeakObjectPtr<UAudioComponent> ReloadAudio;
    TWeakObjectPtr<UAnimMontage> PreviousFireMontage;
    TWeakObjectPtr<UAnimMontage> PreviousAimFireMontage;
    TWeakObjectPtr<AShooterGameMode> GameMode;
    TWeakObjectPtr<UShooterWeaponComponent> Weapon;
    TWeakObjectPtr<USkeletalMeshComponent> OriginalMesh;
    TWeakObjectPtr<USceneComponent> Muzzle;
    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> EquippedMesh;
    FVector OriginalMuzzleLocation = FVector::ZeroVector;
    bool bOriginalVisible = true;
};
