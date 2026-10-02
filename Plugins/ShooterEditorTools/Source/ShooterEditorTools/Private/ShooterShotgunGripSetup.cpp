#include "ShooterShotgunGripSetup.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Animation/ShooterPlayerAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_TwoBoneIK.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "K2Node_VariableGet.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
DEFINE_LOG_CATEGORY_STATIC(LogShotgunGrip,Log,All);
namespace
{
    bool SaveGripAsset(UObject* Object)
    {
        auto* Package=Object->GetOutermost(); Package->MarkPackageDirty();
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        return UPackage::SavePackage(Package,Object,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args);
    }
    template<class T> T* AddNode(UEdGraph* Graph,int X,int Y)
    {
        FGraphNodeCreator<T> Creator(*Graph); T* Node=Creator.CreateNode();
        Node->NodePosX=X; Node->NodePosY=Y; Creator.Finalize(); return Node;
    }
    bool Connect(UEdGraph* Graph,UEdGraphNode* A,const TCHAR* Out,UEdGraphNode* B,const TCHAR* In)
    {
        return A&&B&&A->FindPin(Out)&&B->FindPin(In)&&Graph->GetSchema()->TryCreateConnection(A->FindPin(Out),B->FindPin(In));
    }
    UK2Node_VariableGet* ReadValue(UEdGraph* Graph,const TCHAR* Name,int X,int Y)
    {
        FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph); auto* Node=Creator.CreateNode();
        Node->VariableReference.SetSelfMember(Name); Node->NodePosX=X; Node->NodePosY=Y; Creator.Finalize(); return Node;
    }
}
int32 SetupShooterShotgunGrip(UShooterWeaponDefinition* Definition,UAnimSequence* Reference,const FString& Params)
{
    auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Blueprints/Shooter_idle.Shooter_idle"));
    if(!BP||!Definition||!Reference||BP->ParentClass!=UShooterPlayerAnimInstance::StaticClass()) { return 20; }
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); UEdGraph* Graph=nullptr;
    for(auto* G:Graphs) { if(G->GetName()==TEXT("AnimGraph")) { Graph=G; break; } }
    if(!Graph) { return 21; }
    UAnimGraphNode_Root* Root=nullptr; UAnimGraphNode_TwoBoneIK* Existing=nullptr;
    TSet<FGuid> OldNodes;
    for(UEdGraphNode* N:Graph->Nodes)
    {
        OldNodes.Add(N->NodeGuid);
        if(auto* R=Cast<UAnimGraphNode_Root>(N)) { Root=R; }
        if(auto* IK=Cast<UAnimGraphNode_TwoBoneIK>(N)) { if(IK->Node.IKBone.BoneName==TEXT("b_LeftHand")) { Existing=IK; } }
    }
    if(FParse::Param(*Params,TEXT("GripVerify")))
    {
        const bool bOK=Existing&&Definition->bUseLeftHandIK&&Existing->Node.EffectorTarget.BoneReference.BoneName==TEXT("b_RightWeapon")
            &&Existing->FindPin(TEXT("Alpha"))&&!Existing->FindPin(TEXT("Alpha"))->LinkedTo.IsEmpty();
        UE_LOG(LogShotgunGrip,Display,TEXT("GRIP saved graph %s target=%s elbow=%s"),bOK?TEXT("PASS"):TEXT("FAIL"),*Definition->LeftHandGripLocation.ToString(),*Definition->LeftElbowHint.ToString());
        return bOK?0:22;
    }
    if(!Root||!Root->FindPin(TEXT("Result"))||Root->FindPin(TEXT("Result"))->LinkedTo.Num()!=1) { return 23; }
    if(!Existing)
    {
        auto* Input=Root->FindPin(TEXT("Result")); auto* Previous=Input->LinkedTo[0];
        const int X=Root->NodePosX,Y=Root->NodePosY;
        auto* ToComponent=AddNode<UAnimGraphNode_LocalToComponentSpace>(Graph,X,Y);
        FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> Creator(*Graph); auto* IK=Creator.CreateNode();
        IK->Node.IKBone.BoneName=TEXT("b_LeftHand");
        IK->Node.EffectorLocationSpace=BCS_BoneSpace; IK->Node.JointTargetLocationSpace=BCS_BoneSpace;
        IK->Node.EffectorTarget=FBoneSocketTarget(TEXT("b_RightWeapon"));
        IK->Node.JointTarget=FBoneSocketTarget(TEXT("b_RightWeapon"));
        IK->Node.bAllowStretching=false; IK->Node.bTakeRotationFromEffectorSpace=false; IK->Node.bMaintainEffectorRelRot=false;
        IK->NodePosX=X+220; IK->NodePosY=Y;
        IK->NodeComment=TEXT("霰弹枪左手握持：仅存活且非换弹时启用，原步枪/换弹/死亡通过 Alpha=0 保留");
        IK->bCommentBubbleVisible=true; Creator.Finalize();
        auto* ToLocal=AddNode<UAnimGraphNode_ComponentToLocalSpace>(Graph,X+500,Y); Root->NodePosX=X+720;
        auto* Alpha=ReadValue(Graph,TEXT("ShooterGripAlpha"),X-160,Y+200);
        auto* Grip=ReadValue(Graph,TEXT("ShooterGripTarget"),X-160,Y+290);
        auto* Elbow=ReadValue(Graph,TEXT("ShooterElbowTarget"),X-160,Y+380);
        Input->BreakLinkTo(Previous);
        bool bOK=Graph->GetSchema()->TryCreateConnection(Previous,ToComponent->FindPinChecked(TEXT("LocalPose")));
        bOK&=Connect(Graph,ToComponent,TEXT("ComponentPose"),IK,TEXT("ComponentPose"));
        bOK&=Connect(Graph,IK,TEXT("Pose"),ToLocal,TEXT("ComponentPose"));
        bOK&=Connect(Graph,ToLocal,TEXT("Pose"),Root,TEXT("Result"));
        bOK&=Connect(Graph,Alpha,TEXT("ShooterGripAlpha"),IK,TEXT("Alpha"));
        bOK&=Connect(Graph,Grip,TEXT("ShooterGripTarget"),IK,TEXT("EffectorLocation"));
        bOK&=Connect(Graph,Elbow,TEXT("ShooterElbowTarget"),IK,TEXT("JointTargetLocation"));
        if(!bOK) { UE_LOG(LogShotgunGrip,Error,TEXT("Grip graph pin connection failed, no asset saved.")); return 24; }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
        if(Log.NumErrors||BP->Status==BS_Error) { return 25; }
        for(const FGuid& Guid:OldNodes)
        {
            bool bFound=false; for(UEdGraphNode* N:Graph->Nodes) { bFound|=N->NodeGuid==Guid; }
            if(!bFound) { UE_LOG(LogShotgunGrip,Error,TEXT("An original animation node was lost.")); return 26; }
        }
        if(!SaveGripAsset(BP)) { return 27; }
        UE_LOG(LogShotgunGrip,Display,TEXT("Original graph nodes retained: %d; added %d grip nodes; blueprint compiled."),OldNodes.Num(),Graph->Nodes.Num()-OldNodes.Num());
    }
    // 从原姿势求支撑点，偏移只作用于左手，右手及整枪挂载不动。
    const auto& Ref=Reference->GetSkeleton()->GetReferenceSkeleton(); TArray<FTransform> CS;
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        TArray<FTransform> Keys; const FName Name=Ref.GetBoneName(I);
        if(Reference->GetDataModel()->IsValidBoneTrackName(Name)) { Reference->GetDataModel()->GetBoneTrackTransforms(Name,Keys); }
        const FTransform Local=Keys.IsEmpty()?Ref.GetRefBonePose()[I]:Keys[0]; const int32 Parent=Ref.GetParentIndex(I);
        CS.Add(Parent==INDEX_NONE?Local:Local*CS[Parent]);
    }
    const auto* Socket=Reference->GetSkeleton()->FindSocket(TEXT("Right_Weapon")); const int32 Hand=Ref.FindBoneIndex(TEXT("b_LeftHand"));
    if(!Socket||Hand==INDEX_NONE) { return 28; }
    const FTransform Weapon=Socket->GetSocketLocalTransform()*CS[Ref.FindBoneIndex(Socket->BoneName)];
    float DX=0,DY=-6,DZ=4; FParse::Value(*Params,TEXT("DX="),DX); FParse::Value(*Params,TEXT("DY="),DY); FParse::Value(*Params,TEXT("DZ="),DZ);
    Definition->bUseLeftHandIK=true;
    const FVector Base=Weapon.InverseTransformPosition(CS[Hand].GetTranslation());
    Definition->LeftHandGripLocation=Base+FVector(DX,DY,DZ);
    Definition->LeftElbowHint=Weapon.InverseTransformPosition(CS[Ref.GetParentIndex(Hand)].GetTranslation());
    UE_LOG(LogShotgunGrip,Display,TEXT("GRIP base=%s target=%s elbow=%s"),*Base.ToString(),*Definition->LeftHandGripLocation.ToString(),*Definition->LeftElbowHint.ToString());
    return SaveGripAsset(Definition)?0:29;
}
