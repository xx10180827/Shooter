#include "Weapons/ShooterWeaponComponent.h"
#include "AbilitySystemComponent.h"
#include "Characters/ShooterCharacterBase.h"
#include "Combat/ShooterDamageLibrary.h"
#include "GAS/Abilities/ShooterFireAbility.h"
#include "GAS/ShooterGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UShooterWeaponComponent::UShooterWeaponComponent()
{
    // 连射由能力定时器驱动，组件不需要逐帧 Tick。
    PrimaryComponentTick.bCanEverTick = false;
}

void UShooterWeaponComponent::BeginPlay()
{
    Super::BeginPlay();

    // 防御非法类默认值；每个新角色从满弹匣开始，T05 再提供换弹入口。
    Damage = FMath::IsFinite(Damage) ? FMath::Max(Damage, 0.01f) : 25.0f;
    FireInterval = FMath::IsFinite(FireInterval) ? FMath::Max(FireInterval, 0.01f) : 0.1f;
    Range = FMath::IsFinite(Range) ? FMath::Max(Range, 1.0f) : 10000.0f;
    MagazineCapacity = FMath::Max(MagazineCapacity, 1);
    CurrentAmmo = MagazineCapacity;

    AShooterCharacterBase* Character = Cast<AShooterCharacterBase>(GetOwner());
    if (!Character || !Character->HasAuthority() || !Character->IsGASInitialized())
    {
        return;
    }

    // 公共角色在组件 BeginPlay 前初始化 ASC；SourceObject 指明本能力使用哪把武器。
    UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
    AbilitySystem = ASC;
    FireAbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(UShooterFireAbility::StaticClass(), 1, INDEX_NONE, this));
    ReloadTagChangedHandle = ASC->RegisterGameplayTagEvent(
        ShooterGameplayTags::State_Reloading, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UShooterWeaponComponent::HandleReloadTagChanged);
}

void UShooterWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 组件移除或退出关卡时，撤销自己授予的能力与委托，避免旧对象继续收到回调。
    if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
    {
        ASC->RegisterGameplayTagEvent(ShooterGameplayTags::State_Reloading,
            EGameplayTagEventType::NewOrRemoved).Remove(ReloadTagChangedHandle);
        StopFiring();
        if (GetOwner()->HasAuthority() && FireAbilityHandle.IsValid())
        {
            ASC->ClearAbility(FireAbilityHandle);
        }
    }
    FireAbilityHandle = FGameplayAbilitySpecHandle();
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

bool UShooterWeaponComponent::CanFire() const
{
    const AShooterCharacterBase* Character = Cast<AShooterCharacterBase>(GetOwner());
    const UAbilitySystemComponent* ASC = AbilitySystem.Get();
    return !IsBeingDestroyed() && IsValid(Character) && !Character->IsActorBeingDestroyed()
        && Character->HasAuthority() && Character->IsGASInitialized()
        && !Character->HasGASDeathStarted() && Character->GetGASHealth() > 0.0f
        && CurrentAmmo > 0 && ASC
        && !ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Dead)
        && !ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Reloading);
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
