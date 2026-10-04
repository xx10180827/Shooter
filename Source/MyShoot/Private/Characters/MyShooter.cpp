#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterAimComponent.h"
#include "Components/InputComponent.h"
#include "Weapons/ShooterWeaponPresentationComponent.h"
#include "Weapons/ShooterRecoilComponent.h"
#include "Interaction/ShooterInteractionComponent.h"
#include "Movement/ShooterDashComponent.h"

// 只在玩家扩展类中创建武器；敌人继续继承公共基类，后续按 AI 攻击方案接入。
AMyShooter::AMyShooter()
{
    ShooterDash=CreateDefaultSubobject<UShooterDashComponent>(TEXT("ShooterDash"));
    ShooterInteraction=CreateDefaultSubobject<UShooterInteractionComponent>(TEXT("ShooterInteraction"));
    ShooterAim = CreateDefaultSubobject<UShooterAimComponent>(TEXT("ShooterAim"));
    ShooterWeapon = CreateDefaultSubobject<UShooterWeaponComponent>(TEXT("ShooterWeapon"));
    ShooterRecoil = CreateDefaultSubobject<UShooterRecoilComponent>(TEXT("ShooterRecoil"));
    WeaponPresentation = CreateDefaultSubobject<UShooterWeaponPresentationComponent>(TEXT("WeaponPresentation"));
}

void AMyShooter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAction(TEXT("dash"), IE_Pressed, this, &AMyShooter::DashInput);
    PlayerInputComponent->BindAction(TEXT("interact"), IE_Pressed, this, &AMyShooter::InteractInput);
    PlayerInputComponent->BindAction(TEXT("weapon_primary"), IE_Pressed, this, &AMyShooter::EquipPrimaryInput);
    PlayerInputComponent->BindAction(TEXT("weapon_secondary"), IE_Pressed, this, &AMyShooter::EquipSecondaryInput);
    // 仅右键按下切换；不绑定松开，符合用户选择的切换开镜方式。
    PlayerInputComponent->BindAction(TEXT("aim"), IE_Pressed, this, &AMyShooter::ToggleAimInput);
}
void AMyShooter::ToggleAimInput() { if (ShooterAim) { ShooterAim->ToggleAiming(); } }
void AMyShooter::UnPossessed()
{
    if (ShooterAim) { ShooterAim->ResetAiming(); }
    if (ShooterRecoil) { ShooterRecoil->CancelPendingRecoil(); }
    if(ShooterDash) { ShooterDash->CancelDash(); }
    Super::UnPossessed();
}

void AMyShooter::EquipPrimaryInput() { if (ShooterWeapon) { ShooterWeapon->EquipWeapon(0); } }
void AMyShooter::EquipSecondaryInput() { if (ShooterWeapon) { ShooterWeapon->EquipWeapon(1); } }

void AMyShooter::DashInput() { if(ShooterDash) { ShooterDash->TryDash(); } }
void AMyShooter::InteractInput() { if(ShooterInteraction) { ShooterInteraction->TryInteract(); } }
