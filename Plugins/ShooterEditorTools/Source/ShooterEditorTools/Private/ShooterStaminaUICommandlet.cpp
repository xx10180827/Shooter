#include "ShooterStaminaUICommandlet.h"
#include "Engine/Texture2D.h"
#include "Engine/Blueprint.h"
#include "Characters/MyShooter.h"
#include "Movement/ShooterDashComponent.h"
#include "Factories/TextureFactory.h"
#include "AutomatedAssetImportData.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
namespace
{
    bool Save(UObject* Object)
    {
        auto* P=Object->GetOutermost(); P->MarkPackageDirty();
        const FString Filename=FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; return UPackage::SavePackage(P,Object,*Filename,Args);
    }
    template<class T> T* Node(UMaterial* M,int32 X,int32 Y)
    {
        auto* N=NewObject<T>(M); N->MaterialExpressionEditorX=X; N->MaterialExpressionEditorY=Y; M->GetExpressionCollection().AddExpression(N); return N;
    }
}
UShooterStaminaUICommandlet::UShooterStaminaUICommandlet() { IsEditor=true; IsClient=false; IsServer=false; LogToConsole=true; }
int32 UShooterStaminaUICommandlet::Main(const FString& Params)
{
    const bool bVerify=FParse::Param(*Params,TEXT("Verify"));
    constexpr auto TexturePath=TEXT("/Game/UI/T_StaminaEnergy");
    constexpr auto MaterialPath=TEXT("/Game/UI/M_StaminaEnergy");
    UTexture2D* Texture=FindObject<UTexture2D>(nullptr,TEXT("/Game/UI/T_StaminaEnergy.T_StaminaEnergy"));
    if(!Texture&&FPackageName::DoesPackageExist(TexturePath)) { Texture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/T_StaminaEnergy.T_StaminaEnergy")); }
    if(!Texture&&!bVerify)
    {
        auto* Factory=NewObject<UTextureFactory>(); Factory->AutomatedImportData=NewObject<UAutomatedAssetImportData>();
        const FString File=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("SourceArt/UI/Energy_Transparent.png"));
        Texture=Cast<UTexture2D>(UFactory::StaticImportObject(UTexture2D::StaticClass(),CreatePackage(TexturePath),TEXT("T_StaminaEnergy"),RF_Public|RF_Standalone,*File,nullptr,Factory));
        if(Texture) { FAssetRegistryModule::AssetCreated(Texture); }
    }
    if(!Texture) { return 1; }
    UMaterial* M=nullptr;
    if(FPackageName::DoesPackageExist(MaterialPath)) { M=LoadObject<UMaterial>(nullptr,TEXT("/Game/UI/M_StaminaEnergy.M_StaminaEnergy")); }
    if(!M&&!bVerify)
    { M=NewObject<UMaterial>(CreatePackage(MaterialPath),TEXT("M_StaminaEnergy"),RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(M); }
    if(!M) { return 2; }
    if(!bVerify)
    {
        Texture->LODGroup=TEXTUREGROUP_UI; Texture->CompressionSettings=TC_EditorIcon; Texture->MipGenSettings=TMGS_NoMipmaps; Texture->NeverStream=true; Texture->SRGB=true; Texture->PostEditChange();
        M->GetExpressionCollection().Empty(); M->MaterialDomain=MD_UI; M->BlendMode=BLEND_Translucent;
        auto* UV=Node<UMaterialExpressionTextureCoordinate>(M,-850,180);
        auto* Image=Node<UMaterialExpressionTextureSample>(M,-650,-100); Image->Texture=Texture; Image->SamplerType=SAMPLERTYPE_Color; Image->Coordinates.Connect(0,UV);
        auto* Fraction=Node<UMaterialExpressionScalarParameter>(M,-650,400); Fraction->ParameterName=TEXT("StaminaFraction"); Fraction->DefaultValue=1.f;
        auto* Color=Node<UMaterialExpressionCustom>(M,-250,0); Color->OutputType=CMOT_Float3; Color->Description=TEXT("仅熄灭已消耗部分的蓝色能量，金属外框保持完整"); Color->Inputs.Empty();
        auto& A=Color->Inputs.AddDefaulted_GetRef(); A.InputName=TEXT("RGB"); A.Input.Connect(0,Image);
        auto& B=Color->Inputs.AddDefaulted_GetRef(); B.InputName=TEXT("UV"); B.Input.Connect(0,UV);
        auto& C=Color->Inputs.AddDefaulted_GetRef(); C.InputName=TEXT("Fraction"); C.Input.Connect(0,Fraction);
        Color->Code=TEXT(R"HLSL(
float x=saturate((UV.x-0.025)/0.95);
float filled=1-smoothstep(Fraction-0.004,Fraction+0.004,x);
filled=(Fraction>=0.999)?1:((Fraction<=0.001)?0:filled);
float energy=saturate((max(RGB.g,RGB.b)-RGB.r)*8);
return RGB*lerp(1.0,0.12,energy*(1-filled));
)HLSL");
        M->GetEditorOnlyData()->EmissiveColor.Connect(0,Color); M->GetEditorOnlyData()->Opacity.Connect(4,Image);
        M->PostEditChange(); if(!Save(Texture)||!Save(M)) { return 3; }
    }
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/Shooter.Shooter"));
    auto* Player=BP&&BP->GeneratedClass?Cast<AMyShooter>(BP->GeneratedClass->GetDefaultObject()):nullptr;
    auto* Dash=Player?Player->FindComponentByClass<UShooterDashComponent>():nullptr; if(!Dash) { return 4; }
    if(!FMath::IsNearlyEqual(Dash->DashDistance,480.f))
    {
        if(bVerify) { UE_LOG(LogTemp,Error,TEXT("Player dash distance %.1f instead of 480"),Dash->DashDistance); return 5; }
        Dash->DashDistance=480.f; FBlueprintEditorUtils::MarkBlueprintAsModified(BP); FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log); if(Log.NumErrors||!Save(BP)) { return 6; }
    }
    if(M->MaterialDomain!=MD_UI||M->BlendMode!=BLEND_Translucent) { return 7; }
    UE_LOG(LogTemp,Display,TEXT("PASS: stamina energy UI material/texture loaded, player dash distance=480 cm; texture=%dx%d"),Texture->GetSizeX(),Texture->GetSizeY()); return 0;
}