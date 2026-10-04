#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Game/ShooterGameMode.h"
#include "ShooterDashComponent.generated.h"
class UAbilitySystemComponent;
class AMyShooter;
/** 玩家闪避入口与体力恢复调度；移动生命周期交给 GAS AbilityTask。 */
UCLASS(ClassGroup=(Shooter),meta=(BlueprintSpawnableComponent))
class MYSHOOT_API UShooterDashComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShooterDashComponent();
    UFUNCTION(BlueprintCallable,Category="Shooter|Dash") bool TryDash();
    UFUNCTION(BlueprintCallable,Category="Shooter|Dash") void CancelDash();
    UFUNCTION(BlueprintPure,Category="Shooter|Dash") bool IsDashing() const;
    UFUNCTION(BlueprintPure,Category="Shooter|Dash") float GetStamina() const;
    UFUNCTION(BlueprintPure,Category="Shooter|Dash") float GetMaxStamina() const;
    UFUNCTION(BlueprintPure,Category="Shooter|Dash") float GetCooldownRemaining() const;
    bool CanStartDash() const;
    FVector GetDashDirection() const;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Shooter|Dash",meta=(ClampMin="50",ClampMax="1000",Units="cm")) float DashDistance=480.f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Shooter|Dash",meta=(ClampMin="0.05",ClampMax="0.8",Units="s")) float DashDuration=.22f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void RegenerateStamina();
    UFUNCTION() void HandleRoundChanged(EShooterRoundState State);
    TWeakObjectPtr<AMyShooter> Character;
    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
    TWeakObjectPtr<AShooterGameMode> GameMode;
    FGameplayAbilitySpecHandle DashHandle;
    FTimerHandle RegenTimer;
};