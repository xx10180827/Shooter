#include "ShooterReloadWiring.h"
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
#include "UObject/UnrealType.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Misc/ScopeExit.h"
#include "Misc/App.h"

// 加载保存后的 Shooter，调用真实输入委托，同时检查规则和动画蓝图的执行结果。
bool ShooterReloadWiring::Verify(UBlueprint* Blueprint, FString& Result)
{
    if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(AMyShooter::StaticClass())
        || !FApp::CanEverRender())
    {
        Result = TEXT("VerifyT05 requires Shooter and -AllowCommandletRendering without -NullRHI.");
        return false;
    }
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr,
        TEXT("/Game/Blueprints/Shooter_reload_GAS_Montage.Shooter_reload_GAS_Montage"));
    if (!Montage || Montage->GetPlayLength() <= 0.0f) { Result = TEXT("Saved reload montage missing."); return false; }
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) { Result = TEXT("Cannot create reload verification world."); return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 SavedFrameCounter = GFrameCounter;
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        GFrameCounter = SavedFrameCounter;
    };
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance = [World](float Seconds)
    {
        while (Seconds > KINDA_SMALL_NUMBER)
        {
            const float Step = FMath::Min(Seconds, 0.02f);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, Step);
            Seconds -= Step;
        }
    };
    // 先推进首帧，随后输入创建的定时器使用当前时钟，不进入首帧 Pending 集合。
    Advance(0.01f);
    AMyShooter* Player = World->SpawnActorDeferred<AMyShooter>(Blueprint->GeneratedClass, FTransform::Identity,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Player) { Result = TEXT("Cannot spawn Shooter."); return false; }
    UShooterWeaponComponent* Weapon = Player->GetShooterWeapon();
    FFloatProperty* Duration = FindFProperty<FFloatProperty>(UShooterWeaponComponent::StaticClass(), TEXT("ReloadDuration"));
    if (!Weapon || !Duration) { Result = TEXT("Reload component/configuration missing."); return false; }
    // 非默认时间验证动画速率确实跟随参数，防止蓝图写死 1.5 秒。
    Duration->SetPropertyValue_InContainer(Weapon, 0.8f);
    Player->FinishSpawning(FTransform::Identity);
    Player->GetCharacterMovement()->DisableMovement();
    UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance();
    if (!Anim || !Weapon->OnReloadStarted.IsBound() || !Weapon->OnReloadFinished.IsBound())
    {
        Result = TEXT("Reload animation instance or BeginPlay bindings missing.");
        return false;
    }
    UInputComponent* Input = NewObject<UInputComponent>(Player);
    UInputDelegateBinding::BindInputDelegates(Player->GetClass(), Input, Player);
    FInputActionBinding* Pressed = nullptr;
    FInputActionBinding* Released = nullptr;
    FInputActionBinding* Reload = nullptr;
    for (int32 Index = 0; Index < Input->GetNumActionBindings(); ++Index)
    {
        FInputActionBinding& Binding = Input->GetActionBinding(Index);
        if (Binding.GetActionName() == TEXT("fire") && Binding.KeyEvent == IE_Pressed) { Pressed = &Binding; }
        if (Binding.GetActionName() == TEXT("fire") && Binding.KeyEvent == IE_Released) { Released = &Binding; }
        if (Binding.GetActionName() == TEXT("reload") && Binding.KeyEvent == IE_Pressed) { Reload = &Binding; }
    }
    if (!Pressed || !Released || !Reload) { Result = TEXT("Compiled fire/reload action bindings missing."); return false; }
    Reload->ActionDelegate.Execute(EKeys::R);
    if (Weapon->IsReloading() || Anim->Montage_IsPlaying(Montage))
    {
        Result = TEXT("Full magazine incorrectly starts reload or animation.");
        return false;
    }
    const int32 InitialAmmo = Weapon->GetCurrentAmmo();
    const int32 InitialReserve = Weapon->GetReserveAmmo();
    Pressed->ActionDelegate.Execute(EKeys::LeftMouseButton);
    Released->ActionDelegate.Execute(EKeys::LeftMouseButton);
    Reload->ActionDelegate.Execute(EKeys::R);
    if (!Weapon->IsReloading() || !Anim->Montage_IsPlaying(Montage)
        || !FMath::IsNearlyEqual(Anim->Montage_GetPlayRate(Montage), Montage->GetPlayLength() / 0.8f, 0.001f))
    {
        Result = TEXT("R did not start GAS reload and correctly timed montage.");
        return false;
    }
    Advance(0.3f);
    const float Position = Anim->Montage_GetPosition(Montage);
    Reload->ActionDelegate.Execute(EKeys::R);
    Pressed->ActionDelegate.Execute(EKeys::LeftMouseButton);
    Released->ActionDelegate.Execute(EKeys::LeftMouseButton);
    if (Weapon->GetCurrentAmmo() != InitialAmmo - 1 || Weapon->GetReserveAmmo() != InitialReserve
        || !FMath::IsNearlyEqual(Position, Anim->Montage_GetPosition(Montage), 0.001f) || Weapon->IsFiring())
    {
        Result = TEXT("Repeated R/fire changed pending reload, animation position or ammo.");
        return false;
    }
    Advance(0.52f);
    if (Weapon->IsReloading() || Weapon->GetCurrentAmmo() != InitialAmmo
        || Weapon->GetReserveAmmo() != InitialReserve - 1)
    {
        Result = FString::Printf(TEXT("Reload transfer mismatch: ammo=%d expected=%d reserve=%d expected=%d reloading=%d time=%.3f"),
            Weapon->GetCurrentAmmo(), InitialAmmo, Weapon->GetReserveAmmo(), InitialReserve - 1,
            Weapon->IsReloading(), World->GetTimeSeconds());
        return false;
    }
    Advance(0.12f);
    if (Anim->Montage_IsPlaying(Montage)) { Result = TEXT("Reload animation did not stop at completion."); return false; }

    Pressed->ActionDelegate.Execute(EKeys::LeftMouseButton);
    Released->ActionDelegate.Execute(EKeys::LeftMouseButton);
    Reload->ActionDelegate.Execute(EKeys::R);
    Advance(0.2f);
    Weapon->CancelReloading();
    Advance(0.9f);
    if (Weapon->IsReloading() || Anim->Montage_IsPlaying(Montage) || Weapon->GetCurrentAmmo() != InitialAmmo - 1
        || Weapon->GetReserveAmmo() != InitialReserve - 1)
    {
        Result = TEXT("Cancel did not stop animation or leaked a delayed ammo transfer.");
        return false;
    }
    Reload->ActionDelegate.Execute(EKeys::R);
    AActor* Source = World->SpawnActor<AActor>();
    if (!Source || !UShooterDamageLibrary::ApplyGASDamage(Source, Player, Player->GetGASHealth() + 1, Source, FHitResult()))
    {
        Result = TEXT("Cannot apply death during reload.");
        return false;
    }
    Advance(0.9f);
    if (Weapon->IsReloading() || Anim->Montage_IsPlaying(Montage) || Weapon->GetCurrentAmmo() != InitialAmmo - 1
        || Weapon->GetReserveAmmo() != InitialReserve - 1 || Weapon->StartReloading())
    {
        Result = TEXT("Death failed to cancel real reload and its animation.");
        return false;
    }
    Result = TEXT("PASS T05: saved Shooter R input, full-mag rejection, reload animation/rate, repeated R, fire exclusion, delayed transfer, completion, cancellation and death cleanup.");
    return true;
}
