#include "ShooterCombatPresentationCommandlet.h"
#include "AI/ShooterAIController.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AnimGraphNode_Slot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"
#include "Factories/SoundFactory.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimData/IAnimationDataModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterPresentation, Log, All);
namespace
{
    constexpr const TCHAR* SequencePath = TEXT("/Game/Animations/AS_AI_RifleRecoil");
    constexpr const TCHAR* MontagePath = TEXT("/Game/Animations/AM_AI_RifleFire");
    constexpr const TCHAR* SoundPath = TEXT("/Game/Audio/SW_RifleShot");
    constexpr const TCHAR* AttenuationPath = TEXT("/Game/Audio/SA_RifleShot");
    const FName AttackSlot(TEXT("DefaultSlot"));

    template<class T> T* Asset(const TCHAR* Path)
    {
        return LoadObject<T>(nullptr, *(FString(Path) + TEXT(".") + FPackageName::GetShortName(Path)));
    }
    template<class T> T* NewAsset(const TCHAR* Path)
    {
        return NewObject<T>(CreatePackage(Path), *FPackageName::GetShortName(Path), RF_Public | RF_Standalone);
    }
    bool Save(UObject* Object)
    {
        if (!Object) { return false; }
        UPackage* Package = Object->GetOutermost(); Package->MarkPackageDirty();
        FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Package, Object, *File, Args);
    }
    bool Compile(UBlueprint* BP)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
        return Log.NumErrors == 0 && BP->Status != BS_Error;
    }
    bool SetObject(UObject* Target, const TCHAR* Name, UObject* Value)
    {
        FObjectPropertyBase* Property = Target ? FindFProperty<FObjectPropertyBase>(Target->GetClass(), Name) : nullptr;
        if (!Property) { return false; }
        Property->SetObjectPropertyValue_InContainer(Target, Value); return true;
    }
    UObject* GetObject(UObject* Target, const TCHAR* Name)
    {
        FObjectPropertyBase* Property = Target ? FindFProperty<FObjectPropertyBase>(Target->GetClass(), Name) : nullptr;
        return Property ? Property->GetObjectPropertyValue_InContainer(Target) : nullptr;
    }
    UEdGraph* AliveGraph(UBlueprint* BP)
    {
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs) { if (Graph->GetName() == TEXT("Alive")) { return Graph; } }
        return nullptr;
    }
    bool HasSlot(UEdGraph* Graph)
    {
        if (!Graph) { return false; }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node))
            {
                if (Slot->Node.SlotName == AttackSlot && Slot->FindPin(TEXT("Source"))
                    && Slot->FindPin(TEXT("Source"))->LinkedTo.Num() == 1
                    && Slot->FindPin(TEXT("Pose"))->LinkedTo.Num() == 1) { return true; }
            }
        }
        return false;
    }
    bool AddSlot(UEdGraph* Graph)
    {
        if (!Graph) { return false; }
        if (HasSlot(Graph)) { return true; }
        UEdGraphNode* Result = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node->GetClass()->GetFName() == TEXT("AnimGraphNode_StateResult")) { Result = Node; break; }
        }
        UEdGraphPin* Input = Result ? Result->FindPin(TEXT("Result")) : nullptr;
        if (!Input || Input->LinkedTo.Num() != 1) { return false; }
        UEdGraphPin* Previous = Input->LinkedTo[0];
        FGraphNodeCreator<UAnimGraphNode_Slot> Creator(*Graph);
        UAnimGraphNode_Slot* Slot = Creator.CreateNode();
        Slot->Node.SlotName = AttackSlot;
        Slot->Node.bAlwaysUpdateSourcePose = true;
        Slot->NodePosX = Result->NodePosX - 240; Slot->NodePosY = Result->NodePosY + 180;
        Slot->NodeComment = TEXT("T09 AI 开火叠加层：保留原移动姿势，仅 Alive 分支播放；死亡分支保持原样");
        Slot->bCommentBubbleVisible = true;
        Creator.Finalize();
        const UEdGraphSchema* Schema = Graph->GetSchema();
        Input->BreakLinkTo(Previous);
        return Schema->TryCreateConnection(Previous, Slot->FindPinChecked(TEXT("Source")))
            && Schema->TryCreateConnection(Slot->FindPinChecked(TEXT("Pose")), Input);
    }

    // 复用用户已有的第三人称瞄准姿势：小幅抬枪后回位，不修改源资源。
    UAnimSequence* CreateRecoil(USkeleton* Skeleton)
    {
        UAnimSequence* Forward = Asset<UAnimSequence>(TEXT("/Game/Assets/Characters/TTP_Animations/AimOffsetFwd"));
        UAnimSequence* Up = Asset<UAnimSequence>(TEXT("/Game/Assets/Characters/TTP_Animations/AimOffsetUp"));
        if (!Forward || !Up || Forward->GetSkeleton() != Skeleton || Up->GetSkeleton() != Skeleton
            || !Forward->GetDataModel() || !Up->GetDataModel()) { return nullptr; }
        const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
        const int32 Spine = Ref.FindBoneIndex(TEXT("b_Spine"));
        if (Spine == INDEX_NONE) { return nullptr; }
        UAnimSequence* Sequence = NewAsset<UAnimSequence>(SequencePath);
        Sequence->SetSkeleton(Skeleton);
        Sequence->AdditiveAnimType = AAT_LocalSpaceBase;
        Sequence->RefPoseType = ABPT_AnimFrame;
        Sequence->RefPoseSeq = Forward;
        Sequence->RefFrameIndex = 0;
        IAnimationDataController& Controller = Sequence->GetController();
        Controller.InitializeModel();
        Controller.OpenBracket(FText::FromString(TEXT("从现有第三人称瞄准姿势制作单发后坐力")), false);
        Controller.SetFrameRate(FFrameRate(60,1), false);
        Controller.SetNumberOfFrames(FFrameNumber(18), false);
        for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
        {
            const FName BoneName = Ref.GetBoneName(Bone);
            TArray<FTransform> ForwardKeys, UpKeys;
            if (Forward->GetDataModel()->IsValidBoneTrackName(BoneName)) { Forward->GetDataModel()->GetBoneTrackTransforms(BoneName, ForwardKeys); }
            if (Up->GetDataModel()->IsValidBoneTrackName(BoneName)) { Up->GetDataModel()->GetBoneTrackTransforms(BoneName, UpKeys); }
            const FTransform Base = ForwardKeys.IsEmpty() ? Ref.GetRefBonePose()[Bone] : ForwardKeys[0];
            const FTransform Raised = UpKeys.IsEmpty() ? Base : UpKeys[0];
            bool bUpperBody = false;
            for (int32 Parent = Bone; Parent != INDEX_NONE; Parent = Ref.GetParentIndex(Parent))
            {
                if (Parent == Spine) { bUpperBody = true; break; }
            }
            TArray<FVector3f> Positions, Scales; TArray<FQuat4f> Rotations;
            for (int32 Frame = 0; Frame <= 18; ++Frame)
            {
                const float Time = Frame / 60.0f;
                const float Envelope = Time <= 0.05f ? Time / 0.05f : FMath::Square(FMath::Max(0.f, 1.f - (Time - 0.05f) / 0.25f));
                FTransform Pose = Base;
                // 只取向上瞄准的 12%，下半身保持零叠加，不改变原移动和站立姿势。
                if (bUpperBody) { Pose.Blend(Base, Raised, Envelope * 0.12f); }
                Positions.Add(FVector3f(Pose.GetTranslation())); Rotations.Add(FQuat4f(Pose.GetRotation())); Scales.Add(FVector3f(Pose.GetScale3D()));
            }
            if (!Controller.AddBoneCurve(BoneName, false)
                || !Controller.SetBoneTrackKeys(BoneName, Positions, Rotations, Scales, false)) { return nullptr; }
        }
        Controller.NotifyPopulated(); Controller.CloseBracket(false);
        Sequence->PostEditChange();
        return Sequence;
    }
}

