#include "ShooterFeedbackSetupCommandlet.h"
#include "Combat/ShooterFeedbackConfig.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "Factories/SoundFactory.h"
#include "AutomatedAssetImportData.h"
#include "Sound/SoundWave.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSpriteEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Distributions/DistributionFloatConstant.h"
#include "Distributions/DistributionFloatUniform.h"
#include "Distributions/DistributionFloatConstantCurve.h"
#include "Distributions/DistributionVectorConstant.h"
#include "Distributions/DistributionVectorUniform.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace
{
    template<class T> T* Asset(const TCHAR* Name)
    {
        const FString Path = FString(TEXT("/Game/Feedback/")) + Name;
        if (FPackageName::DoesPackageExist(Path)) { return LoadObject<T>(nullptr, *(Path + TEXT(".") + Name)); }
        auto* Object = NewObject<T>(CreatePackage(*Path), Name, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Object); return Object;
    }
    bool Save(UObject* Object)
    {
        if (!Object) { return false; }
        auto* Package = Object->GetOutermost(); Package->MarkPackageDirty();
        const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Package, Object, *File, Args);
    }
    template<class T> T* Node(UMaterial* Material)
    {
        auto* Value = NewObject<T>(Material); Material->GetExpressionCollection().AddExpression(Value); return Value;
    }
    void FloatConstant(UObject* Owner, FRawDistributionFloat& Value, float Constant)
    {
        auto* D = NewObject<UDistributionFloatConstant>(Owner); D->Constant = Constant; Value.Distribution = D;
    }
    void FloatRange(UObject* Owner, FRawDistributionFloat& Value, float Min, float Max)
    {
        auto* D = NewObject<UDistributionFloatUniform>(Owner); D->Min = Min; D->Max = Max; Value.Distribution = D;
    }
    void VectorRange(UObject* Owner, FRawDistributionVector& Value, FVector Min, FVector Max)
    {
        auto* D = NewObject<UDistributionVectorUniform>(Owner); D->Min = Min; D->Max = Max; Value.Distribution = D;
    }
    // 自制短确认音：确定性合成、无外部版权素材；保留 WAV 源文件便于替换试听。
    USoundWave* MakeSound(const TCHAR* Name, bool bKill, bool bReplace = false)
    {
        const FString PackagePath = FString(TEXT("/Game/Feedback/")) + Name;
        if (!bReplace && FPackageName::DoesPackageExist(PackagePath)) { return LoadObject<USoundWave>(nullptr, *(PackagePath + TEXT(".") + Name)); }
        constexpr int32 Rate = 44100; const int32 Count = FMath::RoundToInt(Rate * (bKill ? .20f : .095f));
        TArray<uint8> Wave;
        auto U16 = [&](uint16 V) { Wave.Add(V & 255); Wave.Add(V >> 8); };
        auto U32 = [&](uint32 V) { for (int32 I = 0; I < 4; ++I) { Wave.Add((V >> (I * 8)) & 255); } };
        auto Tag = [&](const ANSICHAR* S) { for (int32 I = 0; I < 4; ++I) { Wave.Add(S[I]); } };
        Tag("RIFF"); U32(36 + Count * 2); Tag("WAVE"); Tag("fmt "); U32(16); U16(1); U16(1); U32(Rate); U32(Rate * 2); U16(2); U16(16); Tag("data"); U32(Count * 2);
        FRandomStream Random(20261006);
        for (int32 I = 0; I < Count; ++I)
        {
            const float T = float(I) / Rate, EndFade = FMath::Clamp((float(Count - I) / Rate) / .015f, 0.f, 1.f);
            float V = .45f * Random.FRandRange(-1.f, 1.f) * FMath::Exp(-T * 190.f);
            V += .32f * FMath::Sin(2 * PI * (bKill ? 1100.f : 1700.f) * T) * FMath::Exp(-T * (bKill ? 24.f : 65.f));
            if (bKill && T > .045f) { V += .26f * FMath::Sin(2 * PI * 1650.f * (T - .045f)) * FMath::Exp(-(T - .045f) * 27.f); }
            if (!bKill)
            {
                // 清脆双泛音瞬态，低频留给枪声；短尾音使连续命中仍有独立节奏。
                V = .50f * FMath::Sin(2 * PI * 2400.f * T) * FMath::Exp(-T * 48.f)
                  + .23f * FMath::Sin(2 * PI * 3650.f * T) * FMath::Exp(-T * 72.f)
                  + .12f * Random.FRandRange(-1.f, 1.f) * FMath::Exp(-T * 260.f);
            }
            V *= FMath::Clamp(T / .0015f, 0.f, 1.f) * EndFade;
            U16(uint16(int16(FMath::Clamp(V, -.95f, .95f) * 32767)));
        }
        const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("SourceArt/Audio") / (FString(Name) + TEXT(".wav")));
        if (!FFileHelper::SaveArrayToFile(Wave, *File)) { return nullptr; }
        auto* Factory = NewObject<USoundFactory>(); Factory->AutomatedImportData = NewObject<UAutomatedAssetImportData>();
        auto* Sound = Cast<USoundWave>(UFactory::StaticImportObject(USoundWave::StaticClass(), CreatePackage(*PackagePath), Name, RF_Public | RF_Standalone, *File, nullptr, Factory));
        if (Sound) { FAssetRegistryModule::AssetCreated(Sound); Sound->bLooping = false; Save(Sound); }
        return Sound;
    }
}
UShooterFeedbackSetupCommandlet::UShooterFeedbackSetupCommandlet() { IsEditor = true; IsClient = false; IsServer = false; LogToConsole = true; }
int32 UShooterFeedbackSetupCommandlet::Main(const FString& Params)
{
    FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("BinkAudioDecoder"));
    if (FParse::Param(*Params, TEXT("Verify")))
    {
        auto* Config = LoadObject<UShooterFeedbackConfig>(nullptr, TEXT("/Game/Feedback/DA_CombatFeedback.DA_CombatFeedback"));
        const bool bValid = Config && Config->KillIcon && Config->HitSound && Config->KillSound && Config->BloodEffect && Config->BloodEffect->Emitters.Num() == 2;
        UE_LOG(LogTemp, Display, TEXT("Feedback asset verification: %s"), bValid ? TEXT("PASS") : TEXT("FAIL")); return bValid ? 0 : 1;
    }
    auto* Config = Asset<UShooterFeedbackConfig>(TEXT("DA_CombatFeedback"));
    // 调音仅更新指定命中 WAV 与配置，避免重写用户已经认可的图标、击杀音和血雾资产。
    if (FParse::Param(*Params, TEXT("TuneHit")))
    {
        Config->HitSound = MakeSound(TEXT("SW_HitConfirm"), false, true);
        Config->HitVolume = .55f; Config->HitDuration = .20f;
        Config->MarkerInnerOffset = 9.f; Config->MarkerLineLength = 13.f;
        Config->MarkerThickness = 3.f; Config->MarkerOutline = 1.f;
        const bool bOK = Config->HitSound && Save(Config);
        UE_LOG(LogTemp, Display, TEXT("Hit feedback tuning: %s"), bOK ? TEXT("PASS") : TEXT("FAIL"));
        return bOK ? 0 : 4;
    }
    constexpr auto TexturePath = TEXT("/Game/Feedback/T_KillIcon");
    UTexture2D* Texture = nullptr;
    if (FPackageName::DoesPackageExist(TexturePath)) { Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Feedback/T_KillIcon.T_KillIcon")); }
    else
    {
        auto* Factory = NewObject<UTextureFactory>(); Factory->AutomatedImportData = NewObject<UAutomatedAssetImportData>();
        const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("SourceArt/UI/KillIcon_Transparent.png"));
        Texture = Cast<UTexture2D>(UFactory::StaticImportObject(UTexture2D::StaticClass(), CreatePackage(TexturePath), TEXT("T_KillIcon"), RF_Public | RF_Standalone, *File, nullptr, Factory));
        if (Texture) { FAssetRegistryModule::AssetCreated(Texture); }
    }
    if (!Texture) { return 2; }
    Texture->LODGroup = TEXTUREGROUP_UI; Texture->CompressionSettings = TC_EditorIcon; Texture->MipGenSettings = TMGS_NoMipmaps; Texture->NeverStream = true; Texture->PostEditChange();
    auto* Material = Asset<UMaterial>(TEXT("M_BloodBurst"));
    Material->GetExpressionCollection().Empty(); Material->BlendMode = BLEND_Translucent; Material->SetShadingModel(MSM_Unlit); Material->TwoSided = true; Material->bUsedWithParticleSprites = true;
    // ParticleColor 未导出 C++ StaticClass，使用引擎反射类创建节点。
    auto* Color = NewObject<UMaterialExpression>(Material, LoadClass<UMaterialExpression>(nullptr, TEXT("/Script/Engine.MaterialExpressionParticleColor")));
    Material->GetExpressionCollection().AddExpression(Color);
    auto* UV = Node<UMaterialExpressionTextureCoordinate>(Material);
    auto* Mask = Node<UMaterialExpressionCustom>(Material); Mask->OutputType = CMOT_Float1;
    auto& Input = Mask->Inputs.AddDefaulted_GetRef(); Input.InputName = TEXT("UV"); Input.Input.Connect(0, UV);
    // 不规则柔边云团与小液滴共用；粒子自身负责定向速度、数量和生命周期。
    Mask->Code = TEXT("float2 p=(UV-0.5)*2; float a=atan2(p.y,p.x); float r=length(p); float edge=0.72+0.12*sin(a*5)+0.07*sin(a*9+1.2); float core=1-smoothstep(edge*0.25,edge,r); return core*core;");
    auto* Opacity = Node<UMaterialExpressionMultiply>(Material); Opacity->A.Connect(0, Mask); Opacity->B.Connect(4, Color);
    Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Color); Material->GetEditorOnlyData()->Opacity.Connect(0, Opacity); Material->PostEditChange();
    auto* Particle = Asset<UParticleSystem>(TEXT("P_BloodBurst")); Particle->Emitters.Empty(); Particle->bUseFixedRelativeBoundingBox = true; Particle->FixedRelativeBoundingBox = FBox(FVector(-100), FVector(100));
    for (int32 Kind = 0; Kind < 2; ++Kind)
    {
        auto* Emitter = NewObject<UParticleSpriteEmitter>(Particle); Particle->Emitters.Add(Emitter); Emitter->CreateLODLevel(0);
        auto* LOD = Emitter->LODLevels[0].Get(); LOD->Modules.Empty(); LOD->bEnabled = true;
        auto* Required = LOD->RequiredModule.Get(); Required->Material = Material; Required->EmitterDuration = .1f; Required->EmitterLoops = 1; Required->bUseLocalSpace = false; Required->ScreenAlignment = PSA_Square;
        FloatConstant(LOD->SpawnModule, LOD->SpawnModule->Rate, 0.f); LOD->SpawnModule->BurstList.Empty();
        FParticleBurst Burst; Burst.Count = Kind ? 8 : 5; Burst.CountLow = Burst.Count; Burst.Time = 0.f; LOD->SpawnModule->BurstList.Add(Burst);
        auto* Life = NewObject<UParticleModuleLifetime>(Particle); FloatRange(Life, Life->Lifetime, Kind ? .16f : .22f, Kind ? .28f : .34f); LOD->Modules.Add(Life);
        auto* Size = NewObject<UParticleModuleSize>(Particle); VectorRange(Size, Size->StartSize, FVector(Kind ? 1.f : 16.f), FVector(Kind ? 2.f : 28.f)); LOD->Modules.Add(Size);
        auto* Velocity = NewObject<UParticleModuleVelocity>(Particle); VectorRange(Velocity, Velocity->StartVelocity, FVector(Kind ? 100.f : 35.f, -45, -30), FVector(Kind ? 210.f : 85.f, 45, 65)); FloatConstant(Velocity, Velocity->StartVelocityRadial, 0.f); Velocity->bInWorldSpace = false; LOD->Modules.Add(Velocity);
        auto* Fade = NewObject<UParticleModuleColorOverLife>(Particle);
        auto* Red = NewObject<UDistributionVectorConstant>(Fade); Red->Constant = Kind ? FVector(.36f,.008f,.013f) : FVector(.28f,.006f,.012f); Fade->ColorOverLife.Distribution = Red;
        auto* Alpha = NewObject<UDistributionFloatConstantCurve>(Fade); Alpha->ConstantCurve.AddPoint(0.f, Kind ? .9f : .65f); Alpha->ConstantCurve.AddPoint(.2f, Kind ? .8f : .55f); Alpha->ConstantCurve.AddPoint(1.f, 0.f); Fade->AlphaOverLife.Distribution = Alpha; LOD->Modules.Add(Fade);
        for (const auto& Module : LOD->Modules) { Module->LODValidity = 1; }
        LOD->UpdateModuleLists(); Emitter->PostEditChange();
    }
    Particle->PostEditChange();
    Config->KillIcon = Texture; Config->HitSound = MakeSound(TEXT("SW_HitConfirm"), false); Config->KillSound = MakeSound(TEXT("SW_KillConfirm"), true); Config->BloodEffect = Particle;
    if (!Config->HitSound || !Config->KillSound || !Save(Texture) || !Save(Material) || !Save(Particle) || !Save(Config)) { return 3; }
    UE_LOG(LogTemp, Display, TEXT("PASS: created kill icon, two short confirmation sounds, two-emitter blood burst and feedback configuration")); return 0;
}
