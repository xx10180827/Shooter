#include "ShooterBlueprintWiring.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
    const TCHAR* SingleShotPackage = TEXT("/Game/Assets/Effects/ParticleSystems/Weapons/AssaultRifle/Muzzle/P_AssaultRifle_MF_GAS");
    const TCHAR* PresentationName = TEXT("GAS_PlayFireEffects");

    UEdGraph* FindGraph(UBlueprint* Blueprint, const TCHAR* Name)
    {
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs) { if (Graph->GetName() == Name) { return Graph; } }
        return nullptr;
    }

    UEdGraphNode* FindNode(UEdGraph* Graph, const TCHAR* Guid)
    {
        if (Graph)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (Node && Node->NodeGuid.ToString() == Guid) { return Node; }
            }
        }
        return nullptr;
    }

    // 所有连接均交给 K2 schema 验证类型，连接失败时禁止保存。
    bool Connect(UEdGraphNode* From, const TCHAR* Out, UEdGraphNode* To, const TCHAR* In)
    {
        UEdGraphPin* A = From ? From->FindPin(Out) : nullptr;
        UEdGraphPin* B = To ? To->FindPin(In) : nullptr;
        return A && B && GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A, B);
    }

    UK2Node_CallFunction* AddCall(UEdGraph* Graph, UFunction* Function, int32 X, int32 Y, bool bSelf = false)
    {
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

    void AddComment(UEdGraph* Graph, const FString& Text, int32 X, int32 Y, int32 Width, int32 Height,
        const FLinearColor Color)
    {
        FGraphNodeCreator<UEdGraphNode_Comment> Creator(*Graph);
        UEdGraphNode_Comment* Comment = Creator.CreateNode();
        Creator.Finalize();
        Comment->NodeComment = Text;
        Comment->NodePosX = X;
        Comment->NodePosY = Y;
        Comment->NodeWidth = Width;
        Comment->NodeHeight = Height;
        Comment->CommentColor = Color;
        Comment->FontSize = 20;
    }

    bool Compile(UBlueprint* Blueprint, FString& Result)
    {
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Log);
        if (Log.NumErrors > 0 || Blueprint->Status == BS_Error)
        {
            Result = FString::Printf(TEXT("Blueprint compilation failed with %d errors; nothing saved."), Log.NumErrors);
            return false;
        }
        return true;
    }

    bool SaveAsset(UObject* Asset)
    {
        UPackage* Package = Asset->GetOutermost();
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *Filename, Args);
    }
}

