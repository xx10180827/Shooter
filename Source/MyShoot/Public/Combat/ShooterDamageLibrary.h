#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/HitResult.h"
#include "ShooterDamageLibrary.generated.h"

UCLASS()
class MYSHOOT_API UShooterDamageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 返回 true 表示目标实际掉血；SourceActor 兼容尚未接入 ASC 的旧蓝图。
    UFUNCTION(BlueprintCallable, Category = "Shooter|Combat",
        meta = (DisplayName = "Apply GAS Damage", AutoCreateRefTerm = "HitResult",
            AdvancedDisplay = "DamageCauser,HitResult"))
    static bool ApplyGASDamage(AActor* SourceActor, AActor* TargetActor, float Damage,
        AActor* DamageCauser, const FHitResult& HitResult);
};
