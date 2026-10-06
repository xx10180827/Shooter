#include "Weapons/ShooterWeaponComponent.h"
#include "Perception/AISense_Hearing.h"
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

    InitializeLoadout();

    // 防御非法类默认值；每个新角色从满弹匣与配置的备用弹药开始。
    Damage = FMath::IsFinite(Damage) ? FMath::Max(Damage, 0.01f) : 25.0f;
    FireInterval = FMath::IsFinite(FireInterval) ? FMath::Max(FireInterval, 0.01f) : 0.1f;
    Range = FMath::IsFinite(Range) ? FMath::Max(Range, 1.0f) : 10000.0f;
    MagazineCapacity = FMath::Max(MagazineCapacity, 1);
    const int32 PreviousAmmo = CurrentAmmo;
    const int32 PreviousReserve = ReserveAmmo;
    CurrentAmmo = MagazineCapacity;
    InitialReserveAmmo = FMath::Max(InitialReserveAmmo, 0);
    ReserveAmmo = InitialReserveAmmo;
    ReloadDuration = FMath::IsFinite(ReloadDuration) ? FMath::Max(ReloadDuration, 0.01f) : 1.5f;

    // HUD 可能在控制器 BeginPlay 时已订阅组件；初始化也必须广播，避免启动顺序变化时显示 0 发。
    OnAmmoChanged.Broadcast(PreviousAmmo, CurrentAmmo);
    OnReserveAmmoChanged.Broadcast(PreviousReserve, ReserveAmmo);
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
    return AShooterGameMode::IsCombatAllowed(this) && !bShuttingDown && !bReloadEnding && !bSwitchingWeapon && !bResolvingShot && !IsBeingDestroyed()
        && IsValid(Character) && !Character->IsActorBeingDestroyed()
        && Character->HasAuthority() && Character->IsGASInitialized()
        && !Character->HasGASDeathStarted() && Character->GetGASHealth() > 0.0f
        && ASC && !ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Dead) && !ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Dashing);
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

    // 发射和广播期间拒绝重入切枪；一次发射只扣一次弹药、播放一次枪声与手臂动画。
    TGuardValue<bool> ShotGuard(bResolvingShot, true);
    const int32 OldAmmo = CurrentAmmo;
    --CurrentAmmo;
    NextAllowedShotTime = GetWorld()->GetTimeSeconds() + static_cast<double>(FireInterval);
    FVector Start; FRotator Rotation;
    AShooterCharacterBase* Character = CastChecked<AShooterCharacterBase>(GetOwner());
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController())) { PC->GetPlayerViewPoint(Start, Rotation); }
    else { Character->GetActorEyesViewPoint(Start, Rotation); }
    UAISense_Hearing::ReportNoiseEvent(this, Character->GetActorLocation(), 1.f, Character, 0.f, TEXT("ShooterGunshot"));
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterFire), false, Character);
    TArray<FHitResult> Hits;
    TMap<AActor*, int32> HitCounts;
    TMap<AActor*, FHitResult> TargetHits;
    for (int32 Index=0; Index<PelletCount; ++Index)
    {
        const FVector Direction = SpreadHalfAngle > 0.f
            ? FMath::VRandCone(Rotation.Vector(), FMath::DegreesToRadians(SpreadHalfAngle)) : Rotation.Vector();
        const FVector End = Start + Direction * Range;
        FHitResult Hit;
        GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, QueryParams);
        Hit.TraceStart = Start; Hit.TraceEnd = End; Hits.Add(Hit);
        if (Hit.bBlockingHit && Hit.GetActor()) { ++HitCounts.FindOrAdd(Hit.GetActor()); TargetHits.FindOrAdd(Hit.GetActor()) = Hit; }
    }
    // 同一目标的弹丸合并一次 GAS 结算，避免多次死亡/警觉回调；各射线仍独立检查遮挡。
    FShooterShotResult ShotResult; ShotResult.ShotId = ++ResolvedShotId;
    for (const auto& Pair : HitCounts)
    {
        if (IsValid(Pair.Key))
        {
            const auto Result = UShooterDamageLibrary::ResolveGASDamage(Character, Pair.Key, Damage * Pair.Value, Character, TargetHits[Pair.Key]);
            if (Result.WasDamaged()) { ShotResult.Targets.Add(Result); }
        }
    }
    if (IsValid(this) && !IsBeingDestroyed() && !Character->IsActorBeingDestroyed())
    {
        if (FireSound && GetWorld()->GetNetMode() != NM_DedicatedServer)
        { UGameplayStatics::PlaySound2D(this, FireSound, FireSoundVolume, 1.f, 0.f, nullptr, Character, false); }
        for (const auto& Hit : Hits) { SpawnBulletVisual(Hit, Start); }
        OnShotResolved.Broadcast(ShotResult);
        OnAmmoChanged.Broadcast(OldAmmo, CurrentAmmo);
        if (IsValid(this) && !IsBeingDestroyed() && !Character->IsActorBeingDestroyed())
        {
            // 保持旧蓝图单发表现事件；取第一个命中作命中特效，所有弹丸都有可见轨迹。
            const FHitResult* PresentationHit = &Hits[0];
            for (const auto& Hit : Hits) { if (Hit.bBlockingHit) { PresentationHit=&Hit; break; } }
            OnShotFired.Broadcast(PresentationHit->bBlockingHit, *PresentationHit);
        }
    }
    return true;
}

void UShooterWeaponComponent::SpawnBulletVisual(const FHitResult& Hit, const FVector& Start)
{
    if (!BulletVisualClass || GetWorld()->GetNetMode() == NM_DedicatedServer) { return; }
    FVector Muzzle = Start;
    TInlineComponentArray<USceneComponent*> Components(GetOwner());
    for (USceneComponent* Component : Components)
    { if (Component->GetFName() == MuzzleComponentName) { Muzzle = Component->GetComponentLocation(); break; } }
    FVector VisualEnd = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : FVector(Hit.TraceEnd);
    FHitResult Obstruction;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterVisual), false, GetOwner());
    if (GetWorld()->LineTraceSingleByChannel(Obstruction, Muzzle, VisualEnd, TraceChannel, QueryParams)) { VisualEnd = Obstruction.ImpactPoint; }
    FActorSpawnParameters Spawn; Spawn.Owner = GetOwner(); Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (AShooterBulletVisual* Bullet = GetWorld()->SpawnActor<AShooterBulletVisual>(BulletVisualClass, Muzzle, (VisualEnd-Muzzle).Rotation(), Spawn))
    { Bullet->Launch(VisualEnd, BulletVisualSpeed); }
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
