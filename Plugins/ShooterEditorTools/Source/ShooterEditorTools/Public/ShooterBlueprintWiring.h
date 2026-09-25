#pragma once

#include "CoreMinimal.h"

class UBlueprint;

// 只供编辑器命令使用：迁移先检查指定蓝图结构，成功编译后才保存。
namespace ShooterBlueprintWiring
{
    bool MigrateT04(UBlueprint* Blueprint, FString& Result);
    bool AnnotateT04(UBlueprint* Blueprint, FString& Result);
    bool VerifyT04(UBlueprint* Blueprint, FString& Result);
}
