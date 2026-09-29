#include "ShooterAIRangedSetupCommandlet.h"
#include "AI/ShooterAIController.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAIRangedSetup, Log, All);

UShooterAIRangedSetupCommandlet::UShooterAIRangedSetupCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UShooterAIRangedSetupCommandlet::Main(const FString& Params)
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr,
        TEXT("/Game/Blueprints/Boot_Shooter_controller.Boot_Shooter_controller"));
    if (!Blueprint || !Blueprint->GeneratedClass) { return 1; }
    UObject* Defaults = Blueprint->GeneratedClass->GetDefaultObject();
    const TMap<FName, float> Settings = {
        {TEXT("DetectionRange"), 2000.f}, {TEXT("LoseTargetRange"), 3000.f},
        {TEXT("AttackRange"), 1200.f}, {TEXT("SightHalfAngle"), 60.f},
        {TEXT("FireHalfAngle"), 15.f}, {TEXT("SightMemorySeconds"), 3.f},
        {TEXT("MoveAcceptanceRadius"), 75.f}
    };
    const bool bApply = FParse::Param(*Params, TEXT("Apply"));
    const bool bVerify = FParse::Param(*Params, TEXT("Verify"));
    for (const auto& Setting : Settings)
    {
        FFloatProperty* Property = FindFProperty<FFloatProperty>(Defaults->GetClass(), Setting.Key);
        if (!Property) { return 2; }
        const float OldValue = Property->GetPropertyValue_InContainer(Defaults);
        UE_LOG(LogShooterAIRangedSetup, Display, TEXT("%s current=%.2f configured=%.2f"),
            *Setting.Key.ToString(), OldValue, Setting.Value);
        if (bVerify && !FMath::IsNearlyEqual(OldValue, Setting.Value)) { return 3; }
        if (bApply) { Property->SetPropertyValue_InContainer(Defaults, Setting.Value); }
    }
    if (bApply)
    {
        // 仅变更这些交战参数。现有伤害、射速、动画、声音和蓝图图表保留。
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        FCompilerResultsLog CompilerLog;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompilerLog);
        if (CompilerLog.NumErrors > 0) { return 4; }
        UPackage* Package = Blueprint->GetOutermost();
        Package->MarkPackageDirty();
        FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs)) { return 5; }
    }
    UE_LOG(LogShooterAIRangedSetup, Display, TEXT("AI ranged configuration %s passed"), bApply ? TEXT("save") : TEXT("inspect/verify"));
    return 0;
}