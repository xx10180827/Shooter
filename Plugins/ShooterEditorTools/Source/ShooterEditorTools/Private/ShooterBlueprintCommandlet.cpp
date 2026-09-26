#include "ShooterBlueprintCommandlet.h"
#include "ShooterBlueprintWiring.h"
#include "ShooterReloadWiring.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CallFunction.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Animation/AnimMontage.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterBlueprintTool, Log, All);

UShooterBlueprintCommandlet::UShooterBlueprintCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

// 导出精确的节点 GUID、引脚与连接，用于检查迁移范围；不会保存或修改蓝图资产。
static bool ExportBlueprint(UBlueprint* Blueprint, const FString& Directory)
{
    IFileManager::Get().MakeDirectory(*Directory, true);
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("asset"), Blueprint->GetPathName());
    Root->SetStringField(TEXT("parent"), Blueprint->ParentClass->GetPathName());
    TArray<TSharedPtr<FJsonValue>> GraphValues;
    for (UEdGraph* Graph : Graphs)
    {
        TSharedRef<FJsonObject> GraphJson = MakeShared<FJsonObject>();
        GraphJson->SetStringField(TEXT("name"), Graph->GetName());
        TArray<TSharedPtr<FJsonValue>> Nodes;
        TSet<UObject*> ExportSet;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) { continue; }
            ExportSet.Add(Node);
            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
            Item->SetStringField(TEXT("name"), Node->GetName());
            Item->SetStringField(TEXT("type"), Node->GetClass()->GetName());
            Item->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
            Item->SetStringField(TEXT("comment"), Node->NodeComment);
            Item->SetNumberField(TEXT("x"), Node->NodePosX);
            Item->SetNumberField(TEXT("y"), Node->NodePosY);
            if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
            {
                Item->SetStringField(TEXT("function"), Call->FunctionReference.GetMemberName().ToString());
            }
            TArray<TSharedPtr<FJsonValue>> Pins;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                TSharedRef<FJsonObject> PinJson = MakeShared<FJsonObject>();
                PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
                PinJson->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
                PinJson->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"));
                PinJson->SetStringField(TEXT("default"), Pin->DefaultValue);
                if (Pin->DefaultObject) { PinJson->SetStringField(TEXT("object"), Pin->DefaultObject->GetPathName()); }
                TArray<TSharedPtr<FJsonValue>> Links;
                for (UEdGraphPin* Other : Pin->LinkedTo)
                {
                    Links.Add(MakeShared<FJsonValueString>(
                        Other->GetOwningNode()->NodeGuid.ToString() + TEXT(":") + Other->PinName.ToString()));
                }
                PinJson->SetArrayField(TEXT("links"), Links);
                Pins.Add(MakeShared<FJsonValueObject>(PinJson));
            }
            Item->SetArrayField(TEXT("pins"), Pins);
            Nodes.Add(MakeShared<FJsonValueObject>(Item));
        }
        GraphJson->SetArrayField(TEXT("nodes"), Nodes);
        GraphValues.Add(MakeShared<FJsonValueObject>(GraphJson));
        FString Text;
        FEdGraphUtilities::ExportNodesToText(ExportSet, Text);
        FFileHelper::SaveStringToFile(Text, *(Directory / (Graph->GetName() + TEXT(".txt"))),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
    Root->SetArrayField(TEXT("graphs"), GraphValues);
    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    return FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("graphs.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}


// 同时记录旧表现资源的循环配置；迁移按实际资源决定是否需要单发副本。
static void ExportPresentationResources(const FString& Directory)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    UParticleSystem* Particle = LoadObject<UParticleSystem>(nullptr,
        TEXT("/Game/Assets/Effects/ParticleSystems/Weapons/AssaultRifle/Muzzle/P_AssaultRifle_MF.P_AssaultRifle_MF"));
    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    if (Particle)
    {
        for (UParticleEmitter* Emitter : Particle->Emitters)
        {
            for (UParticleLODLevel* LOD : Emitter->LODLevels)
            {
                if (!LOD || !LOD->RequiredModule) { continue; }
                TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
                Item->SetStringField(TEXT("emitter"), Emitter->GetName());
                Item->SetNumberField(TEXT("loops"), LOD->RequiredModule->EmitterLoops);
                Item->SetNumberField(TEXT("duration"), LOD->RequiredModule->EmitterDuration);
                EmitterValues.Add(MakeShared<FJsonValueObject>(Item));
            }
        }
    }
    Root->SetArrayField(TEXT("particle"), EmitterValues);
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr,
        TEXT("/Game/Blueprints/Shooter_fire_Montage.Shooter_fire_Montage"));
    TArray<TSharedPtr<FJsonValue>> SectionValues;
    if (Montage)
    {
        Root->SetNumberField(TEXT("montageLength"), Montage->GetPlayLength());
        for (const FCompositeSection& Section : Montage->CompositeSections)
        {
            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("section"), Section.SectionName.ToString());
            Item->SetStringField(TEXT("next"), Section.NextSectionName.ToString());
            SectionValues.Add(MakeShared<FJsonValueObject>(Item));
        }
    }
    Root->SetArrayField(TEXT("sections"), SectionValues);
    FString Json;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
    FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("presentation.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

int32 UShooterBlueprintCommandlet::Main(const FString& Params)
{
    FString Asset = TEXT("/Game/Blueprints/Shooter.Shooter");
    FParse::Value(*Params, TEXT("Asset="), Asset);
    FString Output = TEXT("Saved/T04/BlueprintWiring/Inspect");
    FParse::Value(*Params, TEXT("Output="), Output);
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Asset);
    if (!Blueprint)
    {
        UE_LOG(LogShooterBlueprintTool, Error, TEXT("Cannot load blueprint: %s"), *Asset);
        return 1;
    }
    FString OperationResult;
    if (FParse::Param(*Params, TEXT("MigrateT04")))
    {
        if (!ShooterBlueprintWiring::MigrateT04(Blueprint, OperationResult))
        {
            UE_LOG(LogShooterBlueprintTool, Error, TEXT("%s"), *OperationResult);
            return 2;
        }
        UE_LOG(LogShooterBlueprintTool, Display, TEXT("%s"), *OperationResult);
    }
    if (FParse::Param(*Params, TEXT("AnnotateT04")))
    {
        if (!ShooterBlueprintWiring::AnnotateT04(Blueprint, OperationResult))
        {
            UE_LOG(LogShooterBlueprintTool, Error, TEXT("%s"), *OperationResult);
            return 4;
        }
        UE_LOG(LogShooterBlueprintTool, Display, TEXT("%s"), *OperationResult);
    }

    if (FParse::Param(*Params, TEXT("VerifyT04")))
    {
        if (!ShooterBlueprintWiring::VerifyT04(Blueprint, OperationResult))
        {
            UE_LOG(LogShooterBlueprintTool, Error, TEXT("%s"), *OperationResult);
            return 3;
        }
        UE_LOG(LogShooterBlueprintTool, Display, TEXT("%s"), *OperationResult);
    }

    if (FParse::Param(*Params, TEXT("MigrateT05")))
    {
        if (!ShooterReloadWiring::Migrate(Blueprint, OperationResult))
        {
            UE_LOG(LogShooterBlueprintTool, Error, TEXT("%s"), *OperationResult);
            return 5;
        }
        UE_LOG(LogShooterBlueprintTool, Display, TEXT("%s"), *OperationResult);
    }
    if (FParse::Param(*Params, TEXT("RefreshT05Montage")))
    {
        if (!ShooterReloadWiring::RefreshMontage(OperationResult))
        {
            UE_LOG(LogShooterBlueprintTool, Error, TEXT("%s"), *OperationResult);
            return 7;
        }
        UE_LOG(LogShooterBlueprintTool, Display, TEXT("%s"), *OperationResult);
    }
    if (FParse::Param(*Params, TEXT("VerifyT05")))
    {
        if (!ShooterReloadWiring::Verify(Blueprint, OperationResult))
        {
            UE_LOG(LogShooterBlueprintTool, Error, TEXT("%s"), *OperationResult);
            return 6;
        }
        UE_LOG(LogShooterBlueprintTool, Display, TEXT("%s"), *OperationResult);
    }

    const bool bExported = ExportBlueprint(Blueprint, FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Output));
    ExportPresentationResources(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Output));
    UE_LOG(LogShooterBlueprintTool, Display, TEXT("Inspection %s: %s"), bExported ? TEXT("succeeded") : TEXT("failed"), *Asset);
    return bExported ? 0 : 1;
}
