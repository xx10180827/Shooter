#include "Movement/ShooterDashComponent.h"
#include "Characters/MyShooter.h"
#include "AbilitySystemComponent.h"
#include "GAS/Abilities/ShooterDashAbility.h"
#include "GAS/Effects/ShooterDashEffects.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "GAS/ShooterGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
UShooterDashComponent::UShooterDashComponent() { PrimaryComponentTick.bCanEverTick=false; }
void UShooterDashComponent::BeginPlay()
{
    Super::BeginPlay(); Character=Cast<AMyShooter>(GetOwner());
    if(!Character.IsValid()||!Character->HasAuthority()||!Character->IsGASInitialized()) { return; }
    AbilitySystem=Character->GetAbilitySystemComponent();
    DashHandle=AbilitySystem->GiveAbility(FGameplayAbilitySpec(UShooterDashAbility::StaticClass(),1,INDEX_NONE,this));
    GameMode=Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
    if(GameMode.IsValid()) { GameMode->OnRoundChanged.AddUniqueDynamic(this,&UShooterDashComponent::HandleRoundChanged); }
    GetWorld()->GetTimerManager().SetTimer(RegenTimer,this,&UShooterDashComponent::RegenerateStamina,.2f,true);
}
bool UShooterDashComponent::CanStartDash() const
{
    const auto* C=Character.Get(); const auto* ASC=AbilitySystem.Get();
    return IsValid(C)&&!C->IsActorBeingDestroyed()&&!C->HasGASDeathStarted()&&C->GetGASHealth()>0
        &&C->GetController()&&!C->GetController()->IsMoveInputIgnored()&&AShooterGameMode::IsCombatAllowed(this)
        &&C->GetCharacterMovement()->IsMovingOnGround()&&ASC
        &&!ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Dead)
        &&!ASC->HasMatchingGameplayTag(ShooterGameplayTags::State_Reloading);
}
bool UShooterDashComponent::TryDash()
{
    return CanStartDash()&&DashHandle.IsValid()&&!IsDashing()&&AbilitySystem->TryActivateAbility(DashHandle);
}
void UShooterDashComponent::CancelDash() { if(AbilitySystem.IsValid()) { AbilitySystem->CancelAbilityHandle(DashHandle); } }
bool UShooterDashComponent::IsDashing() const { return AbilitySystem.IsValid()&&AbilitySystem->HasMatchingGameplayTag(ShooterGameplayTags::State_Dashing); }
float UShooterDashComponent::GetStamina() const { return AbilitySystem.IsValid()?AbilitySystem->GetNumericAttribute(UShooterAttributeSet::GetStaminaAttribute()):0.f; }
float UShooterDashComponent::GetMaxStamina() const { return AbilitySystem.IsValid()?AbilitySystem->GetNumericAttribute(UShooterAttributeSet::GetMaxStaminaAttribute()):100.f; }
float UShooterDashComponent::GetCooldownRemaining() const
{
    if(!AbilitySystem.IsValid()) { return 0.f; }
    FGameplayTagContainer Tags; Tags.AddTag(ShooterGameplayTags::Cooldown_Dash);
    const auto Times=AbilitySystem->GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
    float Result=0; for(float Time:Times) { Result=FMath::Max(Result,Time); } return Result;
}
FVector UShooterDashComponent::GetDashDirection() const
{
    const auto* C=Character.Get(); if(!C) { return FVector::ForwardVector; }
    // 同帧新输入优先，其次上帧输入；视角只作为静止时的水平方向，不受抬头/低头影响。
    FVector Direction=C->GetPendingMovementInputVector();
    if(Direction.IsNearlyZero()) { Direction=C->GetLastMovementInputVector(); }
    Direction.Z=0;
    if(Direction.IsNearlyZero()) { Direction=FRotator(0,C->GetControlRotation().Yaw,0).Vector(); }
    return Direction.GetSafeNormal();
}
void UShooterDashComponent::RegenerateStamina()
{
    if(!Character.IsValid()||Character->HasGASDeathStarted()||!AbilitySystem.IsValid()
        ||!AShooterGameMode::IsCombatAllowed(this)||IsDashing()||GetCooldownRemaining()>0.f||GetStamina()>=GetMaxStamina()) { return; }
    const auto Spec=AbilitySystem->MakeOutgoingSpec(UShooterStaminaRegenEffect::StaticClass(),1.f,AbilitySystem->MakeEffectContext());
    if(Spec.IsValid()) { AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()); }
}
void UShooterDashComponent::HandleRoundChanged(EShooterRoundState State) { if(State!=EShooterRoundState::Playing) { CancelDash(); } }
void UShooterDashComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(RegenTimer); CancelDash();
    if(GameMode.IsValid()) { GameMode->OnRoundChanged.RemoveDynamic(this,&UShooterDashComponent::HandleRoundChanged); }
    if(AbilitySystem.IsValid()&&GetOwner()->HasAuthority()&&DashHandle.IsValid()) { AbilitySystem->ClearAbility(DashHandle); }
    AbilitySystem.Reset(); Character.Reset(); Super::EndPlay(Reason);
}