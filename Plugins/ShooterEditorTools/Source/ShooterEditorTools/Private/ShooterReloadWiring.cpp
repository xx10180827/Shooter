#include "ShooterReloadWiring.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_InputAction.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetCompiler.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    const TCHAR* ReloadMontagePackage = TEXT("/Game/Blueprints/Shooter_reload_GAS_Montage");

    UEdGraphNode* FindNode(UEdGraph* Graph, const TCHAR* Guid)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->NodeGuid.ToString() == Guid) { return Node; }
        }
        return nullptr;
    }

    bool Connect(UEdGraphNode* A, const TCHAR* Out, UEdGraphNode* B, const TCHAR* In)
    {
        UEdGraphPin* From = A ? A->FindPin(Out) : nullptr;
        UEdGraphPin* To = B ? B->FindPin(In) : nullptr;
        return From && To && GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From, To);
    }

    UK2Node_CallFunction* Call(UEdGraph* Graph, UClass* Class, const TCHAR* Name, int32 X, int32 Y, bool bSelf = false)
    {
        UFunction* Function = Class->FindFunctionByName(Name);
        if (!Function) { return nullptr; }
        FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
        UK2Node_CallFunction* Node = Creator.CreateNode();
        Node->SetFromFunction(Function);
        if (bSelf) { Node->FunctionReference.SetSelfMember(Function->GetFName()); }
        Node->NodePosX = X;
        Node->NodePosY = Y;
        Creator.Finalize();
        return Node;
    }

    UK2Node_AddDelegate* Bind(UEdGraph* Graph, FMulticastDelegateProperty* Property, int32 X)
    {
        FGraphNodeCreator<UK2Node_AddDelegate> Creator(*Graph);
        UK2Node_AddDelegate* Node = Creator.CreateNode();
        Node->SetFromProperty(Property, false, UShooterWeaponComponent::StaticClass());
        Node->NodePosX = X;
        Node->NodePosY = -1440;
        Creator.Finalize();
        return Node;
    }

    void Comment(UEdGraph* Graph, const TCHAR* Text, int32 X, int32 Y, int32 W, int32 H)
    {
        FGraphNodeCreator<UEdGraphNode_Comment> Creator(*Graph);
        UEdGraphNode_Comment* Node = Creator.CreateNode();
        Creator.Finalize();
        Node->NodeComment = Text;
        Node->NodePosX = X;
        Node->NodePosY = Y;
        Node->NodeWidth = W;
        Node->NodeHeight = H;
        Node->CommentColor = FLinearColor(0.15f, 0.35f, 0.55f);
        Node->FontSize = 20;
    }

    bool Save(UObject* Asset)
    {
        UPackage* Package = Asset->GetOutermost();
        Package->MarkPackageDirty();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        return UPackage::SavePackage(Package, Asset, *Filename, Args);
    }
}

