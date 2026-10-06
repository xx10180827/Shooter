#include "Combat/ShooterCombatFeedbackComponent.h"
#include "Combat/ShooterFeedbackConfig.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Characters/ShooterCharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"

UShooterCombatFeedbackComponent::UShooterCombatFeedbackComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ConfigAsset = TSoftObjectPtr<UShooterFeedbackConfig>(FSoftObjectPath(TEXT("/Game/Feedback/DA_CombatFeedback.DA_CombatFeedback")));
}
void UShooterCombatFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();
    Config = ConfigAsset.LoadSynchronous();
    if (!Config) { Config = NewObject<UShooterFeedbackConfig>(this); }
}
double UShooterCombatFeedbackComponent::Now() const { return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.; }
void UShooterCombatFeedbackComponent::ObserveWeapon(UShooterWeaponComponent* Weapon)
{
    if (ObservedWeapon.Get() == Weapon) { return; }
    if (ObservedWeapon.IsValid()) { ObservedWeapon->OnShotResolved.RemoveDynamic(this, &ThisClass::HandleShotResolved); }
    ClearFeedback(); LastShotId = 0;
    ObservedWeapon = Weapon;
    if (Weapon) { Weapon->OnShotResolved.AddUniqueDynamic(this, &ThisClass::HandleShotResolved); }
}
void UShooterCombatFeedbackComponent::HandleShotResolved(const FShooterShotResult& Result)
{
    auto* PC = Cast<APlayerController>(GetOwner());
    const auto* Player = PC ? Cast<AShooterCharacterBase>(PC->GetPawn()) : nullptr;
    // 只向实际开枪的本地玩家反馈；拒绝重发结果和已死亡玩家。
    if (!Config || !PC || !PC->IsLocalController() || !Player || Player->HasGASDeathStarted()
        || !AShooterGameMode::IsCombatAllowed(this) || Result.ShotId <= LastShotId) { return; }
    LastShotId = Result.ShotId;
    if (Result.Targets.IsEmpty()) { return; }
    int32 DamagedCount = 0, Kills = 0;
    for (const auto& Target : Result.Targets)
    {
        if (Target.WasDamaged()) { ++DamagedCount; if (Target.bKilled) { ++Kills; } }
    }
    if (!DamagedCount) { return; }
    ++ConfirmationCount;
    const double Time = Now();
    // 击杀提示优先：下一发普通命中不把仍在显示的金色提示覆盖成白色。
    if (Kills || GetMarkerAlpha() <= 0.f || !bKillMarker)
    {
        bKillMarker = Kills > 0;
        MarkerStarted = Time;
        MarkerDuration = FMath::Max(.01f, bKillMarker ? Config->KillMarkerDuration : Config->HitDuration);
    }
    if (Kills)
    {
        MultiKillCount = Time - LastKillTime <= FMath::Max(0.f, Config->MultiKillWindow) ? MultiKillCount + Kills : Kills;
        LastKillTime = Time; IconStarted = Time;
    }
    // 本地确认音复用独立组件：每个有效单发从头播放，不依赖自动销毁后的旧指针。
    USoundBase* Sound = Kills ? Config->KillSound : Config->HitSound;
    bAwaitingAudioPlayback = false;
    if (IsValid(ConfirmationAudio)) { ConfirmationAudio->Stop(); }
    if (Sound)
    {
        if (!IsValid(ConfirmationAudio))
        {
            ConfirmationAudio = UGameplayStatics::CreateSound2D(this, Sound, 1.f, 1.f, 0.f, nullptr, false, false);
            if (ConfirmationAudio)
            {
                ConfirmationAudio->OnAudioPlaybackPercentNative.AddUObject(this, &ThisClass::OnConfirmationPlayback);
            }
        }
        if (ConfirmationAudio)
        {
            ConfirmationAudio->SetSound(Sound);
            ConfirmationAudio->SetVolumeMultiplier(FMath::Clamp(Kills ? Config->KillVolume : Config->HitVolume, 0.f, 1.f));
            bAwaitingAudioPlayback = true;
            ConfirmationAudio->Play(0.f);
        }
    }
    ActiveBlood.RemoveAll([](const auto& Entry) { return !Entry.IsValid() || !Entry->IsActive(); });
    int32 Spawned = 0;
    for (const auto& Target : Result.Targets)
    {
        if (!Target.WasDamaged() || !Config->BloodEffect || Spawned >= FMath::Clamp(Config->MaxBloodBurstsPerShot, 0, 8)) { continue; }
        if (ActiveBlood.Num() >= FMath::Clamp(Config->MaxActiveBloodBursts, 1, 32)) { break; }
        // 沿命中表面向外喷散；没有有效表面法线时回退到入射反方向。
        FVector Normal = Target.Hit.ImpactNormal.GetSafeNormal();
        if (Normal.IsNearlyZero()) { Normal = (Target.Hit.TraceStart - Target.Hit.TraceEnd).GetSafeNormal(); }
        if (Normal.IsNearlyZero()) { Normal = FVector::UpVector; }
        auto* Burst = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Config->BloodEffect,
            Target.Hit.ImpactPoint + Normal * 2.f, Normal.Rotation(), FVector(FMath::Clamp(Config->BloodScale, .1f, 3.f)), true);
        if (Burst) { ActiveBlood.Add(Burst); ++Spawned; ++BloodBurstCount; }
    }
}
void UShooterCombatFeedbackComponent::HandleRoundState(EShooterRoundState State)
{
    // 胜利页保留最后一枪的确认动画；其他非战斗状态清理，避免菜单/重开残留。
    if (State != EShooterRoundState::Playing && State != EShooterRoundState::Won) { ClearFeedback(); }
}
void UShooterCombatFeedbackComponent::ClearFeedback()
{
    bAwaitingAudioPlayback = false;
    if (IsValid(ConfirmationAudio)) { ConfirmationAudio->Stop(); }
    for (const auto& Entry : ActiveBlood) { if (Entry.IsValid()) { Entry->DestroyComponent(); } }
    ActiveBlood.Empty(); MarkerStarted = IconStarted = LastKillTime = -1000.; MultiKillCount = 0; bKillMarker = false;
}
float UShooterCombatFeedbackComponent::GetMarkerAlpha() const
{
    return FMath::Clamp(1.f - float(Now() - MarkerStarted) / FMath::Max(.01f, MarkerDuration), 0.f, 1.f);
}
float UShooterCombatFeedbackComponent::GetIconAlpha() const
{
    const float Duration = Config ? FMath::Max(.1f, Config->KillIconDuration) : .85f;
    const float Age = float(Now() - IconStarted);
    return Age >= 0.f ? FMath::Clamp((Duration - Age) / (Duration * .4f), 0.f, 1.f) : 0.f;
}
float UShooterCombatFeedbackComponent::GetIconScale() const
{
    const float Age = FMath::Max(0.f, float(Now() - IconStarted));
    return Age < .08f ? FMath::Lerp(.7f, 1.15f, Age / .08f) : FMath::Lerp(1.15f, 1.f, FMath::Clamp((Age - .08f) / .12f, 0.f, 1.f));
}
bool UShooterCombatFeedbackComponent::IsConfirmationAudioPlaying() const
{
    return IsValid(ConfirmationAudio) && ConfirmationAudio->IsPlaying();
}
void UShooterCombatFeedbackComponent::OnConfirmationPlayback(const UAudioComponent* Component, const USoundWave* Sound, float Percent)
{
    if (bAwaitingAudioPlayback && Component == ConfirmationAudio && Percent > 0.f)
    {
        bAwaitingAudioPlayback = false;
        ++AudioPlaybackCount;
    }
}
void UShooterCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ObserveWeapon(nullptr); ClearFeedback();
    if (IsValid(ConfirmationAudio)) { ConfirmationAudio->DestroyComponent(); }
    ConfirmationAudio = nullptr;
    Super::EndPlay(Reason);
}
