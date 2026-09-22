#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ShooterInitialAttributesEffect.generated.h"

// Instant startup effect; sets MaxHealth before Health using one configured value.
UCLASS()
class MYSHOOT_API UShooterInitialAttributesEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UShooterInitialAttributesEffect();
    static FGameplayTag GetInitialHealthTag();
};