bool ShooterReloadWiring::Migrate(UBlueprint* Blueprint, FString& Result)
{
    if (!Blueprint || Blueprint->GetPathName() != TEXT("/Game/Blueprints/Shooter.Shooter")
        || !Blueprint->GeneratedClass || Blueprint->UbergraphPages.Num() != 1)
    {
        Result = TEXT("Unexpected Shooter blueprint. Nothing saved.");
        return false;
    }
    UEdGraph* Graph = Blueprint->UbergraphPages[0];
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node);
        if (Event && Event->CustomFunctionName == TEXT("HandleReloadStarted"))
        {
            Result = TEXT("Reload already connected; use VerifyT05.");
            return false;
        }
    }
    UEdGraphNode* ShotBind = FindNode(Graph, TEXT("C98A40DB446907BD9A77399AB938F0C6"));
    UEdGraphNode* Weapon = FindNode(Graph, TEXT("E584325D4A675ED89223BE93CF0ECDB4"));
    if (!ShotBind || !Weapon || !ShotBind->FindPin(TEXT("then"))
        || ShotBind->FindPin(TEXT("then"))->LinkedTo.Num() > 1)
    {
        Result = TEXT("T04 binding chain differs from inspected baseline. Nothing saved.");
        return false;
    }
    UEdGraphPin* ExistingNext = ShotBind->FindPin(TEXT("then"))->LinkedTo.Num() == 1
        ? ShotBind->FindPin(TEXT("then"))->LinkedTo[0] : nullptr;
    // 用户增加了共用的弹药打印：只在 BeginPlay 分支前插入绑定，不能接到打印之后，
    // 否则每发射击也会经过绑定。保留打印的参数、射击入口和原节点位置。
    if (ExistingNext && (ExistingNext->GetOwningNode()->NodeGuid.ToString() != TEXT("A117F88B43BA51713B8650B61D3EB4B6")
        || ExistingNext->PinName != TEXT("execute")))
    {
        Result = TEXT("Unexpected continuation after shot binding. Nothing saved.");
        return false;
    }
    AMyShooter* Defaults = Cast<AMyShooter>(Blueprint->GeneratedClass->GetDefaultObject());
    UAnimMontage* Fire = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Blueprints/Shooter_fire_Montage.Shooter_fire_Montage"));
    UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Assets/Characters/FPP_Animations/FPP_RifleReload.FPP_RifleReload"));
    USkeletalMesh* Mesh = Defaults ? Defaults->GetMesh()->GetSkeletalMeshAsset() : nullptr;
    if (!Sequence || !Fire || !Mesh || Sequence->GetSkeleton() != Mesh->GetSkeleton()
        || Fire->GetSkeleton() != Mesh->GetSkeleton() || Fire->SlotAnimTracks.IsEmpty())
    {
        Result = FString::Printf(TEXT("Reload skeleton mismatch: sequence=%s mesh=%s fire=%s. Nothing saved."),
            *GetNameSafe(Sequence ? Sequence->GetSkeleton() : nullptr),
            *GetNameSafe(Mesh ? Mesh->GetSkeleton() : nullptr), *GetNameSafe(Fire ? Fire->GetSkeleton() : nullptr));
        return false;
    }
    if (FPackageName::DoesPackageExist(ReloadMontagePackage))
    {
        Result = TEXT("Reload montage already exists; refusing to overwrite it.");
        return false;
    }

    // 使用现有第一人称换弹序列和开火 Slot；新建蒙太奇，不修改原动画资源。
    UAnimMontage* Dynamic = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
        Sequence, Fire->SlotAnimTracks[0].SlotName, 0.08f, 0.08f, 1.0f, 1);
    if (!Dynamic) { Result = TEXT("Cannot create reload montage."); return false; }
    UPackage* Package = CreatePackage(ReloadMontagePackage);
    UAnimMontage* Montage = DuplicateObject<UAnimMontage>(Dynamic, Package, TEXT("Shooter_reload_GAS_Montage"));
    Montage->ClearFlags(RF_Transient);
    Montage->SetFlags(RF_Public | RF_Standalone);
    // 动态蒙太奇只赋值运行时长度；持久资产还需同步编辑器的动画数据模型。
    Montage->SetCompositeLength(Sequence->GetPlayLength());
    Montage->PostEditChange();
    if (Montage->GetPlayLength() <= 0.0f)
    {
        Result = TEXT("Reload montage has no playable length. Nothing saved.");
        return false;
    }

    FMulticastDelegateProperty* StartedProperty = FindFProperty<FMulticastDelegateProperty>(
        UShooterWeaponComponent::StaticClass(), TEXT("OnReloadStarted"));
    FMulticastDelegateProperty* FinishedProperty = FindFProperty<FMulticastDelegateProperty>(
        UShooterWeaponComponent::StaticClass(), TEXT("OnReloadFinished"));
    if (!StartedProperty || !FinishedProperty) { Result = TEXT("Reload delegates missing."); return false; }
    UK2Node_AddDelegate* StartedBind = Bind(Graph, StartedProperty, 1600);
    UK2Node_AddDelegate* FinishedBind = Bind(Graph, FinishedProperty, 2050);
    UK2Node_CustomEvent* Started = UK2Node_CustomEvent::CreateFromFunction(
        FVector2D(1600, -950), Graph, TEXT("HandleReloadStarted"), StartedProperty->SignatureFunction, false);
    UK2Node_CustomEvent* Finished = UK2Node_CustomEvent::CreateFromFunction(
        FVector2D(1600, -150), Graph, TEXT("HandleReloadFinished"), FinishedProperty->SignatureFunction, false);
    FGraphNodeCreator<UK2Node_InputAction> InputCreator(*Graph);
    UK2Node_InputAction* Input = InputCreator.CreateNode();
    Input->InputActionName = TEXT("reload");
    Input->NodePosX = 1600;
    Input->NodePosY = -2300;
    InputCreator.Finalize();
    UK2Node_CallFunction* InputWeapon = Call(Graph, AMyShooter::StaticClass(), TEXT("GetShooterWeapon"), 1600, -2080, true);
    UK2Node_CallFunction* Reload = Call(Graph, UShooterWeaponComponent::StaticClass(), TEXT("StartReloading"), 2050, -2300);
    UK2Node_CallFunction* Play = Call(Graph, ACharacter::StaticClass(), TEXT("PlayAnimMontage"), 2450, -950, true);
    UK2Node_CallFunction* Stop = Call(Graph, ACharacter::StaticClass(), TEXT("StopAnimMontage"), 2100, -150, true);
    UK2Node_CallFunction* Length = Call(Graph, UAnimationAsset::StaticClass(), TEXT("GetPlayLength"), 1600, -680);
    UK2Node_CallFunction* Divide = Call(Graph, UKismetMathLibrary::StaticClass(), TEXT("Divide_DoubleDouble"), 2050, -650);
    if (!Play || !Stop || !Length || !Divide || !InputWeapon || !Reload)
    {
        Result = TEXT("Required animation/input function missing. Nothing saved.");
        return false;
    }
    Play->FindPinChecked(TEXT("AnimMontage"))->DefaultObject = Montage;
    Stop->FindPinChecked(TEXT("AnimMontage"))->DefaultObject = Montage;
    Length->FindPinChecked(TEXT("self"))->DefaultObject = Montage;

    // 只有 BeginPlay 绑定链延长；原射击事件与所有旧函数的节点和连线保留。
    if (ExistingNext) { ShotBind->FindPin(TEXT("then"))->BreakLinkTo(ExistingNext); }
    bool bConnected = Connect(ShotBind, TEXT("then"), StartedBind, TEXT("execute"));
    bConnected &= Connect(StartedBind, TEXT("then"), FinishedBind, TEXT("execute"));
    if (ExistingNext)
    {
        bConnected &= GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(FinishedBind->FindPinChecked(TEXT("then")), ExistingNext);
    }
    bConnected &= Connect(Weapon, TEXT("ReturnValue"), StartedBind, TEXT("self"));
    bConnected &= Connect(Weapon, TEXT("ReturnValue"), FinishedBind, TEXT("self"));
    bConnected &= Connect(Started, TEXT("OutputDelegate"), StartedBind, TEXT("Delegate"));
    bConnected &= Connect(Finished, TEXT("OutputDelegate"), FinishedBind, TEXT("Delegate"));
    bConnected &= Connect(Input, TEXT("Pressed"), Reload, TEXT("execute"));
    bConnected &= Connect(InputWeapon, TEXT("ReturnValue"), Reload, TEXT("self"));
    bConnected &= Connect(Started, TEXT("then"), Play, TEXT("execute"));
    bConnected &= Connect(Length, TEXT("ReturnValue"), Divide, TEXT("A"));
    bConnected &= Connect(Started, TEXT("Duration"), Divide, TEXT("B"));
    bConnected &= Connect(Divide, TEXT("ReturnValue"), Play, TEXT("InPlayRate"));
    bConnected &= Connect(Finished, TEXT("then"), Stop, TEXT("execute"));
    if (!bConnected) { Result = TEXT("At least one reload pin connection failed. Nothing saved."); return false; }
    Comment(Graph, TEXT("T05 输入：R 请求 GAS 换弹；重复按键不会重启，松开 R 不取消"), 1480, -2440, 1150, 570);
    Comment(Graph, TEXT("T05 绑定：BeginPlay 延续 T04 绑定链，连接换弹开始/结束事件"), 1480, -1580, 1150, 420);
    Comment(Graph, TEXT("T05 表现：动画长度 / Duration = 播放速率；弹药只由 C++ 完成时转移"), 1480, -1090, 1450, 690);
    Comment(Graph, TEXT("T05 收尾：完成或取消均只停止换弹蒙太奇，不修改弹药，不停止死亡动画"), 1480, -290, 1350, 400);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Log);
    if (Log.NumErrors || Blueprint->Status == BS_Error)
    {
        Result = FString::Printf(TEXT("Reload blueprint compilation failed (%d). Nothing saved."), Log.NumErrors);
        return false;
    }
    if (!Save(Montage) || !Save(Blueprint)) { Result = TEXT("Failed to save reload assets."); return false; }
    Result = FString::Printf(TEXT("T05 connected and saved: R input, started/finished delegates, reload montage. Sequence=%s Skeleton=%s Slot=%s Length=%.3f"),
        *Sequence->GetPathName(), *GetNameSafe(Mesh->GetSkeleton()), *Fire->SlotAnimTracks[0].SlotName.ToString(), Montage->GetPlayLength());
    return true;
}

// 对本工具新建的 T05 蒙太奇同步时长，限制资产和引用，保留蓝图接线。
bool ShooterReloadWiring::RefreshMontage(FString& Result)
{
    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr,
        TEXT("/Game/Blueprints/Shooter_reload_GAS_Montage.Shooter_reload_GAS_Montage"));
    UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Assets/Characters/FPP_Animations/FPP_RifleReload.FPP_RifleReload"));
    if (!Montage || !Sequence || Sequence->GetPlayLength() <= 0.0f
        || Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1
        || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference() != Sequence)
    {
        Result = TEXT("Unexpected reload montage source; nothing saved.");
        return false;
    }
    Montage->SetCompositeLength(Sequence->GetPlayLength());
    Montage->PostEditChange();
    if (Montage->GetPlayLength() <= 0.0f || !Save(Montage))
    {
        Result = TEXT("Failed to synchronize reload montage length.");
        return false;
    }
    Result = FString::Printf(TEXT("Reload montage length synchronized: %.3f seconds; source %.3f seconds."),
        Montage->GetPlayLength(), Sequence->GetPlayLength());
    return true;
}