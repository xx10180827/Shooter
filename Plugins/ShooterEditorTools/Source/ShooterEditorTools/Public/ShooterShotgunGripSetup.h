#pragma once
#include "CoreMinimal.h"
class UShooterWeaponDefinition;
class UAnimSequence;
/** 保留原图节点，在最终输出前接入仅霰弹枪启用的左手 IK；支持重复校准和只读验证。 */
int32 SetupShooterShotgunGrip(UShooterWeaponDefinition* Definition,UAnimSequence* Reference,const FString& Params);
