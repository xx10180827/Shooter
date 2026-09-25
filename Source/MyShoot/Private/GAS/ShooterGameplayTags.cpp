#include "GAS/ShooterGameplayTags.h"

// 原生标签随模块自动注册；死亡流程维护 Dead，后续换弹能力维护 Reloading。
namespace ShooterGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
    UE_DEFINE_GAMEPLAY_TAG(State_Reloading, "State.Reloading");
}
