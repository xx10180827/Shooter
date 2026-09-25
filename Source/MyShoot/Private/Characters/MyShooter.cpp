#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"

// 只在玩家扩展类中创建武器；敌人继续继承公共基类，后续按 AI 攻击方案接入。
AMyShooter::AMyShooter()
{
    ShooterWeapon = CreateDefaultSubobject<UShooterWeaponComponent>(TEXT("ShooterWeapon"));
}