bool ShooterBlueprintWiring::MigrateT04(UBlueprint* Blueprint, FString& Result)
{
    if (!Blueprint || Blueprint->GetPathName() != TEXT("/Game/Blueprints/Shooter.Shooter"))
    {
        Result = TEXT("Migration is restricted to the inspected Shooter blueprint.");
        return false;
    }
    // 防止重复运行产生第二套绑定、输入或表现节点；已迁移资产只能走验证命令。
    if (FindGraph(Blueprint, PresentationName))
    {
        Result = TEXT("GAS_PlayFireEffects already exists. Run VerifyT04 instead of migrating again.");
        return false;
    }

    UEdGraph* Events = FindGraph(Blueprint, TEXT("EventGraph"));
    UEdGraph* Start = FindGraph(Blueprint, TEXT("Start_fire"));
    UEdGraph* End = FindGraph(Blueprint, TEXT("End_fire"));
    UEdGraph* Shoot = FindGraph(Blueprint, TEXT("Shoot_Once"));
    UEdGraphNode* Input = FindNode(Events, TEXT("CCC847EF4804D3CCEEC6FF961C6B93D5"));
    UEdGraphNode* HandleShot = FindNode(Events, TEXT("E13B84D4483AED1E4D5BAB9040E3D04F"));
    UEdGraphNode* Bind = FindNode(Events, TEXT("C98A40DB446907BD9A77399AB938F0C6"));
    UEdGraphNode* Begin = FindNode(Events, TEXT("9C3091D2460AE06BA1821AA90F72F96C"));
    UEdGraphNode* BindWeapon = FindNode(Events, TEXT("E584325D4A675ED89223BE93CF0ECDB4"));
    UEdGraphNode* OldStartCall = FindNode(Events, TEXT("66C0423347A8D5BBFE846DB1FD5A7F80"));
    UEdGraphNode* OldEndCall = FindNode(Events, TEXT("532928C049B38624ADFDD5B7E1E19BB2"));
    UEdGraphNode* OldMontage = FindNode(Start, TEXT("B582D9FC42387BBD79A333B0CCED29F5"));
    UEdGraphNode* OldEmitter = FindNode(Start, TEXT("E93F2C1345E75AF5A5D6F6AE0D45375D"));
    UEdGraphNode* OldMuzzle = FindNode(Start, TEXT("3A270B2648EBD7F2C18C879B7894CEF1"));
    if (!Events || !Start || !End || !Shoot || !Input || !HandleShot || !Bind || !Begin || !BindWeapon
        || !OldStartCall || !OldEndCall || !OldMontage || !OldEmitter || !OldMuzzle
        || !HandleShot->FindPin(TEXT("bBlockingHit")) || !HandleShot->FindPin(TEXT("HitResult"))
        || !Input->FindPin(TEXT("Pressed")) || !Input->FindPin(TEXT("Released")))
    {
        Result = TEXT("Inspected graph or pin is missing. Refusing to guess connections.");
        return false;
    }

    UParticleSystem* OriginalParticle = Cast<UParticleSystem>(OldEmitter->FindPin(TEXT("EmitterTemplate"))->DefaultObject);
    if (!OriginalParticle || !OldMontage->FindPin(TEXT("AnimMontage"))->DefaultObject
        || FPackageName::DoesPackageExist(SingleShotPackage))
    {
        Result = TEXT("Original presentation assets are missing, or single-shot package already exists.");
        return false;
    }

    // 复制粒子并只修改副本：原资源无限循环，逐发创建时必须改为短暂发射和自动销毁。
    UPackage* ParticlePackage = CreatePackage(SingleShotPackage);
    UParticleSystem* Particle = DuplicateObject<UParticleSystem>(OriginalParticle, ParticlePackage, TEXT("P_AssaultRifle_MF_GAS"));
    if (!Particle) { Result = TEXT("Cannot duplicate muzzle particle."); return false; }
    Particle->SetFlags(RF_Public | RF_Standalone);
    for (int32 Index = 0; Index < Particle->Emitters.Num(); ++Index)
    {
        UParticleEmitter* Emitter = Particle->Emitters[Index];
        if (Emitter == OriginalParticle->Emitters[Index])
        {
            Result = TEXT("Particle subobjects were not duplicated independently.");
            return false;
        }
        for (UParticleLODLevel* LOD : Emitter->LODLevels)
        {
            if (!LOD || !LOD->RequiredModule) { continue; }
            if (LOD->RequiredModule->GetOutermost() != ParticlePackage)
            {
                Result = TEXT("Required module still belongs to the original particle; refusing to edit.");
                return false;
            }
            LOD->RequiredModule->EmitterLoops = 1;
            LOD->RequiredModule->EmitterDuration = 0.05f;
            LOD->RequiredModule->EmitterDurationLow = 0.05f;
            LOD->RequiredModule->bEmitterDurationUseRange = false;
        }
    }
    Particle->UpdateAllModuleLists();
    Particle->PostEditChange();

    // 新建纯表现函数，复制已配置好资源的节点；旧函数和内部连线完整保留。
    UEdGraph* Effects = FBlueprintEditorUtils::CreateNewGraph(Blueprint, PresentationName,
        UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Effects, true, nullptr);
    UK2Node_FunctionEntry* Entry = nullptr;
    for (UEdGraphNode* Node : Effects->Nodes) { if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(Node)) { Entry = E; } }
    if (!Entry) { Result = TEXT("Cannot create presentation function entry."); return false; }
    Entry->CreateUserDefinedPin(TEXT("bBlockingHit"), HandleShot->FindPin(TEXT("bBlockingHit"))->PinType, EGPD_Output);
    Entry->CreateUserDefinedPin(TEXT("HitResult"), HandleShot->FindPin(TEXT("HitResult"))->PinType, EGPD_Output);
    Entry->NodePosX = 0;
    Entry->NodePosY = 0;
    Entry->NodeComment = TEXT("仅在实际发射后执行。命中参数已传入；旧项目没有命中特效，后续可在这里使用。");
    Entry->bCommentBubbleVisible = true;

    TSet<UObject*> Sources;
    Sources.Add(OldMontage);
    Sources.Add(OldEmitter);
    Sources.Add(OldMuzzle);
    FString CopyText;
    FEdGraphUtilities::ExportNodesToText(Sources, CopyText);
    TSet<UEdGraphNode*> Imported;
    FEdGraphUtilities::ImportNodesFromText(Effects, CopyText, Imported);
    UEdGraphNode* Montage = nullptr;
    UEdGraphNode* Emitter = nullptr;
    UEdGraphNode* Muzzle = nullptr;
    for (UEdGraphNode* Node : Imported)
    {
        if (Node->NodeGuid == OldMontage->NodeGuid) { Montage = Node; }
        if (Node->NodeGuid == OldEmitter->NodeGuid) { Emitter = Node; }
        if (Node->NodeGuid == OldMuzzle->NodeGuid) { Muzzle = Node; }
        Node->CreateNewGuid();
    }
    if (!Montage || !Emitter || !Muzzle)
    {
        Result = TEXT("Cannot identify copied presentation nodes.");
        return false;
    }
    Montage->NodePosX = 480; Montage->NodePosY = 0;
    Emitter->NodePosX = 960; Emitter->NodePosY = 0;
    Muzzle->NodePosX = 656; Muzzle->NodePosY = 256;
    Emitter->FindPin(TEXT("EmitterTemplate"))->DefaultObject = Particle;
    Emitter->FindPin(TEXT("bAutoDestroy"))->DefaultValue = TEXT("true");
    Montage->NodeComment = TEXT("复用原来的 Shooter_fire_Montage；每发实际射击播放一次。");
    Montage->bCommentBubbleVisible = true;
    Emitter->NodeComment = TEXT("使用原粒子的 GAS 单发副本：发射 0.05 秒，存活粒子结束后自动销毁，不写旧 Fire_ Effect。");
    Emitter->bCommentBubbleVisible = true;
    bool bConnected = Connect(Entry, TEXT("then"), Montage, TEXT("execute"));
    bConnected &= Connect(Montage, TEXT("then"), Emitter, TEXT("execute"));
    bConnected &= Connect(Muzzle, TEXT("Muzzle"), Emitter, TEXT("AttachToComponent"));
    FBlueprintEditorUtils::SetBlueprintFunctionOrMacroCategory(Effects, FText::FromString(TEXT("GAS|Weapon")), true);
    AddComment(Effects, TEXT("GAS 单发表现：动画与枪口。扣弹、射速、射线、伤害在 C++；此函数不重复结算。"),
        -96, -176, 1580, 740, FLinearColor(0.12f, 0.35f, 0.18f));

    // 先更新函数签名，随后生成调用节点，确保布尔与命中结构引脚正确。
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (!bConnected || !Compile(Blueprint, Result)) { return false; }

    UK2Node_CallFunction* GetWeapon = AddCall(Events,
        AMyShooter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(AMyShooter, GetShooterWeapon)), -1920, -980);
    UK2Node_CallFunction* StartFire = AddCall(Events,
        UShooterWeaponComponent::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UShooterWeaponComponent, StartFiring)), -1480, -1330);
    UK2Node_CallFunction* StopFire = AddCall(Events,
        UShooterWeaponComponent::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UShooterWeaponComponent, StopFiring)), -1480, -1110);
    UK2Node_CallFunction* PlayEffects = AddCall(Events,
        Blueprint->SkeletonGeneratedClass->FindFunctionByName(PresentationName), 480, -896, true);
    if (!GetWeapon || !StartFire || !StopFire || !PlayEffects) { Result = TEXT("Cannot create GAS call nodes."); return false; }

    // 只断开旧入口：旧调用节点、旧函数、纯蓝图和 T02 过渡逻辑仍保留供学习。
    Input->FindPin(TEXT("Pressed"))->BreakAllPinLinks();
    Input->FindPin(TEXT("Released"))->BreakAllPinLinks();
    bConnected &= Connect(Input, TEXT("Pressed"), StartFire, TEXT("execute"));
    bConnected &= Connect(Input, TEXT("Released"), StopFire, TEXT("execute"));
    bConnected &= Connect(GetWeapon, TEXT("ReturnValue"), StartFire, TEXT("self"));
    bConnected &= Connect(GetWeapon, TEXT("ReturnValue"), StopFire, TEXT("self"));
    bConnected &= Connect(HandleShot, TEXT("then"), PlayEffects, TEXT("execute"));
    bConnected &= Connect(HandleShot, TEXT("bBlockingHit"), PlayEffects, TEXT("bBlockingHit"));
    bConnected &= Connect(HandleShot, TEXT("HitResult"), PlayEffects, TEXT("HitResult"));
    Input->NodePosX = -1920; Input->NodePosY = -1280;
    Begin->NodePosX = 0; Begin->NodePosY = -1440;
    BindWeapon->NodePosX = 0; BindWeapon->NodePosY = -1296;
    Bind->NodePosX = 480; Bind->NodePosY = -1440;
    HandleShot->NodePosX = 0; HandleShot->NodePosY = -896;
    HandleShot->NodeComment = TEXT("GAS 每发成功后通知；命中与未命中都会播放开火表现。");
    HandleShot->bCommentBubbleVisible = true;
    OldStartCall->NodePosX = -1904; OldStartCall->NodePosY = -480;
    OldEndCall->NodePosX = -1488; OldEndCall->NodePosY = -480;
    for (UEdGraphNode* OldCall : { OldStartCall, OldEndCall })
    {
        OldCall->NodeComment = TEXT("旧版/过渡入口：保留对比，当前输入不调用。双击进入查看旧节点。");
        OldCall->bCommentBubbleVisible = true;
    }
    if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(FindNode(Events, TEXT("353A867244C3A69909808A82C1524217"))))
    {
        Comment->NodePosX = -2064; Comment->NodePosY = -1488;
        Comment->NodeWidth = 1152; Comment->NodeHeight = 680;
        Comment->NodeComment = TEXT("当前 GAS 输入：按下开始 / 松开取消。表现由实际发射事件触发。");
        Comment->CommentColor = FLinearColor(0.12f, 0.35f, 0.18f);
    }
    if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(FindNode(Events, TEXT("4E2745E34D5612C76B1EE19CB7D80DFC"))))
    {
        Comment->NodePosX = -2064; Comment->NodePosY = -656;
        Comment->NodeWidth = 1152; Comment->NodeHeight = 400;
        Comment->NodeComment = TEXT("旧入口保留区（已断开）：进入 Start_fire / End_fire / Shoot_Once 对比旧实现。");
        Comment->CommentColor = FLinearColor(0.4f, 0.22f, 0.08f);
    }
    AddComment(Events, TEXT("BeginPlay 只绑定一次；每发回调进入 GAS_PlayFireEffects。命中参数留给后续命中特效。"),
        -128, -1600, 1320, 1000, FLinearColor(0.12f, 0.28f, 0.4f));
    for (UEdGraph* Legacy : { Start, End, Shoot })
    {
        FBlueprintEditorUtils::SetBlueprintFunctionOrMacroCategory(Legacy,
            FText::FromString(TEXT("Legacy|保留对比")), true);
        int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
        for (UEdGraphNode* Node : Legacy->Nodes)
        {
            MinX = FMath::Min(MinX, Node->NodePosX);
            MinY = FMath::Min(MinY, Node->NodePosY);
            MaxX = FMath::Max(MaxX, Node->NodePosX + 480);
            MaxY = FMath::Max(MaxY, Node->NodePosY + 320);
        }
        AddComment(Legacy, TEXT("旧版与过渡节点保留，当前输入不调用此函数。新实现见 EventGraph 绿色区域及 GAS_PlayFireEffects。"),
            MinX - 128, MinY - 192, MaxX - MinX + 256, MaxY - MinY + 352, FLinearColor(0.35f, 0.2f, 0.08f));
    }

    if (!bConnected) { Result = TEXT("K2 schema rejected a connection. Nothing saved."); return false; }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (!Compile(Blueprint, Result)) { return false; }

    // 只有全部接线成功且蓝图编译无错误才写入磁盘；原始资产已有独立恢复点。
    if (!SaveAsset(Particle) || !SaveAsset(Blueprint))
    {
        Result = TEXT("Failed to save migrated assets; inspect log and restore point.");
        return false;
    }
    Result = TEXT("Migrated Shooter: GAS input and per-shot presentation connected; all original nodes retained; single-shot particle copy saved.");
    return true;
}

