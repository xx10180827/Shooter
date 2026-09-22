#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ShooterCharacterBase.generated.h"

class UAbilitySystemComponent;
class UShooterAttributeSet;

// Single-player foundation: each character owns its own GAS state.
UCLASS()
class MYSHOOT_API AShooterCharacterBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AShooterCharacterBase();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintPure, Category = "Shooter|GAS")
    float GetGASHealth() const;

    UFUNCTION(BlueprintPure, Category = "Shooter|GAS")
    float GetGASMaxHealth() const;

    UFUNCTION(BlueprintPure, Category = "Shooter|GAS")
    bool IsGASInitialized() const { return bGASInitialized; }

protected:
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooter|GAS")
    TObjectPtr<UAbilitySystemComponent> ShooterAbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooter|GAS")
    TObjectPtr<UShooterAttributeSet> ShooterAttributes;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|GAS",
        meta = (ClampMin = "1.0", UIMin = "1.0"))
    float InitialMaxHealth = 100.0f;

private:
    void InitializeGAS();

    bool bGASInitialized = false;
};
