#include "ShooterAimWiringCommandlet.h"
#include "Characters/MyShooter.h"
#include "AI/ShooterAIController.h"
#include "Animation/ShooterPlayerAnimInstance.h"
#include "Weapons/ShooterAimComponent.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_Slot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Camera/CameraComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAimWiring, Log, All);
namespace
{
    template<class T> T* Asset(const TCHAR* Path)
    {
        return LoadObject<T>(nullptr, *(FString(Path) + TEXT(".") + FPackageName::GetShortName(Path)));
    }
    bool Save(UObject* Object)
    {
        UPackage* Package = Object->GetOutermost(); Package->MarkPackageDirty();
        FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Package, Object, *File, Args);
    }
    bool Compile(UBlueprint* BP)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
        return Log.NumErrors == 0 && BP->Status != BS_Error;
    }
    bool SetObject(UObject* Object, const TCHAR* Name, UObject* Value)
    {
        FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name);
        if (!Property) { return false; } Property->SetObjectPropertyValue_InContainer(Object, Value); return true;
    }
    UObject* GetObject(UObject* Object, const TCHAR* Name)
    {
        FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
        return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
    }
    UEdGraph* Graph(UBlueprint* BP, const TCHAR* Name)
    {
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for (UEdGraph* G : Graphs) { if (G->GetName() == Name) { return G; } } return nullptr;
    }
    bool Link(UEdGraph* G, UEdGraphNode* A, const TCHAR* Out, UEdGraphNode* B, const TCHAR* In)
    {
        return A && B && A->FindPin(Out) && B->FindPin(In) && G->GetSchema()->TryCreateConnection(A->FindPin(Out), B->FindPin(In));
    }
    UK2Node_CallFunction* Call(UEdGraph* G, UClass* Class, const TCHAR* Name, int X, int Y, bool bSelf=false)
    {
        UFunction* Function = Class->FindFunctionByName(Name); if (!Function) { return nullptr; }
        FGraphNodeCreator<UK2Node_CallFunction> Creator(*G); UK2Node_CallFunction* Node = Creator.CreateNode();
        Node->SetFromFunction(Function); if (bSelf) { Node->FunctionReference.SetSelfMember(Function->GetFName()); }
        Node->NodePosX=X; Node->NodePosY=Y; Creator.Finalize(); return Node;
    }
    void Inspect(UBlueprint* BP)
    {
        UE_LOG(LogShooterAimWiring, Display, TEXT("BLUEPRINT %s parent=%s"), *BP->GetName(), *BP->ParentClass->GetName());
        if (ACharacter* Character = Cast<ACharacter>(BP->GeneratedClass->GetDefaultObject()))
        {
            USkeletalMeshComponent* Mesh=Character->GetMesh();
            UE_LOG(LogShooterAimWiring, Display, TEXT("Character mesh=%s relative=%s anim=%s"), *GetNameSafe(Mesh->GetSkeletalMeshAsset()), *Mesh->GetRelativeTransform().ToString(), *GetNameSafe(Mesh->GetAnimClass()));
        }
        if (BP->SimpleConstructionScript)
        {
            for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
            {
                USceneComponent* Component = Cast<USceneComponent>(Node->ComponentTemplate);
                if (!Component) { continue; }
                UE_LOG(LogShooterAimWiring, Display, TEXT("COMPONENT %s type=%s parent=%s socket=%s transform=%s"), *Node->GetVariableName().ToString(), *Component->GetClass()->GetName(), *Node->ParentComponentOrVariableName.ToString(), *Node->AttachToName.ToString(), *Component->GetRelativeTransform().ToString());
                if (UCameraComponent* Camera=Cast<UCameraComponent>(Component)) { UE_LOG(LogShooterAimWiring, Display, TEXT("Camera FOV=%.2f"),Camera->FieldOfView); }
                if (USkeletalMeshComponent* Mesh=Cast<USkeletalMeshComponent>(Component))
                {
                    UE_LOG(LogShooterAimWiring, Display, TEXT("Weapon mesh=%s bounds=%s"), *GetNameSafe(Mesh->GetSkeletalMeshAsset()), Mesh->GetSkeletalMeshAsset() ? *Mesh->GetSkeletalMeshAsset()->GetBounds().ToString() : TEXT("None"));
                    TArray<FComponentSocketDescription> Sockets; Mesh->QuerySupportedSockets(Sockets);
                    for (const FComponentSocketDescription& Socket : Sockets) { UE_LOG(LogShooterAimWiring, Display, TEXT("SOCKET %s transform=%s"), *Socket.Name.ToString(), *Mesh->GetSocketTransform(Socket.Name, RTS_Component).ToString()); }
                }
            }
        }
    }
}
UShooterAimWiringCommandlet::UShooterAimWiringCommandlet()
{
    IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true;
}
int32 UShooterAimWiringCommandlet::Main(const FString& Params)
{
    UBlueprint* PlayerBP=Asset<UBlueprint>(TEXT("/Game/Blueprints/Shooter"));
    UBlueprint* EnemyBP=Asset<UBlueprint>(TEXT("/Game/Blueprints/Boot_Shooter_BP"));
    UBlueprint* AIBP=Asset<UBlueprint>(TEXT("/Game/Blueprints/Boot_Shooter_controller"));
    UAnimBlueprint* AnimBP=Asset<UAnimBlueprint>(TEXT("/Game/Blueprints/Shooter_idle"));
    if(!PlayerBP || !EnemyBP || !AIBP || !AnimBP) { return 1; }
    if(FParse::Param(*Params,TEXT("Inspect")))
    {
        Inspect(PlayerBP); Inspect(EnemyBP); Inspect(AIBP);
        for(const TCHAR* Path : {TEXT("/Game/Assets/Characters/FPP_Animations/FPP_RifleAim"), TEXT("/Game/Assets/Characters/FPP_Animations/FPP_RifleAimFire")})
        {
            UAnimSequence* Sequence=Asset<UAnimSequence>(Path); if(!Sequence) { return 2; }
            UE_LOG(LogShooterAimWiring, Display,TEXT("ANIMATION %s length=%.3f additive=%d skeleton=%s"), *Sequence->GetName(),Sequence->GetPlayLength(),int(Sequence->AdditiveAnimType),*GetNameSafe(Sequence->GetSkeleton()));
            for(const FAnimNotifyEvent& Notify : Sequence->Notifies) { UE_LOG(LogShooterAimWiring,Display,TEXT("NOTIFY %s class=%s"),*Notify.NotifyName.ToString(),*GetNameSafe(Notify.Notify)); }
        }
        return 0;
    }
    AMyShooter* Player=Cast<AMyShooter>(PlayerBP->GeneratedClass->GetDefaultObject());
    UClass* BulletClass=LoadClass<AShooterBulletVisual>(nullptr,TEXT("/Game/Weapons/BP_ShooterBulletVisual.BP_ShooterBulletVisual_C"));
    const TCHAR* AimSequencePath=TEXT("/Game/Animations/AS_PlayerAimFire");
    const TCHAR* AimMontagePath=TEXT("/Game/Animations/AM_PlayerAimFire");
    if(!Player || !Player->GetShooterAim() || !BulletClass) { return 3; }
    // 只更新已接入的玩家瞄准偏移；重复校准不会重建动画图或覆盖其他蓝图配置。
    if(FParse::Param(*Params,TEXT("Calibrate")))
    {
        FVector Offset;
        FString OffsetText;
        if(!FParse::Value(*Params,TEXT("Offset="),OffsetText) || !Offset.InitFromString(OffsetText)) { return 13; }
        FStructProperty* Property=FindFProperty<FStructProperty>(UShooterAimComponent::StaticClass(),TEXT("AimMeshOffset"));
        if(!Property || Offset.ContainsNaN()) { return 13; }
        *Property->ContainerPtrToValuePtr<FVector>(Player->GetShooterAim())=Offset;
        if(!Compile(PlayerBP) || !Save(PlayerBP)) { return 14; }
        UE_LOG(LogShooterAimWiring,Display,TEXT("Saved calibrated aim mesh offset: %s"),*Offset.ToString());
        return 0;
    }
    if(FParse::Param(*Params,TEXT("Verify")))
    {
        UAnimMontage* Montage=Asset<UAnimMontage>(AimMontagePath);
        UAnimSequence* CleanFire=Asset<UAnimSequence>(AimSequencePath);
        UEdGraph* AnimGraph=Graph(AnimBP,TEXT("AnimGraph"));
        int32 Blends=0; if(AnimGraph) { for(UEdGraphNode* N : AnimGraph->Nodes) { Blends+=N->IsA<UAnimGraphNode_TwoWayBlend>(); } }
        USCS_Node* Muzzle=EnemyBP->SimpleConstructionScript->FindSCSNode(TEXT("Muzzle"));
        USCS_Node* Rifle=EnemyBP->SimpleConstructionScript->FindSCSNode(TEXT("Rifle"));
        const bool bOK=Montage && CleanFire && CleanFire->Notifies.IsEmpty() && Montage->GetPlayLength()>0
            && AnimBP->ParentClass==UShooterPlayerAnimInstance::StaticClass() && Blends==1
            && GetObject(Player->GetShooterAim(),TEXT("AimFireMontage"))==Montage
            && GetObject(Player->GetShooterAim(),TEXT("HipFireMontage"))!=nullptr
            && GetObject(AIBP->GeneratedClass->GetDefaultObject(),TEXT("BulletVisualClass"))==BulletClass
            && Muzzle && Rifle && Rifle->GetChildNodes().Contains(Muzzle);
        UE_LOG(LogShooterAimWiring,Display,TEXT("T10 saved aim/bullet configuration: %s"),bOK?TEXT("PASS"):TEXT("FAIL"));
        return bOK?0:4;
    }
    if(FPackageName::DoesPackageExist(AimSequencePath) || FPackageName::DoesPackageExist(AimMontagePath)
        || AnimBP->ParentClass!=UAnimInstance::StaticClass())
    { UE_LOG(LogShooterAimWiring,Error,TEXT("Unexpected prior aim setup; use -Verify. No assets saved.")); return 5; }
    UEdGraph* AnimGraph=Graph(AnimBP,TEXT("AnimGraph"));
    UEdGraph* Effects=Graph(PlayerBP,TEXT("GAS_PlayFireEffects"));
    UAnimGraphNode_Slot* Slot=nullptr; UK2Node_CallFunction* FireNode=nullptr;
    if(AnimGraph) { for(UEdGraphNode* N:AnimGraph->Nodes) { if(UAnimGraphNode_Slot* S=Cast<UAnimGraphNode_Slot>(N)) { if(S->Node.SlotName==TEXT("DefaultSlot")) { Slot=S; break; } } } }
    if(Effects) { for(UEdGraphNode* N:Effects->Nodes) { if(UK2Node_CallFunction* C=Cast<UK2Node_CallFunction>(N)) { if(C->FunctionReference.GetMemberName()==TEXT("PlayAnimMontage")) { FireNode=C; break; } } } }
    UEdGraphPin* SlotSource=Slot?Slot->FindPin(TEXT("Source")):nullptr;
    UEdGraphPin* MontagePin=FireNode?FireNode->FindPin(TEXT("AnimMontage")):nullptr;
    UAnimMontage* HipMontage=MontagePin?Cast<UAnimMontage>(MontagePin->DefaultObject):nullptr;
    USCS_Node* PlayerMuzzle=PlayerBP->SimpleConstructionScript->FindSCSNode(TEXT("Muzzle"));
    USCS_Node* Rifle=EnemyBP->SimpleConstructionScript->FindSCSNode(TEXT("Rifle"));
    UAnimSequence* Aim=Asset<UAnimSequence>(TEXT("/Game/Assets/Characters/FPP_Animations/FPP_RifleAim"));
    UAnimSequence* AimFire=Asset<UAnimSequence>(TEXT("/Game/Assets/Characters/FPP_Animations/FPP_RifleAimFire"));
    if(!SlotSource || SlotSource->LinkedTo.Num()!=1 || !MontagePin || !MontagePin->LinkedTo.IsEmpty() || !HipMontage
        || !PlayerMuzzle || !Rifle || !Aim || !AimFire || Aim->GetSkeleton()!=HipMontage->GetSkeleton() || AimFire->GetSkeleton()!=Aim->GetSkeleton())
    { UE_LOG(LogShooterAimWiring,Error,TEXT("Inspected baseline differs: slot, fire pin, muzzle or skeleton. No assets saved.")); return 6; }
    // AimFire 源动画内有 PlaySound 通知。使用副本去掉通知，枪声仍由武器成功发射统一播放。
    UAnimSequence* CleanFire=DuplicateObject<UAnimSequence>(AimFire,CreatePackage(AimSequencePath),*FPackageName::GetShortName(AimSequencePath));
    CleanFire->SetFlags(RF_Public|RF_Standalone); CleanFire->ClearFlags(RF_Transient); CleanFire->Notifies.Reset(); CleanFire->PostEditChange();
    UAnimMontage* Dynamic=UAnimMontage::CreateSlotAnimationAsDynamicMontage(CleanFire,TEXT("DefaultSlot"),0.025f,0.05f,1.f,1);
    if(!Dynamic) { return 7; }
    UAnimMontage* Montage=DuplicateObject<UAnimMontage>(Dynamic,CreatePackage(AimMontagePath),*FPackageName::GetShortName(AimMontagePath));
    Montage->ClearFlags(RF_Transient); Montage->SetFlags(RF_Public|RF_Standalone);
    Montage->SetCompositeLength(CleanFire->GetPlayLength()); Montage->PostEditChange();
    // 原移动图接 A，已有 FPP 瞄准序列接 B；混合之后仍经过原 DefaultSlot。
    AnimBP->ParentClass=UShooterPlayerAnimInstance::StaticClass();
    if(!Compile(AnimBP)) { return 8; }
    SlotSource=Slot->FindPin(TEXT("Source"));
    if(!SlotSource || SlotSource->LinkedTo.Num()!=1) { return 8; }
    FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> BlendCreator(*AnimGraph); UAnimGraphNode_TwoWayBlend* Blend=BlendCreator.CreateNode();
    Blend->NodePosX=Slot->NodePosX-250; Blend->NodePosY=Slot->NodePosY+200;
    Blend->NodeComment=TEXT("T10：原移动姿势与已有瞄准姿势混合；权重由 C++ 开镜组件维护"); Blend->bCommentBubbleVisible=true; BlendCreator.Finalize();
    FGraphNodeCreator<UAnimGraphNode_SequencePlayer> SequenceCreator(*AnimGraph); UAnimGraphNode_SequencePlayer* SequenceNode=SequenceCreator.CreateNode();
    SequenceNode->Node.SetSequence(Aim); SequenceNode->Node.SetLoopAnimation(true);
    SequenceNode->NodePosX=Blend->NodePosX-300; SequenceNode->NodePosY=Blend->NodePosY+170; SequenceCreator.Finalize();
    FGraphNodeCreator<UK2Node_VariableGet> AlphaCreator(*AnimGraph); UK2Node_VariableGet* Alpha=AlphaCreator.CreateNode();
    Alpha->VariableReference.SetSelfMember(TEXT("ShooterAimAlpha")); Alpha->NodePosX=Blend->NodePosX-260; Alpha->NodePosY=Blend->NodePosY+380; AlphaCreator.Finalize();
    UEdGraphPin* Previous=SlotSource->LinkedTo[0]; SlotSource->BreakLinkTo(Previous);
    bool bLinked=AnimGraph->GetSchema()->TryCreateConnection(Previous,Blend->FindPinChecked(TEXT("A")));
    bLinked&=Link(AnimGraph,SequenceNode,TEXT("Pose"),Blend,TEXT("B"));
    bLinked&=Link(AnimGraph,Alpha,TEXT("ShooterAimAlpha"),Blend,TEXT("Alpha"));
    bLinked&=Link(AnimGraph,Blend,TEXT("Pose"),Slot,TEXT("Source"));
    if(!bLinked || !Compile(AnimBP)) { return 8; }
    // 保留原开火执行链和粒子节点，只把蒙太奇参数换成组件的姿势选择结果。
    UK2Node_CallFunction* GetAim=Call(Effects,AMyShooter::StaticClass(),TEXT("GetShooterAim"),FireNode->NodePosX-250,FireNode->NodePosY+410,true);
    UK2Node_CallFunction* GetMontage=Call(Effects,UShooterAimComponent::StaticClass(),TEXT("GetFireMontage"),FireNode->NodePosX,FireNode->NodePosY+410);
    if(!Link(Effects,GetAim,TEXT("ReturnValue"),GetMontage,TEXT("self")) || !Link(Effects,GetMontage,TEXT("ReturnValue"),FireNode,TEXT("AnimMontage"))) { return 9; }
    FireNode->NodeComment=TEXT("T10：普通/瞄准开火蒙太奇由 ShooterAim 选择；仍由成功发射事件触发一次");
    if(!SetObject(Player->GetShooterAim(),TEXT("HipFireMontage"),HipMontage) || !SetObject(Player->GetShooterAim(),TEXT("AimFireMontage"),Montage)
        || !SetObject(AIBP->GeneratedClass->GetDefaultObject(),TEXT("BulletVisualClass"),BulletClass)) { return 10; }
    USCS_Node* EnemyMuzzle=EnemyBP->SimpleConstructionScript->FindSCSNode(TEXT("Muzzle"));
    if(!EnemyMuzzle)
    {
        EnemyMuzzle=EnemyBP->SimpleConstructionScript->CreateNode(UArrowComponent::StaticClass(),TEXT("Muzzle"));
        Rifle->AddChildNode(EnemyMuzzle);
        // 同一 Rifle 模型复用玩家已校准的枪口局部位置；不改敌人手部挂点或武器姿态。
        USceneComponent* Original=CastChecked<USceneComponent>(PlayerMuzzle->ComponentTemplate);
        UArrowComponent* NewMuzzle=CastChecked<UArrowComponent>(EnemyMuzzle->ComponentTemplate);
        NewMuzzle->SetRelativeTransform(Original->GetRelativeTransform()); NewMuzzle->SetHiddenInGame(true);
    }
    if(!Compile(PlayerBP) || !Compile(EnemyBP) || !Compile(AIBP)) { return 11; }
    for(UObject* Object:TArray<UObject*>{CleanFire,Montage,AnimBP,PlayerBP,EnemyBP,AIBP}) { if(!Save(Object)) { return 12; } }
    UE_LOG(LogShooterAimWiring,Display,TEXT("T10 saved: existing FPP aim pose, toggle ADS, aim fire without duplicate sound, AI muzzle/bullet; old graph nodes retained."));
    return 0;
}
