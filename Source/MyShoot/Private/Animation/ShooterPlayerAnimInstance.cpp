#include "Animation/ShooterPlayerAnimInstance.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterAimComponent.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"

void UShooterPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const AMyShooter* Owner = Cast<AMyShooter>(TryGetPawnOwner());
    SetShooterAimAlpha(Owner && Owner->GetShooterAim() ? Owner->GetShooterAim()->GetAimAlpha() : 0.f);
    const auto* Weapon=Owner?Owner->GetShooterWeapon():nullptr;
    const auto* Definition=Weapon?Weapon->GetWeaponDefinition():nullptr;
    const bool bGrip=Definition&&Definition->bUseLeftHandIK&&!Owner->HasGASDeathStarted()&&!Weapon->IsReloading();
    ShooterGripAlpha=bGrip?FMath::FInterpTo(ShooterGripAlpha,1.f,DeltaSeconds,18.f):0.f;
    if(bGrip)
    {
        // 用同一帧的两个组件空间变换提取固定插槽偏移，不从上帧手的位置反推，避免 IK 漂移累积。
        const auto* Mesh=Owner->GetMesh();
        const FTransform Bone=Mesh->GetSocketTransform(TEXT("b_RightWeapon"),RTS_Component);
        const FTransform Socket=Mesh->GetSocketTransform(TEXT("Right_Weapon"),RTS_Component);
        ShooterGripTarget=Bone.InverseTransformPosition(Socket.TransformPosition(Definition->LeftHandGripLocation));
        ShooterElbowTarget=Bone.InverseTransformPosition(Socket.TransformPosition(Definition->LeftElbowHint));
    }
}
