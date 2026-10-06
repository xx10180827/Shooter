#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/ShooterCombatResult.h"
#include "Game/ShooterGameMode.h"
#include "ShooterCombatFeedbackComponent.generated.h"
class UShooterWeaponComponent;
class UShooterFeedbackConfig;
class UAudioComponent;
class USoundWave;
class UParticleSystemComponent;

/** 本地反馈层：订阅单发结果，合并确认音与多杀，控制粒子预算和生命周期。 */
UCLASS(ClassGroup=(Shooter), meta=(BlueprintSpawnableComponent))
class MYSHOOT_API UShooterCombatFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShooterCombatFeedbackComponent();
    void ObserveWeapon(UShooterWeaponComponent* Weapon);
    void HandleRoundState(EShooterRoundState State);
    void ClearFeedback();
    UFUNCTION() void HandleShotResolved(const FShooterShotResult& Result);
    const UShooterFeedbackConfig* GetConfig() const { return Config; }
    float GetMarkerAlpha() const;
    float GetIconAlpha() const;
    float GetIconScale() const;
    bool IsKillMarker() const { return bKillMarker; }
    int32 GetMultiKillCount() const { return MultiKillCount; }
    int32 GetConfirmationCount() const { return ConfirmationCount; }
    int32 GetBloodBurstCount() const { return BloodBurstCount; }
    /** 音频引擎报告播放进度后才计数，避免把请求次数误当作真正开始播放。 */
    int32 GetAudioPlaybackCount() const { return AudioPlaybackCount; }
    bool IsConfirmationAudioPlaying() const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditDefaultsOnly, Category="Feedback") TSoftObjectPtr<UShooterFeedbackConfig> ConfigAsset;
private:
    double Now() const;
    void OnConfirmationPlayback(const UAudioComponent* Component, const USoundWave* Sound, float Percent);
    int32 AudioPlaybackCount = 0;
    bool bAwaitingAudioPlayback = false;
    UPROPERTY(Transient) TObjectPtr<UShooterFeedbackConfig> Config;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> ConfirmationAudio;
    TWeakObjectPtr<UShooterWeaponComponent> ObservedWeapon;
    TArray<TWeakObjectPtr<UParticleSystemComponent>> ActiveBlood;
    double MarkerStarted = -1000.;
    double IconStarted = -1000.;
    double LastKillTime = -1000.;
    float MarkerDuration = .16f;
    bool bKillMarker = false;
    int32 MultiKillCount = 0;
    int32 LastShotId = 0;
    int32 ConfirmationCount = 0;
    int32 BloodBurstCount = 0;
};
