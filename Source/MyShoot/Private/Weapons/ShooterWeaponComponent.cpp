#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Game/ShooterGameMode.h"
#include "Components/SceneComponent.h"
#include "AbilitySystemComponent.h"
#include "Characters/ShooterCharacterBase.h"
#include "Combat/ShooterDamageLibrary.h"
#include "GAS/Abilities/ShooterFireAbility.h"
#include "GAS/Abilities/ShooterReloadAbility.h"
#include "GAS/ShooterGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UShooterWeaponComponent::UShooterWeaponComponent()
{
    // 连射由能力定时器驱动，组件不需要逐帧 Tick。
    PrimaryComponentTick.bCanEverTick = false;
}

void UShooterWeaponComponent::BeginPlay()
{
    Super::BeginPlay();

    // 防御非法类默认值；每个新角色从满弹匣与配置的备用弹药开始。
    Damage = FMath::IsFinite(Damage) ? FMath::Max(Damage, 0.01f) : 25.0f;
    FireInterval = FMath::IsFinite(FireInterval) ? FMath::Max(FireInterval, 0.01f) : 0.1f;
    Range = FMath::IsFinite(Range) ? FMath::Max(Range, 1.0f) : 10000.0f;
    MagazineCapacity = FMath::Max(MagazineCapacity, 1);
    CurrentAmmo = MagazineCapacity;
    InitialReserveAmmo = FMath::Max(InitialReserveAmmo, 0);
    ReserveAmmo = InitialReserveAmmo;
    ReloadDuration = FMath::IsFinite(ReloadDuration) ? FMath::Max(ReloadDuration, 0.01f) : 1.5f;

    AShooterCharacterBase* Character = Cast<AShooterCharacterBase>(GetOwner());
    if (!Character || !Character->HasAuthority() || !Character->IsGASInitialized())
    {
        return;
    }

    // 公共角色在组件 BeginPlay 前初始化 ASC；SourceObject 指明本能力使用哪把武器。
    UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
    AbilitySystem = ASC;
    FireAbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(UShooterFireAbility::StaticClass(), 1, INDEX_NONE, this));
    ReloadAbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(UShooterReloadAbility::StaticClass(), 1, INDEX_NONE, this));
    ReloadTagChangedHandle = ASC->RegisterGameplayTagEvent(
        ShooterGameplayTags::State_Reloading, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UShooterWeaponComponent::HandleReloadTagChanged);
}

void UShooterWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 组件移除或退出关卡时，撤销自己授予的能力与委托，避免旧对象继续收到回调。
    bShuttingDown = true;
    if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
    {
        ASC->RegisterGameplayTagEvent(ShooterGameplayTags::State_Reloading,
            EGameplayTagEventType::NewOrRemoved).Remove(ReloadTagChangedHandle);
        StopFiring();
        CancelReloading();
        if (GetOwner()->HasAuthority() && FireAbilityHandle.IsValid())
        {
            ASC->ClearAbility(FireAbilityHandle);
        }
        if (GetOwner()->HasAuthority() && ReloadAbilityHandle.IsValid())
        {
            ASC->ClearAbility(ReloadAbilityHandle);
        }
    }
    FireAbilityHandle = FGameplayAbilitySpecHandle();
    ReloadAbilityHandle = FGameplayAbilitySpecHandle();
    AbilitySystem.Reset();
    Super::EndPlay(EndPlayReason);
}

bool UShooterWeaponComponent::StartFiring()
{
    if (!CanFire() || !FireAbilityHandle.IsValid())
    {
        return false;
    }
    return IsFiring() || AbilitySystem->TryActivateAbility(FireAbilityHandle);
}

void UShooterWeaponComponent::StopFiring()
{
    if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
    {
        ASC->CancelAbilityHandle(FireAbilityHandle);
    }
}

bool UShooterWeaponComponent::IsFiring() const
{
    const UAbilitySystemComponent* ASC = AbilitySystem.Get();
    const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(FireAbilityHandle) : nullptr;
    return Spec && Spec->IsActive();
}

bool UShooterWeaponComponent::CanUseWeapon() const
{
    const AShooterCharacterBase* Character = Cast<AShooterCharacterBase>(GetOwner());
    const UAbilitySystemComponent* ASC = AbilitySystem.Get();
    return AShooterGameMode::IsCombatAllowed(this) && !bShuttingDown && !bReloadEnding && !IsBeingDestroyed()
        && IsValid(Character) && !Character->IsActorBeingDestroyed()
        && Character->HasAuthority() && Character->IsGASInitialized()
        && !Character->HasGASDeathStarted() && Character->GetGASHealth() > 0.0f
        && ASC && !ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Dead);
}

bool UShooterWeaponComponent::CanFire() const
{
    return CanUseWeapon() && CurrentAmmo > 0 && !IsReloading();
}

float UShooterWeaponComponent::GetTimeUntilNextShot() const
{
    return GetWorld() ? static_cast<float>(FMath::Max(0.0, NextAllowedShotTime - GetWorld()->GetTimeSeconds())) : 0.0f;
}

