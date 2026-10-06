#include "Combat/ShooterDamageLibrary.h"
#include "Perception/AISense_Damage.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "Characters/ShooterCharacterBase.h"
#include "GAS/Effects/ShooterDamageEffect.h"
#include "GAS/ShooterGameplayTags.h"

FShooterDamageResult UShooterDamageLibrary::ResolveGASDamage(AActor* SourceActor, AActor* TargetActor,
    float Damage, AActor* DamageCauser, const FHitResult& HitResult)
{
    FShooterDamageResult Result; Result.Target = TargetActor; Result.Hit = HitResult;
    // 入口校验：排除已销毁对象、非权威端和非法伤害，避免生成无效效果。
    if (!IsValid(SourceActor) || !IsValid(TargetActor)
        || SourceActor->IsActorBeingDestroyed() || TargetActor->IsActorBeingDestroyed()
        || !SourceActor->HasAuthority() || !TargetActor->HasAuthority()
        || !FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return Result;
    }

    if (const AShooterCharacterBase* SourceCharacter = Cast<AShooterCharacterBase>(SourceActor))
    {
        if (SourceCharacter->HasGASDeathStarted()) { return Result; }
    }

    UAbilitySystemComponent* TargetASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (!IsValid(TargetASC) || TargetASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Dead)
        || TargetASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Invulnerable)
        || !TargetASC->HasAttributeSetForAttribute(UShooterAttributeSet::GetHealthAttribute()))
    {
        return Result;
    }

    if (const AShooterCharacterBase* TargetCharacter = Cast<AShooterCharacterBase>(TargetActor))
    {
        if (!TargetCharacter->IsGASInitialized() || TargetCharacter->HasGASDeathStarted())
        {
            return Result;
        }
    }

    const float HealthBefore = TargetASC->GetNumericAttribute(UShooterAttributeSet::GetHealthAttribute());
    if (!FMath::IsFinite(HealthBefore) || HealthBefore <= 0.0f)
    {
        return Result;
    }

    UAbilitySystemComponent* SourceASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
    // 兼容先迁移敌人、后迁移玩家的顺序：来源没有 ASC 时使用目标 ASC 创建效果。
    UAbilitySystemComponent* SpecASC = IsValid(SourceASC) ? SourceASC : TargetASC;
    AActor* ActualCauser = IsValid(DamageCauser) ? DamageCauser : SourceActor;

    // 保留攻击者、伤害载体和命中信息，供后续击杀归属、受击表现等功能读取。
    FGameplayEffectContextHandle Context = SpecASC->MakeEffectContext();
    Context.AddInstigator(SourceActor, ActualCauser);
    Context.AddSourceObject(ActualCauser);
    if (HitResult.bBlockingHit || HitResult.GetActor() != nullptr)
    {
        Context.AddHitResult(HitResult, true);
    }

    FGameplayEffectSpecHandle Spec = SpecASC->MakeOutgoingSpec(
        UShooterDamageEffect::StaticClass(), 1.0f, Context);
    if (!Spec.IsValid())
    {
        return Result;
    }

    Spec.Data->SetSetByCallerMagnitude(UShooterDamageEffect::GetHealthDeltaTag(), -Damage);
    TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

    // 瞬时效果没有持续句柄，用实际血量变化判断伤害是否生效。
    const float HealthAfter = IsValid(TargetASC) ? TargetASC->GetNumericAttribute(UShooterAttributeSet::GetHealthAttribute()) : HealthBefore;
    const bool bDamaged = HealthAfter < HealthBefore;
    // 只有真实生效且目标存活的伤害上报受击感知；不额外扣血，也不依赖音效播放。
    if (bDamaged && HealthAfter > 0 && IsValid(TargetActor) && IsValid(SourceActor))
    {
        UAISense_Damage::ReportDamageEvent(TargetActor, TargetActor, SourceActor, HealthBefore-HealthAfter,
            SourceActor->GetActorLocation(), HitResult.bBlockingHit ? FVector(HitResult.ImpactPoint) : TargetActor->GetActorLocation(), TEXT("ShooterDamage"));
    }
    Result.ActualDamage = FMath::Max(0.f, HealthBefore - HealthAfter);
    Result.bKilled = bDamaged && HealthAfter <= 0.f;
    return Result;
}

bool UShooterDamageLibrary::ApplyGASDamage(AActor* SourceActor, AActor* TargetActor,
    float Damage, AActor* DamageCauser, const FHitResult& HitResult)
{
    return ResolveGASDamage(SourceActor, TargetActor, Damage, DamageCauser, HitResult).WasDamaged();
}