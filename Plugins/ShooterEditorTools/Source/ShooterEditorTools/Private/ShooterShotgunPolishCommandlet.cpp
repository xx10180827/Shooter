#include "ShooterShotgunPolishCommandlet.h"
#include "ShooterShotgunGripSetup.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Sound/SoundWave.h"
#include "Factories/SoundFactory.h"
#include "Audio.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogShotgunPolish,Log,All);
namespace
{
    template<class T> T* Load(const FString& Path) { return LoadObject<T>(nullptr,*(Path+TEXT(".")+FPackageName::GetShortName(Path))); }
    bool Save(UObject* Object)
    {
        if(!Object) { return false; }
        UPackage* Package=Object->GetOutermost(); Package->MarkPackageDirty();
        const FString File=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        return UPackage::SavePackage(Package,Object,*File,Args);
    }
    FTransform Sample(const TArray<FTransform>& Keys,float Alpha,const FTransform& Fallback)
    {
        if(Keys.IsEmpty()) { return Fallback; }
        const float Frame=FMath::Clamp(Alpha,0.f,1.f)*(Keys.Num()-1);
        const int32 A=FMath::FloorToInt(Frame),B=FMath::Min(A+1,Keys.Num()-1);
        FTransform Pose; Pose.Blend(Keys[A],Keys[B],Frame-A); return Pose;
    }
    // 第一人称源动画为完整姿势。重采样所有原骨骼轨道，不改源资源与角色骨架。
    UAnimSequence* MakeSequence(UAnimSequence* Source,const FString& Path,float Duration,float Strength,bool bReload)
    {
        if(!Source||!Source->GetDataModel()||Source->AdditiveAnimType!=AAT_None||FPackageName::DoesPackageExist(Path)) { return nullptr; }
        auto* Sequence=NewObject<UAnimSequence>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone);
        Sequence->SetSkeleton(Source->GetSkeleton());
        auto& Controller=Sequence->GetController(); Controller.InitializeModel();
        Controller.OpenBracket(FText::FromString(TEXT("霰弹枪动作节奏与后坐力调整")),false);
        const int32 Frames=FMath::RoundToInt(Duration*60);
        Controller.SetFrameRate(FFrameRate(60,1),false); Controller.SetNumberOfFrames(FFrameNumber(Frames),false);
        const auto& Ref=Source->GetSkeleton()->GetReferenceSkeleton();
        for(int32 Bone=0;Bone<Ref.GetNum();++Bone)
        {
            const FName Name=Ref.GetBoneName(Bone); TArray<FTransform> Keys;
            if(Source->GetDataModel()->IsValidBoneTrackName(Name)) { Source->GetDataModel()->GetBoneTrackTransforms(Name,Keys); }
            const FTransform Base=Sample(Keys,0,Ref.GetRefBonePose()[Bone]);
            TArray<FVector3f> Positions,Scales; TArray<FQuat4f> Rotations;
            for(int32 Frame=0;Frame<=Frames;++Frame)
            {
                const float T=Frame/static_cast<float>(Frames);
                // 开火快速抬枪、较慢回位；换弹分成准备、操作、压紧、回位四段。
                float Alpha;
                if(bReload)
                {
                    Alpha=T<.2f?T:T<.7f?.2f+(T-.2f)*1.16f:T<.9f?.78f+(T-.7f)*.85f:.95f+(T-.9f)*.5f;
                }
                else { Alpha=T<.12f?T/.12f*.3f:.3f+(T-.12f)/.88f*.7f; }
                const FTransform Pose=Sample(Keys,Alpha,Base);
                const FVector Position=Base.GetTranslation()+(Pose.GetTranslation()-Base.GetTranslation())*Strength;
                const FQuat Rotation=FQuat::Slerp(Base.GetRotation(),Pose.GetRotation(),Strength).GetNormalized();
                Positions.Add(FVector3f(Position)); Rotations.Add(FQuat4f(Rotation)); Scales.Add(FVector3f(Pose.GetScale3D()));
            }
            if(!Controller.AddBoneCurve(Name,false)||!Controller.SetBoneTrackKeys(Name,Positions,Rotations,Scales,false)) { return nullptr; }
        }
        Controller.NotifyPopulated(); Controller.CloseBracket(false);
        // 枪声与弹药由现有成功发射/换弹事件负责，不复制源动画的音频或玩法通知。
        Sequence->Notifies.Reset(); Sequence->PostEditChange(); FAssetRegistryModule::AssetCreated(Sequence);
        UE_LOG(LogShotgunPolish,Display,TEXT("SEQUENCE %s source=%s duration=%.3f strength=%.2f bones=%d"),*Path,*Source->GetName(),Sequence->GetPlayLength(),Strength,Ref.GetNum());
        return Sequence;
    }
    UAnimMontage* MakeMontage(UAnimSequence* Sequence,const FString& Path,bool bReload)
    {
        if(!Sequence||FPackageName::DoesPackageExist(Path)) { return nullptr; }
        auto* Dynamic=UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence,TEXT("DefaultSlot"),bReload?.10f:.012f,bReload?.10f:.08f,1.f,1);
        if(!Dynamic) { return nullptr; }
        auto* Montage=DuplicateObject<UAnimMontage>(Dynamic,CreatePackage(*Path),*FPackageName::GetShortName(Path));
        Montage->ClearFlags(RF_Transient); Montage->SetFlags(RF_Public|RF_Standalone); Montage->SetCompositeLength(Sequence->GetPlayLength());
        Montage->Notifies.Reset(); Montage->PostEditChange(); FAssetRegistryModule::AssetCreated(Montage); return Montage;
    }
    USoundWave* MakeSound(const FString& Name,const TArray<int16>& Source,bool bReload)
    {
        const FString Path=TEXT("/Game/Audio/Shotgun/SW_")+Name;
        if(FPackageName::DoesPackageExist(Path)) { return nullptr; }
        constexpr int32 Rate=48000;
        const float Duration=bReload?2.4f:.58f;
        TArray<float> Samples; Samples.SetNumZeroed(FMath::RoundToInt(Duration*Rate));
        FRandomStream Random(bReload?50421:50420); float LowNoise=0,Peak=0;
        for(int32 I=0;I<Samples.Num();++I)
        {
            const float T=I/static_cast<float>(Rate),Noise=Random.FRandRange(-1.f,1.f);
            LowNoise=FMath::Lerp(LowNoise,Noise,.12f);
            float Value=0;
            if(!bReload)
            {
                // 复用已有原创步枪瞬态，降低播放频率，再叠加衰减低频和短促气流层。
                const int32 Index=FMath::FloorToInt(I*.72f);
                const float Original=Source.IsValidIndex(Index)?Source[Index]/32768.f:0.f;
                Value=.7f*Original+.28f*FMath::Sin(2*PI*(85*T-22*T*T))*FMath::Exp(-13*T)
                    +.3f*LowNoise*FMath::Exp(-22*T)+.16f*Noise*FMath::Exp(-180*T);
            }
            else
            {
                if(Name==TEXT("ShotgunWholeReload"))
                {
                    // 整段换弹：一次取出摩擦、一次插入闷响、一次结束锁定；避免连续颗粒点击误导逐颗装填。
                    if(T>.32f&&T<.92f) { Value=.16f*LowNoise*FMath::Sin(PI*(T-.32f)/.6f); }
                    const float Insert=T-1.48f;
                    if(Insert>=0&&Insert<.20f) { Value+=(.23f*LowNoise+.20f*FMath::Sin(2*PI*110*Insert))*FMath::Exp(-28*Insert); }
                    const float Lock=T-2.03f;
                    if(Lock>=0&&Lock<.15f) { Value+=(.14f*Noise+.1f*FMath::Sin(2*PI*820*Lock))*FMath::Exp(-45*Lock); }
                }
                else
                {
                // 保留第一版声音生成路径，便于对比。
                const float Times[]={.10f,.77f,1.18f,1.85f,2.08f};
                for(int32 Click=0;Click<5;++Click)
                {
                    const float Local=T-Times[Click];
                    if(Local>=0&&Local<.18f)
                    { Value+=(.35f*Noise+.14f*FMath::Sin(2*PI*(650+Click*130)*Local))*FMath::Exp(-55*Local); }
                }
                if(T>.45f&&T<1.65f) { Value+=.055f*LowNoise*FMath::Sin(PI*(T-.45f)/1.2f); }
                }
            }
            // 首尾淡入淡出并归一化，输出标准 48 kHz / mono / PCM16 原创 WAV。
            Value*=FMath::Min(1.f,T/.001f)*FMath::Min(1.f,(Duration-T)/.03f);
            Samples[I]=Value; Peak=FMath::Max(Peak,FMath::Abs(Value));
        }
        TArray<int16> PCM; PCM.Reserve(Samples.Num());
        for(float SampleValue:Samples) { PCM.Add(static_cast<int16>(FMath::Clamp(SampleValue/FMath::Max(Peak,.001f)*(bReload?.60f:.86f),-1.f,1.f)*32767)); }
        TArray<uint8> Wave; SerializeWaveFile(Wave,reinterpret_cast<const uint8*>(PCM.GetData()),PCM.Num()*sizeof(int16),1,Rate);
        const FString Wav=FPaths::ProjectDir()/TEXT("SourceArt/Audio/")+Name+TEXT(".wav");
        if(!FFileHelper::SaveArrayToFile(Wave,*Wav)) { return nullptr; }
        auto* Factory=NewObject<USoundFactory>(); Factory->bAutoCreateCue=false;
        auto* Sound=Cast<USoundWave>(UFactory::StaticImportObject(USoundWave::StaticClass(),CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone,*Wav,nullptr,Factory));
        if(Sound) { Sound->bLooping=false; Sound->PostEditChange(); }
        return Sound;
    }
}
UShooterShotgunPolishCommandlet::UShooterShotgunPolishCommandlet() { IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }
int32 UShooterShotgunPolishCommandlet::Main(const FString& Params)
{
    FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("BinkAudioDecoder"));
    auto* Definition=Load<UShooterWeaponDefinition>(TEXT("/Game/Weapons/Data/DA_Shotgun"));
    auto* Hip=Load<UAnimSequence>(TEXT("/Game/Assets/Characters/FPP_Animations/FPP_RifleFire"));
    auto* Aim=Load<UAnimSequence>(TEXT("/Game/Animations/AS_PlayerAimFire"));
    auto* Reload=Load<UAnimSequence>(TEXT("/Game/Assets/Characters/FPP_Animations/FPP_RifleReload"));
    auto* RifleSound=Load<USoundWave>(TEXT("/Game/Audio/SW_RifleShot"));
    if(!Definition||!Hip||!Aim||!Reload||!RifleSound||Hip->GetSkeleton()!=Aim->GetSkeleton()||Hip->GetSkeleton()!=Reload->GetSkeleton()) { return 1; }
    if(FParse::Param(*Params,TEXT("Grip"))||FParse::Param(*Params,TEXT("GripVerify"))) { return SetupShooterShotgunGrip(Definition,Hip,Params); }
    if(FParse::Param(*Params,TEXT("Diagnose")))
    {
        const auto& Ref=Hip->GetSkeleton()->GetReferenceSkeleton();
        const auto* Socket=Hip->GetSkeleton()->FindSocket(TEXT("Right_Weapon"));
        if(!Socket) { UE_LOG(LogShotgunPolish,Error,TEXT("Missing Right_Weapon socket")); return 8; }
        UE_LOG(LogShotgunPolish,Display,TEXT("GRIP socket parent=%s local=%s"),*Socket->BoneName.ToString(),*Socket->GetSocketLocalTransform().ToString());
        for(int32 I=0;I<Ref.GetNum();++I)
        { if(Ref.GetBoneName(I).ToString().Contains(TEXT("hand"),ESearchCase::IgnoreCase)) { UE_LOG(LogShotgunPolish,Display,TEXT("BONE %s"),*Ref.GetBoneName(I).ToString()); } }
        for(auto* Seq:{Hip,Aim,Reload})
        {
            for(float T:{0.f,.2f,.5f,.8f,1.f})
            {
                TArray<FTransform> CS;
                for(int32 I=0;I<Ref.GetNum();++I)
                {
                    TArray<FTransform> Keys; const FName Bone=Ref.GetBoneName(I);
                    if(Seq->GetDataModel()->IsValidBoneTrackName(Bone)) { Seq->GetDataModel()->GetBoneTrackTransforms(Bone,Keys); }
                    FTransform Pose=Sample(Keys,T,Ref.GetRefBonePose()[I]);
                    const int32 Parent=Ref.GetParentIndex(I); CS.Add(Parent==INDEX_NONE?Pose:Pose*CS[Parent]);
                }
                const FTransform Grip=Socket->GetSocketLocalTransform()*CS[Ref.FindBoneIndex(Socket->BoneName)];
                for(int32 I=0;I<Ref.GetNum();++I)
                {
                    const FString Name=Ref.GetBoneName(I).ToString();
                    if(Name.Equals(TEXT("b_LeftHand"),ESearchCase::IgnoreCase)||Name.Equals(TEXT("b_RightHand"),ESearchCase::IgnoreCase))
                    { UE_LOG(LogShotgunPolish,Display,TEXT("GRIP %s t=%.2f %s local=%s"),*Seq->GetName(),T,*Name,*Grip.InverseTransformPosition(CS[I].GetTranslation()).ToString()); }
                }
            }
        }
        return 0;
    }
    if(FParse::Param(*Params,TEXT("Repair")))
    {
        const FString Revised=TEXT("/Game/Animations/Shotgun/Correction/");
        // 不再对每节骨骼分别放大。保留源动作的手指/手腕配合，只调整时间曲线。
        auto* NewHip=MakeSequence(Hip,Revised+TEXT("AS_ShotgunFire"),.55f,1.f,false);
        auto* NewAim=MakeSequence(Aim,Revised+TEXT("AS_ShotgunAimFire"),.45f,1.f,false);
        auto* NewHipMontage=MakeMontage(NewHip,Revised+TEXT("AM_ShotgunFire"),false);
        auto* NewAimMontage=MakeMontage(NewAim,Revised+TEXT("AM_ShotgunAimFire"),false);
        auto* WholeReload=MakeSound(TEXT("ShotgunWholeReload"),TArray<int16>(),true);
        if(!NewHip||!NewAim||!NewHipMontage||!NewAimMontage||!WholeReload) { return 9; }
        for(UObject* Asset:TArray<UObject*>{NewHip,NewAim,NewHipMontage,NewAimMontage,WholeReload}) { if(!Save(Asset)) { return 10; } }
        Definition->FireMontage=NewHipMontage; Definition->AimFireMontage=NewAimMontage;
        Definition->ReloadSound=WholeReload;
        Definition->RecoilPitch=2.4f; Definition->AimRecoilMultiplier=.65f; Definition->RecoilKickDuration=.06f;
        if(!Save(Definition)) { return 11; }
        UE_LOG(LogShotgunPolish,Display,TEXT("REPAIR saved: unamplified bone animation, real 2.4 degree recoil, whole reload sound; accepted fire audio unchanged."));
        return 0;
    }
    const FString Root=TEXT("/Game/Animations/Shotgun/");
    if(FParse::Param(*Params,TEXT("Verify")))
    {
        bool bOK=true;
        for(const FString& Name:{FString(TEXT("Fire")),FString(TEXT("AimFire")),FString(TEXT("Reload"))})
        {
            const FString Folder=FParse::Param(*Params,TEXT("Corrected"))&&Name!=TEXT("Reload")?Root+TEXT("Correction/"):Root;
            auto* Seq=Load<UAnimSequence>(Folder+TEXT("AS_Shotgun")+Name);
            auto* Montage=Load<UAnimMontage>(Folder+TEXT("AM_Shotgun")+Name);
            bOK&=Seq&&Montage&&Seq->GetSkeleton()==Hip->GetSkeleton()&&Seq->Notifies.IsEmpty()&&Montage->Notifies.IsEmpty()&&Montage->GetPlayLength()>0;
        }
        for(auto* Sound:{Cast<USoundWave>(Definition->FireSound),Cast<USoundWave>(Definition->ReloadSound)})
        {
            TArray<uint8> PCM; uint32 Rate=0; uint16 Channels=0;
            // NullRHI Commandlet 没有播放代理，GetRuntimeFormat 会返回 InvalidFormat；显式验证 Windows 使用的 Bink 压缩。
            const SIZE_T CompressedBytes=Sound?Sound->GetCompressedDataSize(FName(TEXT("BINKA"))):0;
            const bool bSourceOK=Sound&&Sound->GetImportedSoundWaveData(PCM,Rate,Channels);
            const bool bSoundOK=bSourceOK&&Rate==48000&&Channels==1&&!Sound->bLooping&&PCM.Num()>0&&CompressedBytes>0;
            UE_LOG(LogShotgunPolish,Display,TEXT("PCM valid=%d bytes=%d rate=%u channels=%u BINKA=%llu"),bSourceOK,PCM.Num(),Rate,Channels,static_cast<uint64>(CompressedBytes));
            bOK&=bSoundOK; UE_LOG(LogShotgunPolish,Display,TEXT("AUDIO %s valid=%d duration=%.3f"),*GetNameSafe(Sound),bSoundOK,Sound?Sound->Duration:0);
        }
        const FString FireRoot=FParse::Param(*Params,TEXT("Corrected"))?Root+TEXT("Correction/"):Root;
        bOK&=Definition->FireMontage==Load<UAnimMontage>(FireRoot+TEXT("AM_ShotgunFire"))&&Definition->AimFireMontage==Load<UAnimMontage>(FireRoot+TEXT("AM_ShotgunAimFire"))&&Definition->ReloadMontage==Load<UAnimMontage>(Root+TEXT("AM_ShotgunReload"));
        UE_LOG(LogShotgunPolish,Display,TEXT("SHOTGUN persisted verification: %s"),bOK?TEXT("PASS"):TEXT("FAIL")); return bOK?0:2;
    }
    auto* HipCopy=MakeSequence(Hip,Root+TEXT("AS_ShotgunFire"),.55f,1.55f,false);
    auto* AimCopy=MakeSequence(Aim,Root+TEXT("AS_ShotgunAimFire"),.45f,1.35f,false);
    auto* ReloadCopy=MakeSequence(Reload,Root+TEXT("AS_ShotgunReload"),2.4f,1.f,true);
    auto* HipMontage=MakeMontage(HipCopy,Root+TEXT("AM_ShotgunFire"),false);
    auto* AimMontage=MakeMontage(AimCopy,Root+TEXT("AM_ShotgunAimFire"),false);
    auto* ReloadMontage=MakeMontage(ReloadCopy,Root+TEXT("AM_ShotgunReload"),true);
    TArray<uint8> SourcePCM; uint32 Rate=0; uint16 Channels=0;
    if(!RifleSound->GetImportedSoundWaveData(SourcePCM,Rate,Channels)||Rate!=48000||Channels!=1) { return 3; }
    TArray<int16> Source; Source.SetNum(SourcePCM.Num()/sizeof(int16)); FMemory::Memcpy(Source.GetData(),SourcePCM.GetData(),SourcePCM.Num());
    auto* Shot=MakeSound(TEXT("ShotgunShot"),Source,false); auto* ReloadAudio=MakeSound(TEXT("ShotgunReload"),Source,true);
    if(!HipCopy||!AimCopy||!ReloadCopy||!HipMontage||!AimMontage||!ReloadMontage||!Shot||!ReloadAudio) { UE_LOG(LogShotgunPolish,Error,TEXT("Cannot create assets (existing assets require -Verify).")); return 4; }
    for(UObject* Asset:TArray<UObject*>{HipCopy,AimCopy,ReloadCopy,HipMontage,AimMontage,ReloadMontage,Shot,ReloadAudio}) { if(!Save(Asset)) { return 5; } }
    Definition->FireMontage=HipMontage; Definition->AimFireMontage=AimMontage; Definition->ReloadMontage=ReloadMontage;
    Definition->FireSound=Shot; Definition->FireSoundVolume=.75f; Definition->ReloadSound=ReloadAudio;
    if(!Save(Definition)) { return 6; }
    UE_LOG(LogShotgunPolish,Display,TEXT("SHOTGUN polish saved: three retimed animations/montages and two original sounds. Source assets untouched.")); return 0;
}
