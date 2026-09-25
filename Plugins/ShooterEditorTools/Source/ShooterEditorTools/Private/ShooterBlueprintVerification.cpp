#include "ShooterBlueprintWiring.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/InputDelegateBinding.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ScopeExit.h"
#include "Misc/App.h"
#include "UObject/UObjectIterator.h"

// 从保存后的蓝图生成真实实例，执行编译后的输入委托，验证绑定与表现确实进入了运行路径。
bool ShooterBlueprintWiring::VerifyT04(UBlueprint* Blueprint, FString& Result)
{
    if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(AMyShooter::StaticClass()))
    {
        Result = TEXT("Shooter must derive from MyShooter.");
        return false;
    }
    // 粒子生成在无渲染模式会被引擎主动跳过，验证必须显式允许命令行渲染。
    if (!FApp::CanEverRender()) { Result = TEXT("VerifyT04 requires -AllowCommandletRendering without -NullRHI."); return false; }
    UParticleSystem* Particle = LoadObject<UParticleSystem>(nullptr,
        TEXT("/Game/Assets/Effects/ParticleSystems/Weapons/AssaultRifle/Muzzle/P_AssaultRifle_MF_GAS.P_AssaultRifle_MF_GAS"));
    if (!Particle) { Result = TEXT("Missing single-shot muzzle asset."); return false; }
    for (UParticleEmitter* Emitter : Particle->Emitters)
    {
        for (UParticleLODLevel* LOD : Emitter->LODLevels)
        {
            if (LOD && LOD->RequiredModule && LOD->RequiredModule->EmitterLoops != 1)
            {
                Result = TEXT("Single-shot muzzle contains a looping emitter.");
                return false;
            }
        }
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) { Result = TEXT("Cannot create verification world."); return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AMyShooter* Player = World->SpawnActor<AMyShooter>(Blueprint->GeneratedClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Player || !Player->IsGASInitialized() || !Player->GetShooterWeapon())
    {
        Result = TEXT("Spawned Shooter did not initialize GAS/weapon.");
        return false;
    }
    Player->GetCharacterMovement()->DisableMovement();
    UShooterWeaponComponent* Weapon = Player->GetShooterWeapon();
    if (!Weapon->OnShotFired.IsBound())
    {
        Result = TEXT("Blueprint BeginPlay did not bind OnShotFired.");
        return false;
    }

    UInputComponent* Input = NewObject<UInputComponent>(Player);
    UInputDelegateBinding::BindInputDelegates(Player->GetClass(), Input, Player);
    FInputActionBinding* Pressed = nullptr;
    FInputActionBinding* Released = nullptr;
    for (int32 Index = 0; Index < Input->GetNumActionBindings(); ++Index)
    {
        FInputActionBinding& Binding = Input->GetActionBinding(Index);
        if (Binding.GetActionName() == TEXT("fire"))
        {
            if (Binding.KeyEvent == IE_Pressed) { Pressed = &Binding; }
            if (Binding.KeyEvent == IE_Released) { Released = &Binding; }
        }
    }
    if (!Pressed || !Released) { Result = TEXT("Compiled fire input bindings are missing."); return false; }

    const int32 InitialAmmo = Weapon->GetCurrentAmmo();
    Pressed->ActionDelegate.Execute(EKeys::LeftMouseButton);
    if (Weapon->GetCurrentAmmo() != InitialAmmo - 1)
    {
        Result = TEXT("Actual Blueprint pressed input failed to consume exactly one bullet.");
        return false;
    }
    int32 MuzzleCount = 0;
    for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
    {
        UParticleSystemComponent* Component = *It;
        if (Component->Template == Particle && Component->GetAttachParent()
            && Component->GetAttachParent()->GetOwner() == Player)
        {
            if (!Component->bAutoDestroy) { Result = TEXT("Muzzle must auto-destroy."); return false; }
            ++MuzzleCount;
        }
    }
    UAnimMontage* ExpectedMontage = LoadObject<UAnimMontage>(nullptr,
        TEXT("/Game/Blueprints/Shooter_fire_Montage.Shooter_fire_Montage"));
    if (MuzzleCount != 1 || Player->GetCurrentMontage() != ExpectedMontage)
    {
        Result = FString::Printf(TEXT("Presentation did not execute exactly once: muzzle=%d montage=%s expected=%s"),
            MuzzleCount, *GetNameSafe(Player->GetCurrentMontage()), *GetNameSafe(ExpectedMontage));
        return false;
    }
    if (UKismetSystemLibrary::K2_IsTimerActive(Player, TEXT("Shoot_Once")))
    {
        Result = TEXT("Legacy Shoot_Once timer is still active.");
        return false;
    }

    // 实际 Released 事件只取消 GAS，不会再无条件销毁旧 Fire_ Effect 空引用。
    Released->ActionDelegate.Execute(EKeys::LeftMouseButton);
    if (Weapon->IsFiring()) { Result = TEXT("Released input did not stop firing."); return false; }
    Pressed->ActionDelegate.Execute(EKeys::LeftMouseButton);
    AActor* Source = World->SpawnActor<AActor>();
    if (!Source || !UShooterDamageLibrary::ApplyGASDamage(Source, Player, Player->GetGASHealth() + 1.0f, Source, FHitResult())
        || !Player->HasGASDeathStarted() || Weapon->IsFiring())
    {
        Result = TEXT("Death did not cancel the Blueprint-triggered fire ability.");
        return false;
    }
    Result = FString::Printf(TEXT("PASS: saved Shooter binds OnShotFired; real pressed input spends one bullet; one muzzle and montage play; released stops; legacy timer absent; death cancels. InitialAmmo=%d"), InitialAmmo);
    return true;
}
