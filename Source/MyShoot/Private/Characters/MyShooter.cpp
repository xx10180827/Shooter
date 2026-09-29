#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterAimComponent.h"
#include "Components/InputComponent.h"

// 只在玩家扩展类中创建武器；敌人继续继承公共基类，后续按 AI 攻击方案接入。
AMyShooter::AMyShooter()
{
    ShooterAim = CreateDefaultSubobject<UShooterAimComponent>(TEXT("ShooterAim"));
    ShooterWeapon = CreateDefaultSubobject<UShooterWeaponComponent>(TEXT("ShooterWeapon"));
}

void AMyShooter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // 仅右键按下切换；不绑定松开，符合用户选择的切换开镜方式。
    PlayerInputComponent->BindAction(TEXT("aim"), IE_Pressed, this, &AMyShooter::ToggleAimInput);
}
void AMyShooter::ToggleAimInput() { if (ShooterAim) { ShooterAim->ToggleAiming(); } }
void AMyShooter::UnPossessed()
{
    if (ShooterAim) { ShooterAim->ResetAiming(); }
    Super::UnPossessed();
}