bool UShooterWeaponComponent::TryFireOneShot()
{
    if (!CanFire() || GetTimeUntilNextShot() > 0.0f)
    {
        return false;
    }

    // 先扣弹并记录下一次允许时间，外部回调即使再次请求开火也无法重复扣弹。
    const int32 OldAmmo = CurrentAmmo;
    --CurrentAmmo;
    NextAllowedShotTime = GetWorld()->GetTimeSeconds() + static_cast<double>(FireInterval);

    FVector Start;
    FRotator Rotation;
    AShooterCharacterBase* Character = CastChecked<AShooterCharacterBase>(GetOwner());
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
    {
        PC->GetPlayerViewPoint(Start, Rotation);
    }
    else
    {
        // 无玩家控制器时仍可使用角色眼睛朝向，便于自动测试和后续 AI 复用。
        Character->GetActorEyesViewPoint(Start, Rotation);
    }

    const FVector End = Start + Rotation.Vector() * Range;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterFire), false, Character);
    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, QueryParams);
    Hit.TraceStart = Start;
    Hit.TraceEnd = End;
    if (bHit && Hit.GetActor())
    {
        UShooterDamageLibrary::ApplyGASDamage(Character, Hit.GetActor(), Damage, Character, Hit);
    }

    // 伤害可能引发任意蓝图事件；销毁后不继续广播。表现事件没有扣血、扣弹职责。
    if (IsValid(this) && !IsBeingDestroyed() && !Character->IsActorBeingDestroyed())
    {
        // 本地玩家采用 2D 枪声，避免第一人称枪口距离导致忽大忽小；不用 UI 音频，暂停时一起暂停。
        // 枪声不再交给蓝图或动画通知，确保单发只播放一次。
        if (FireSound && GetWorld()->GetNetMode() != NM_DedicatedServer)
        {
            UGameplayStatics::PlaySound2D(this, FireSound, FireSoundVolume, 1.0f, 0.0f, nullptr, Character, false);
        }
        // 枪口到射线端点的可见模型；只额外裁剪表现路径，绝不再次结算伤害。
        if (BulletVisualClass && GetWorld()->GetNetMode() != NM_DedicatedServer)
        {
            FVector Muzzle = Start;
            TInlineComponentArray<USceneComponent*> Components(Character);
            for (USceneComponent* Component : Components)
            {
                if (Component->GetFName() == MuzzleComponentName) { Muzzle = Component->GetComponentLocation(); break; }
            }
            FVector VisualEnd = bHit ? Hit.ImpactPoint : End;
            FHitResult Obstruction;
            if (GetWorld()->LineTraceSingleByChannel(Obstruction, Muzzle, VisualEnd, TraceChannel, QueryParams))
            {
                VisualEnd = Obstruction.ImpactPoint;
            }
            FActorSpawnParameters Spawn; Spawn.Owner = Character;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if (AShooterBulletVisual* Bullet = GetWorld()->SpawnActor<AShooterBulletVisual>(BulletVisualClass,
                Muzzle, (VisualEnd - Muzzle).Rotation(), Spawn)) { Bullet->Launch(VisualEnd, BulletVisualSpeed); }
        }
        OnAmmoChanged.Broadcast(OldAmmo, CurrentAmmo);
        if (IsValid(this) && !IsBeingDestroyed() && !Character->IsActorBeingDestroyed())
        {
            OnShotFired.Broadcast(bHit, Hit);
        }
    }
    return true;
}

void UShooterWeaponComponent::HandleReloadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    // 换弹状态出现时立即取消连射，不等待下一发定时器；移除标签不会自动恢复射击。
    if (NewCount > 0)
    {
        StopFiring();
    }
}

bool UShooterWeaponComponent::StartReloading()
{
    return CanReload() && ReloadAbilityHandle.IsValid()
        && AbilitySystem->TryActivateAbility(ReloadAbilityHandle);
}

void UShooterWeaponComponent::CancelReloading()
{
    if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
    {
        ASC->CancelAbilityHandle(ReloadAbilityHandle);
    }
}

bool UShooterWeaponComponent::IsReloading() const
{
    const UAbilitySystemComponent* ASC = AbilitySystem.Get();
    return ASC && ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Reloading);
}

bool UShooterWeaponComponent::CanReload() const
{
    return CanCompleteReload() && !IsReloading();
}

bool UShooterWeaponComponent::CanCompleteReload() const
{
    // 正常完成时能力仍持有换弹标签，此处只检查角色、弹匣和备用弹药。
    return CanUseWeapon() && CurrentAmmo < MagazineCapacity && ReserveAmmo > 0;
}

bool UShooterWeaponComponent::CommitReloadAmmo(int32& OutOldAmmo, int32& OutOldReserve)
{
    if (!CanCompleteReload())
    {
        return false;
    }
    const int32 Transfer = FMath::Min(MagazineCapacity - CurrentAmmo, ReserveAmmo);
    OutOldAmmo = CurrentAmmo;
    OutOldReserve = ReserveAmmo;
    CurrentAmmo += Transfer;
    ReserveAmmo -= Transfer;
    return Transfer > 0;
}

void UShooterWeaponComponent::PublishReloadEnd(bool bSucceeded, bool bNotify, int32 OldAmmo, int32 OldReserve)
{
    // GAS 已清除标签；先发布同一次转移的两个数值，再开放下一次输入。
    const auto CanNotify = [this]()
    {
        return IsValid(this) && !IsBeingDestroyed() && !bShuttingDown
            && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed();
    };
    if (bSucceeded && CanNotify())
    {
        OnAmmoChanged.Broadcast(OldAmmo, CurrentAmmo);
        if (CanNotify())
        {
            OnReserveAmmoChanged.Broadcast(OldReserve, ReserveAmmo);
        }
    }
    bReloadEnding = false;
    if (bNotify && CanNotify())
    {
        OnReloadFinished.Broadcast(bSucceeded);
    }
}
