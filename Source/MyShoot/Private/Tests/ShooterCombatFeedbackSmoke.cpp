#include "Tests/ShooterWeaponSmokeSubsystem.h"
#include "Combat/ShooterCombatFeedbackComponent.h"
#include "Combat/ShooterFeedbackConfig.h"
#include "Characters/MyShooter.h"
#include "Player/ShooterPlayerController.h"
#include "AI/ShooterAIController.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Weapons/ShooterAimComponent.h"
#include "Game/ShooterGameMode.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "GAS/ShooterGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Particles/ParticleSystemComponent.h"
#include "ParticleEmitterInstances.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#endif

// 仅显式参数启动；真实地图、玩家控制器、现有武器和 GAS 链路，不保存地图改动。
bool UShooterWeaponSmokeSubsystem::StepCombatFeedback(float)
{
    if (bFinished) { return false; }
    const double Time = FPlatformTime::Seconds();
    if (Time > Deadline) { Finish(false, FString::Printf(TEXT("Feedback timeout phase %d"), Phase)); return false; }
    if (Time < NextTime) { return true; }
    auto* World = GetWorld(); auto* PC = World ? Cast<AShooterPlayerController>(World->GetFirstPlayerController()) : nullptr;
    auto* Player = PC ? Cast<AMyShooter>(PC->GetPawn()) : nullptr;
    auto* GM = World ? Cast<AShooterGameMode>(World->GetAuthGameMode()) : nullptr;
    if (!Player || !GM || !Player->IsGASInitialized()) { return true; }
    auto* W = Player->GetShooterWeapon(); auto* F = PC->GetCombatFeedback();
    auto* Target = Cast<AShooterCharacterBase>(FeedbackTarget.Get());
    auto Aim = [&]()
    {
        auto* Camera = Player->FindComponentByClass<UCameraComponent>();
        const FVector Eye = Camera ? Camera->GetComponentLocation() : Player->GetPawnViewLocation();
        PC->SetControlRotation((Target->GetActorLocation() + FVector(0,0,30) - Eye).Rotation()); PC->PlayerCameraManager->UpdateCamera(.016f);
    };
    auto Fire = [&]() { Aim(); const bool Accepted = W->StartFiring(); W->StopFiring(); return Accepted; };
    switch (Phase)
    {
    case 0:
#if WITH_EDITOR
        if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) { return true; }
#endif
        if (!Check(F && F->GetConfig() && F->GetConfig()->KillIcon && F->GetConfig()->BloodEffect, TEXT("Real controller loads feedback config, icon and blood"))) { return false; }
        GM->StartRound(); Player->GetAbilitySystemComponent()->AddLooseGameplayTag(ShooterGameplayTags::State_Invulnerable);
        for (TActorIterator<AShooterAIController> It(World); It; ++It)
        {
            It->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(*It);
            auto* Enemy = Cast<AShooterCharacterBase>(It->GetPawn());
            if (Enemy) { Enemy->GetCharacterMovement()->DisableMovement(); if (!FeedbackTarget.IsValid()) { FeedbackTarget = Enemy; } }
        }
        if (!Check(FeedbackTarget.IsValid(), TEXT("Existing blueprint enemy available"))) { return false; }
        Target = CastChecked<AShooterCharacterBase>(FeedbackTarget.Get());
        Player->SetActorLocation(FVector(-3070,1450,820), false, nullptr, ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->DisableMovement();
        Target->SetActorLocation(FVector(-2770,1450,820), false, nullptr, ETeleportType::TeleportPhysics); Target->SetActorRotation(FRotator(0,180,0));
        Target->GetAbilitySystemComponent()->SetNumericAttributeBase(UShooterAttributeSet::GetMaxHealthAttribute(), 500.f);
        Target->GetAbilitySystemComponent()->SetNumericAttributeBase(UShooterAttributeSet::GetHealthAttribute(), 500.f);
        // 预热粒子材质的运行时变体，避免首次命中截图发生在着色器准备期间。
        UGameplayStatics::SpawnEmitterAtLocation(World, F->GetConfig()->BloodEffect, Player->GetActorLocation() + FVector(0,0,2000));
        Aim(); Next(1,2.f); break;
    case 1:
#if WITH_EDITOR
        FAssetCompilingManager::Get().FinishAllCompilation();
        GShaderCompilingManager->FinishAllCompilation();
#endif
        if (!Check(Fire() && F->GetConfirmationCount()==1 && !F->IsKillMarker(), TEXT("Real rifle shot confirms actual nonlethal damage once"))) { return false; }
        Next(10,.065f); break;
    case 10:
        Capture(TEXT("01-HipHit.png")); Next(2,.35f); break;
    case 2:
        // 截图会阻塞渲染线程；淡出按游戏时间检查，避免真实时间已经过去但世界尚未推进。
        if (F->GetMarkerAlpha()>0.f) { return true; }
        if (!Check(F->GetMarkerAlpha()==0.f, TEXT("Four-line hit marker expires"))) { return false; }
        Player->GetShooterAim()->StartAiming(); Next(3,.25f); break;
    case 3:
        if (!Check(Fire() && F->GetConfirmationCount()==2, TEXT("ADS confirms hit and keeps aim reticle"))) { return false; }
        Next(4,.065f); break;
    case 4:
    {
        int32 Alive = 0;
        for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
        {
            if (It->GetWorld()==World && It->Template==F->GetConfig()->BloodEffect)
            {
                Alive += It->GetNumActiveParticles();
                for (auto* Emitter : It->EmitterInstances)
                {
                    if (Emitter && Emitter->ActiveParticles > 0)
                    {
                        const auto* Particle = Emitter->GetParticle(0);
                        UE_LOG(LogTemp, Display, TEXT("BLOOD live=%d color=%s size=%s location=%s relative=%.3f component=%s bounds=%s"), Emitter->ActiveParticles,
                            *Particle->Color.ToString(), *Particle->Size.ToString(), *Particle->Location.ToString(), Particle->RelativeTime, *It->GetComponentLocation().ToString(), *It->Bounds.BoxExtent.ToString());
                    }
                }
            }
        }
        if (!Check(F->GetBloodBurstCount()==2 && Alive>0, TEXT("Blood burst contains live particles in actual world"))) { return false; }
                if (!Check(F->GetAudioPlaybackCount()==2, TEXT("First and second hit both receive audio-render playback progress"))) { return false; }
        Capture(TEXT("02-ADSHitBlood.png")); Next(20,.20f); break;
    }
    // 连续三发：跨过上一次短音结束，再次命中仍须收到音频引擎进度。
    case 20:
    case 22:
    case 24:
        if (!Check(Fire() && F->IsConfirmationAudioPlaying(), TEXT("Repeated hit restarts confirmation audio"))) { return false; }
        Next(Phase+1,.065f); break;
    case 21:
    case 23:
    case 25:
        if (!Check(F->GetAudioPlaybackCount()==F->GetConfirmationCount(), TEXT("Every repeated hit reaches audio renderer once"))) { return false; }
        UE_LOG(LogTemp, Display, TEXT("AUDIO verified confirmations=%d rendered=%d"), F->GetConfirmationCount(), F->GetAudioPlaybackCount());
        Next(Phase==25 ? 26 : Phase+1,.20f); break;
    case 26:
        Aim();
        if (!Check(W->StartFiring(), TEXT("Held rifle fire starts audio regression"))) { return false; }
        FeedbackAutomaticUntil = World->GetTimeSeconds() + .45;
        Next(27,0.f); break;
    case 27:
        Aim();
        if (World->GetTimeSeconds()<FeedbackAutomaticUntil) { return true; }
        W->StopFiring(); Next(28,.20f); break;
    case 28:
        if (!Check(F->GetConfirmationCount()>=8 && F->GetAudioPlaybackCount()==F->GetConfirmationCount(),
            TEXT("Held automatic fire renders a confirmation for every damaging shot"))) { return false; }
        UE_LOG(LogTemp, Display, TEXT("AUDIO automatic confirmations=%d rendered=%d"), F->GetConfirmationCount(), F->GetAudioPlaybackCount());
        Next(5,.15f); break;
    case 5:
        Target->GetAbilitySystemComponent()->SetNumericAttributeBase(UShooterAttributeSet::GetHealthAttribute(), 25.f);
        if (!Check(Fire() && F->IsKillMarker() && F->GetMultiKillCount()==1, TEXT("Lethal shot shows gold confirmation and user kill icon"))) { return false; }
        Next(11,.10f); break;
    case 11:
        Capture(TEXT("03-SingleKill.png")); Next(6,.25f); break;
    case 6:
    {
        auto* Definition = LoadObject<UShooterWeaponDefinition>(nullptr,TEXT("/Game/Weapons/Data/DA_Shotgun.DA_Shotgun"));
        if (!Check(Definition && W->TryGrantWeapon(Definition) && W->EquipWeapon(1), TEXT("Shotgun available for feedback check"))) { return false; }
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Fresh = World->SpawnActor<AShooterCharacterBase>(Target->GetClass(), FVector(-2770,1470,820), FRotator(0,180,0), P);
        Fresh->GetCharacterMovement()->DisableMovement();
        if (auto* AI = Cast<AShooterAIController>(Fresh->GetController())) { AI->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(AI); }
        Fresh->GetAbilitySystemComponent()->SetNumericAttributeBase(UShooterAttributeSet::GetHealthAttribute(), 1.f);
        FeedbackTarget = Fresh; Target = Fresh; Aim(); Player->GetShooterAim()->StartAiming(); Next(7,.3f); break;
    }
    case 7:
        FeedbackBeforeShotgun = F->GetConfirmationCount();
        if (!Check(Fire() && F->GetConfirmationCount()==FeedbackBeforeShotgun+1 && F->GetMultiKillCount()==2, TEXT("Shotgun pellets merge one confirmation and second kill increments x2"))) { return false; }
        Next(12,.10f); break;
    case 12:
        if (!Check(F->GetAudioPlaybackCount()==F->GetConfirmationCount(), TEXT("Hit-to-kill sound replacement and shotgun render exactly once"))) { return false; }
        Capture(TEXT("04-DoubleKill.png")); Next(8,.3f); break;
    case 8:
        GM->TogglePause();
        if (!Check(F->GetIconAlpha()==0 && F->GetMarkerAlpha()==0 && F->GetMultiKillCount()==0 && !F->IsConfirmationAudioPlaying(), TEXT("Pause clears marker, icon, streak and audio"))) { return false; }
        Capture(TEXT("05-PauseCleared.png")); Next(9,.3f); break;
    case 9:
        GM->TogglePause(); Finish(true,TEXT("Actual rifle/ADS hit, blood particles, kill icon, shotgun x2 and pause cleanup verified")); return false;
    }
    return true;
}
