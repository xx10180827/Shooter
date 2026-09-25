#include "Combat/ShooterDamageLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "Characters/ShooterCharacterBase.h"
#include "GAS/Effects/ShooterDamageEffect.h"
#include "GAS/ShooterGameplayTags.h"

bool UShooterDamageLibrary::ApplyGASDamage(AActor* SourceActor, AActor* TargetActor,
    float Damage, AActor* DamageCauser, const FHitResult& HitResult)
{
    // 入口校验：排除已销毁对象、非权威端和非法伤害，避免生成无效效果。
    if (!IsValid(SourceActor) || !IsValid(TargetActor)
        || SourceActor->IsActorBeingDestroyed() || TargetActor->IsActorBeingDestroyed()
        || !SourceActor->HasAuthority() || !TargetActor->HasAuthority()
        || !FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return false;
    }

    if (const AShooterCharacterBase* SourceCharacter = Cast<AShooterCharacterBase>(SourceActor))
    {
        if (SourceCharacter->HasGASDeathStarted()) { return false; }
    }

    UAbilitySystemComponent* TargetASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (!IsValid(TargetASC) || TargetASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Dead)
        || !TargetASC->HasAttributeSetForAttribute(UShooterAttributeSet::GetHealthAttribute()))
    {
        return false;
    }

    if (const AShooterCharacterBase* TargetCharacter = Cast<AShooterCharacterBase>(TargetActor))
    {
        if (!TargetCharacter->IsGASInitialized() || TargetCharacter->HasGASDeathStarted())
        {
            return false;
        }
    }

    const float HealthBefore = TargetASC->GetNumericAttribute(UShooterAttributeSet::GetHealthAttribute());
    if (!FMath::IsFinite(HealthBefore) || HealthBefore <= 0.0f)
    {
        return false;
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
        return false;
    }

    Spec.Data->SetSetByCallerMagnitude(UShooterDamageEffect::GetHealthDeltaTag(), -Damage);
    TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

    // 瞬时效果没有持续句柄，用实际血量变化判断伤害是否生效。
    return TargetASC->GetNumericAttribute(UShooterAttributeSet::GetHealthAttribute()) < HealthBefore;
}
