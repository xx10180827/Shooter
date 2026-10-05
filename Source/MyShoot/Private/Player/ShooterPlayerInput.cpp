#include "Player/ShooterPlayerInput.h"
#include "Settings/ShooterUserSettings.h"
#include "Weapons/ShooterAimComponent.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

float UShooterPlayerInput::MassageAxisInput(FKey Key,float RawValue)
{
    const float Base=Super::MassageAxisInput(Key,RawValue);
    if(Key!=EKeys::MouseX&&Key!=EKeys::MouseY) { return Base; }
    const auto* PC=GetOuterAPlayerController(); const auto* Pawn=PC?PC->GetPawn():nullptr;
    const auto* Aim=Pawn?Pawn->FindComponentByClass<UShooterAimComponent>():nullptr;
    const auto* Weapon=Pawn?Pawn->FindComponentByClass<UShooterWeaponComponent>():nullptr;
    const auto* Definition=Weapon?Weapon->GetWeaponDefinition():nullptr;
    const FName Profile=Definition?Definition->AimSensitivityProfile:UShooterUserSettings::DefaultAimProfile();
    const auto* Settings=UShooterUserSettings::Get();
    const FVector2D Scale=Settings?Settings->ResolveSensitivity(Profile,Aim?Aim->GetAimAlpha():0.f):FVector2D(.8,.8);
    // 鼠标轴本来就是帧内位移，不再乘 DeltaSeconds；Dash FOV 不参与计算。
    return Base*float(Key==EKeys::MouseX?Scale.X:Scale.Y);
}