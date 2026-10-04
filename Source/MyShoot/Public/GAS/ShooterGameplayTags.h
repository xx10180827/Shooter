#pragma once

#include "NativeGameplayTags.h"

// 集中声明玩法标签，避免各模块重复使用字符串而出现拼写不一致。
namespace ShooterGameplayTags
{
    MYSHOOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dashing);
    MYSHOOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
    MYSHOOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dash);
    MYSHOOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);
    MYSHOOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
    MYSHOOT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Reloading);
}
