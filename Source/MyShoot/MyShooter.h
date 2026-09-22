#pragma once

#include "CoreMinimal.h"
#include "ShooterCharacterBase.h"
#include "MyShooter.generated.h"

// Player-specific extension point. Existing input stays in the Shooter Blueprint for T01.
UCLASS()
class MYSHOOT_API AMyShooter : public AShooterCharacterBase
{
    GENERATED_BODY()

public:
    AMyShooter();
};