bool ShooterBlueprintWiring::AnnotateT04(UBlueprint* Blueprint, FString& Result)
{
    if (!Blueprint || !FindGraph(Blueprint, PresentationName))
    {
        Result = TEXT("T04 presentation graph is missing.");
        return false;
    }
    // 仅修正本次迁移新建的五个注释框，原注释和所有执行/数据连线不变。
    struct FNote { const TCHAR* Graph; const TCHAR* Guid; const TCHAR* Text; FLinearColor Color; };
    const FNote Notes[] = {
        { TEXT("Start_fire"), TEXT("0186F364418CBBF3A1C2A489667D14B5"), TEXT("旧版与过渡节点保留，当前输入不调用此函数。新路径见 EventGraph 绿色区域。"), FLinearColor(0.35f,0.2f,0.08f) },
        { TEXT("End_fire"), TEXT("954F81FF4B6E6483ABFEDD99381FE4DD"), TEXT("旧停止流程保留对比，当前输入直接调用组件 Stop Firing。"), FLinearColor(0.35f,0.2f,0.08f) },
        { TEXT("Shoot_Once"), TEXT("EE2C53EF450E1B75BA7BD7A4FB6155FE"), TEXT("旧射线与扣血节点保留对比，当前 GAS 射击不调用此函数。"), FLinearColor(0.35f,0.2f,0.08f) },
        { TEXT("GAS_PlayFireEffects"), TEXT("7F61226C4A4C69038F00FCADA0815B86"), TEXT("GAS 单发表现：复用动画与枪口资源；射速、弹药、射线及伤害由 C++ 负责。"), FLinearColor(0.12f,0.35f,0.18f) },
        { TEXT("EventGraph"), TEXT("F1EBE1E64B7856C1A7EB71B89A7FE930"), TEXT("BeginPlay 绑定一次；每发回调进入 GAS_PlayFireEffects。命中参数已传入。"), FLinearColor(0.12f,0.28f,0.4f) }
    };
    for (const FNote& Note : Notes)
    {
        UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(FindNode(FindGraph(Blueprint, Note.Graph), Note.Guid));
        if (!Comment) { Result = TEXT("Expected migration comment is missing."); return false; }
        Comment->NodeComment = Note.Text;
        Comment->CommentColor = Note.Color;
        Comment->FontSize = 20;
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    if (!Compile(Blueprint, Result) || !SaveAsset(Blueprint)) { return false; }
    Result = TEXT("Chinese comparison comments saved; graph wiring unchanged.");
    return true;
}