UShooterCombatPresentationCommandlet::UShooterCombatPresentationCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UShooterCombatPresentationCommandlet::Main(const FString& Params)
{
    // -nosound 的命令行不会自动初始化音频设备，但导入/校验仍需要注册 Bink 解码器。
    FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("BinkAudioDecoder"));
    if (FParse::Param(*Params, TEXT("Inspect")))
    {
        FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        Module.Get().SearchAllAssets(true);
        FARFilter Filter; Filter.PackagePaths.Add(TEXT("/Game")); Filter.bRecursivePaths = true;
        Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
        Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
        Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
        TArray<FAssetData> Assets; Module.Get().GetAssets(Filter, Assets);
        for (const FAssetData& Data : Assets)
        {
            if (UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(Data.GetAsset()))
            {
                UE_LOG(LogShooterPresentation, Display, TEXT("ANIMATION %s skeleton=%s length=%.3f notifies=%d"),
                    *Anim->GetPathName(), *GetNameSafe(Anim->GetSkeleton()), Anim->GetPlayLength(), Anim->Notifies.Num());
            }
            else { UE_LOG(LogShooterPresentation, Display, TEXT("AUDIO %s"), *Data.GetObjectPathString()); }
        }
        return 0;
    }
    UBlueprint* PlayerBP = Asset<UBlueprint>(TEXT("/Game/Blueprints/Shooter"));
    UBlueprint* ControllerBP = Asset<UBlueprint>(TEXT("/Game/Blueprints/Boot_Shooter_controller"));
    UBlueprint* EnemyBP = Asset<UBlueprint>(TEXT("/Game/Blueprints/Boot_Shooter_BP"));
    UBlueprint* AnimBP = Asset<UBlueprint>(TEXT("/Game/Blueprints/Boot_Shooter_AnimationBP"));
    if (!PlayerBP || !ControllerBP || !EnemyBP || !AnimBP) { return 1; }
    AShooterCharacterBase* Enemy = Cast<AShooterCharacterBase>(EnemyBP->GeneratedClass->GetDefaultObject());
    AMyShooter* Player = Cast<AMyShooter>(PlayerBP->GeneratedClass->GetDefaultObject());
    USkeleton* Skeleton = Enemy && Enemy->GetMesh()->GetSkeletalMeshAsset() ? Enemy->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
    if (!Skeleton || !Player || !AliveGraph(AnimBP)) { UE_LOG(LogShooterPresentation, Error, TEXT("Unexpected enemy skeleton or Alive graph.")); return 2; }
    if (FParse::Param(*Params, TEXT("Verify")))
    {
        UAnimMontage* Montage = Asset<UAnimMontage>(MontagePath);
        USoundWave* Sound = Asset<USoundWave>(SoundPath);
        UAnimSequence* Sequence = Asset<UAnimSequence>(SequencePath);
        UObject* Defaults = ControllerBP->GeneratedClass->GetDefaultObject();
        TArray<uint8> PCM; uint32 SampleRate = 0; uint16 Channels = 0;
        const bool bAudioDataOK = Sound && Sound->GetImportedSoundWaveData(PCM, SampleRate, Channels)
            && PCM.Num() > 0 && SampleRate == 48000 && Channels == 1
            && Sound->GetCompressedDataSize(Sound->GetRuntimeFormat()) > 0;
        UE_LOG(LogShooterPresentation, Display, TEXT("Audio source and runtime compression: %s; bytes=%d rate=%u channels=%u"),
            bAudioDataOK ? TEXT("PASS") : TEXT("FAIL"), PCM.Num(), SampleRate, Channels);
        const bool bOK = bAudioDataOK && Montage && Sequence && Sound && !Sound->bLooping && Sound->Duration > 0
            && Montage->GetSkeleton() == Skeleton && Montage->GetPlayLength() > 0 && Sequence->IsValidAdditive()
            && HasSlot(AliveGraph(AnimBP)) && GetObject(Defaults, TEXT("AttackMontage")) == Montage
            && GetObject(Defaults, TEXT("AttackSound")) == Sound
            && GetObject(Defaults, TEXT("AttackSoundAttenuation")) == Asset<USoundAttenuation>(AttenuationPath)
            && GetObject(Player->GetShooterWeapon(), TEXT("FireSound")) == Sound;
        UE_LOG(LogShooterPresentation, Display, TEXT("T09 persisted assets: %s; skeleton=%s montage=%.3fs sound=%.3fs"),
            bOK ? TEXT("PASS") : TEXT("FAIL"), *GetNameSafe(Skeleton), Montage ? Montage->GetPlayLength() : 0, Sound ? Sound->Duration : 0);
        return bOK ? 0 : 3;
    }
    for (const TCHAR* Path : {SequencePath, MontagePath, SoundPath, AttenuationPath})
    {
        if (FPackageName::DoesPackageExist(Path)) { UE_LOG(LogShooterPresentation, Error, TEXT("Asset exists; use -Verify: %s"), Path); return 4; }
    }
    UAnimSequence* Sequence = CreateRecoil(Skeleton);
    UAnimMontage* Dynamic = Sequence ? UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, AttackSlot, 0.015f, 0.04f, 1.0f, 1) : nullptr;
    if (!Dynamic) { return 5; }
    UAnimMontage* Montage = DuplicateObject<UAnimMontage>(Dynamic, CreatePackage(MontagePath), *FPackageName::GetShortName(MontagePath));
    Montage->ClearFlags(RF_Transient); Montage->SetFlags(RF_Public | RF_Standalone);
    Montage->SetCompositeLength(Sequence->GetPlayLength()); Montage->PostEditChange();
    // 原创合成 WAV 只作为基础枪声，保持标准 SoundWave，后续可以直接换成用户选定的录音。
    USoundFactory* Factory = NewObject<USoundFactory>(); Factory->bAutoCreateCue = false;
    const FString Wav = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("SourceArt/Audio/RifleShot.wav"));
    USoundWave* Sound = Cast<USoundWave>(UFactory::StaticImportObject(USoundWave::StaticClass(), CreatePackage(SoundPath),
        *FPackageName::GetShortName(SoundPath), RF_Public | RF_Standalone, *Wav, nullptr, Factory));
    if (!Sound) { return 6; }
    Sound->bLooping = false;
    USoundAttenuation* Attenuation = NewAsset<USoundAttenuation>(AttenuationPath);
    Attenuation->Attenuation.bAttenuate = true;
    Attenuation->Attenuation.bSpatialize = true;
    Attenuation->Attenuation.AttenuationShapeExtents = FVector(150,0,0);
    Attenuation->Attenuation.FalloffDistance = 2200.f;
    if (!AddSlot(AliveGraph(AnimBP)) || !Compile(AnimBP)) { UE_LOG(LogShooterPresentation, Error, TEXT("Could not connect AI Alive slot.")); return 7; }
    if (!SetObject(ControllerBP->GeneratedClass->GetDefaultObject(), TEXT("AttackMontage"), Montage)
        || !SetObject(ControllerBP->GeneratedClass->GetDefaultObject(), TEXT("AttackSound"), Sound)
        || !SetObject(ControllerBP->GeneratedClass->GetDefaultObject(), TEXT("AttackSoundAttenuation"), Attenuation)
        || !SetObject(Player->GetShooterWeapon(), TEXT("FireSound"), Sound)) { return 8; }
    if (!Compile(ControllerBP) || !Compile(PlayerBP)) { return 9; }
    for (UObject* Object : TArray<UObject*>{Sequence, Montage, Sound, Attenuation, AnimBP, ControllerBP, PlayerBP})
    {
        if (!Save(Object)) { return 10; }
    }
    UE_LOG(LogShooterPresentation, Display, TEXT("T09 saved: additive AI recoil + Alive Slot + spatial AI sound + single-shot player sound. Old nodes retained."));
    return 0;
}
