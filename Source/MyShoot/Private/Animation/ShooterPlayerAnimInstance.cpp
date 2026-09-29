#include "Animation/ShooterPlayerAnimInstance.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterAimComponent.h"

void UShooterPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const AMyShooter* Owner = Cast<AMyShooter>(TryGetPawnOwner());
    SetShooterAimAlpha(Owner && Owner->GetShooterAim() ? Owner->GetShooterAim()->GetAimAlpha() : 0.f);
}
