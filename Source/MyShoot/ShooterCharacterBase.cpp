#include "ShooterCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "ShooterAttributeSet.h"
#include "ShooterInitialAttributesEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterGAS, Log, All);

AShooterCharacterBase::AShooterCharacterBase()
{
    // Preserve existing Blueprint Event Tick behavior during reparenting.
    PrimaryActorTick.bCanEverTick = true;

    ShooterAbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(
        TEXT("ShooterAbilitySystemComponent"));
    ShooterAttributes = CreateDefaultSubobject<UShooterAttributeSet>(TEXT("ShooterAttributes"));
}

UAbilitySystemComponent* AShooterCharacterBase::GetAbilitySystemComponent() const
{
    return ShooterAbilitySystemComponent;
}

float AShooterCharacterBase::GetGASHealth() const
{
    return ShooterAttributes->GetHealth();
}

float AShooterCharacterBase::GetGASMaxHealth() const
{
    return ShooterAttributes->GetMaxHealth();
}

void AShooterCharacterBase::BeginPlay()
{
    // AActor::BeginPlay dispatches Blueprint BeginPlay: initialize before that event.
    InitializeGAS();
    Super::BeginPlay();
}

void AShooterCharacterBase::InitializeGAS()
{
    if (bGASInitialized)
    {
        return;
    }

    // T01 supports standalone play. Networking needs an explicit replication design.
    ShooterAbilitySystemComponent->InitAbilityActorInfo(this, this);
    if (!HasAuthority())
    {
        return;
    }

    FGameplayEffectContextHandle Context = ShooterAbilitySystemComponent->MakeEffectContext();
    Context.AddSourceObject(this);
    FGameplayEffectSpecHandle Spec = ShooterAbilitySystemComponent->MakeOutgoingSpec(
        UShooterInitialAttributesEffect::StaticClass(), 1.0f, Context);
    if (!Spec.IsValid())
    {
        UE_LOG(LogShooterGAS, Error, TEXT("%s: could not create initial attributes effect."), *GetName());
        return;
    }

    const float SafeMaxHealth = FMath::IsFinite(InitialMaxHealth)
        ? FMath::Max(InitialMaxHealth, 1.0f) : 100.0f;
    Spec.Data->SetSetByCallerMagnitude(UShooterInitialAttributesEffect::GetInitialHealthTag(), SafeMaxHealth);
    ShooterAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

    // An Instant effect does not leave a persistent ActiveGameplayEffectHandle.
    bGASInitialized = FMath::IsNearlyEqual(GetGASMaxHealth(), SafeMaxHealth)
        && FMath::IsNearlyEqual(GetGASHealth(), SafeMaxHealth);
    if (!bGASInitialized)
    {
        UE_LOG(LogShooterGAS, Error, TEXT("%s: initial attributes did not reach the configured values."), *GetName());
    }
}

void AShooterCharacterBase::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (bGASInitialized)
    {
        // Refresh the cached controller; possession must never refill health.
        ShooterAbilitySystemComponent->RefreshAbilityActorInfo();
    }
}

void AShooterCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    ShooterAbilitySystemComponent->ClearActorInfo();
    bGASInitialized = false;
}
