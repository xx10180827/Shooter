#pragma once
#include "CoreMinimal.h"
class UBlueprint;

// T05 编辑器工具：保留旧图，仅增加换弹输入、委托和动画表现。
namespace ShooterReloadWiring
{
    bool Migrate(UBlueprint* Blueprint, FString& Result);
    bool RefreshMontage(FString& Result);
    bool Verify(UBlueprint* Blueprint, FString& Result);
}
