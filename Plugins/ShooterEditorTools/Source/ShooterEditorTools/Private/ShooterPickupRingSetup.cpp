#include "ShooterPickupRingSetup.h"
#include "Pickups/ShooterPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "EngineUtils.h"
namespace
{
    template<class T> T* Load(const TCHAR* Path) { return LoadObject<T>(nullptr,*(FString(Path)+TEXT(".")+FPackageName::GetShortName(Path))); }
    template<class T> T* Asset(const TCHAR* Path)
    {
        if(auto* Existing=Load<T>(Path)) { return Existing; }
        auto* Object=NewObject<T>(CreatePackage(Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone);
        FAssetRegistryModule::AssetCreated(Object); return Object;
    }
    bool Save(UObject* Object)
    {
        auto* Package=Object->GetOutermost(); Package->MarkPackageDirty();
        const FString File=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; return UPackage::SavePackage(Package,Object,*File,Args);
    }
    template<class T> T* Node(UMaterial* Material,int32 X,int32 Y)
    {
        auto* Result=NewObject<T>(Material); Result->MaterialExpressionEditorX=X; Result->MaterialExpressionEditorY=Y;
        Material->GetExpressionCollection().AddExpression(Result); return Result;
    }
    void Apply(AShooterPickup* Pickup,UStaticMesh* Plane,UMaterialInterface* Ring)
    {
        // 清除上轮金色模型覆盖，回到网格自带材质；光效仅由独立地面组件承载。
        Pickup->DisplayMesh->EmptyOverrideMaterials();
        Pickup->GroundMarker->SetStaticMesh(Plane); Pickup->GroundMarker->SetMaterial(0,Ring);
        Pickup->GroundMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Pickup->GroundMarker->SetGenerateOverlapEvents(false);
        Pickup->GroundMarker->SetCanEverAffectNavigation(false); Pickup->GroundMarker->SetCastShadow(false);
        Pickup->RefreshGroundMarker();
    }
    bool Check(AShooterPickup* Pickup,UMaterialInterface* Ring)
    {
        if(!Pickup||!Pickup->GroundMarker||!Pickup->DisplayMesh->GetStaticMesh()) { return false; }
        if(Pickup->GroundMarker->GetMaterial(0)!=Ring||Pickup->GroundMarker->GetCollisionEnabled()!=ECollisionEnabled::NoCollision) { return false; }
        for(int32 Slot=0;Slot<Pickup->DisplayMesh->GetNumMaterials();++Slot)
        { if(Pickup->DisplayMesh->GetMaterial(Slot)!=Pickup->DisplayMesh->GetStaticMesh()->GetMaterial(Slot)) { return false; } }
        return true;
    }
}
int32 UpdateShooterPickupRing(bool bVerify)
{
    constexpr auto MaterialPath=TEXT("/Game/Pickups/Materials/M_PickupMarker");
    constexpr auto InstancePath=TEXT("/Game/Pickups/Materials/MI_PickupMarker");
    auto* Material=bVerify?Load<UMaterial>(MaterialPath):Asset<UMaterial>(MaterialPath);
    auto* Instance=bVerify?Load<UMaterialInstanceConstant>(InstancePath):Asset<UMaterialInstanceConstant>(InstancePath);
    auto* Plane=Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane"));
    if(!Material||!Instance||!Plane) { return 41; }
    if(!bVerify)
    {
        // UV 程序化生成双环、外侧分段和四向刻度；黑色区域完全透明，不需要外部图片。
        Material->GetExpressionCollection().Empty(); Material->SetShadingModel(MSM_Unlit);
        Material->BlendMode=BLEND_Additive; Material->TwoSided=true;
        auto* UV=Node<UMaterialExpressionTextureCoordinate>(Material,-900,250);
        auto* Width=Node<UMaterialExpressionScalarParameter>(Material,-900,420); Width->ParameterName=TEXT("LineWidth"); Width->DefaultValue=.015f;
        auto* Mask=Node<UMaterialExpressionCustom>(Material,-550,200);
        Mask->Description=TEXT("双圆环、外侧分段与四向刻度（透明背景）"); Mask->OutputType=CMOT_Float1;
        Mask->Inputs.Empty();
        auto& UVInput=Mask->Inputs.AddDefaulted_GetRef(); UVInput.InputName=TEXT("UV"); UVInput.Input.Connect(0,UV);
        auto& WidthInput=Mask->Inputs.AddDefaulted_GetRef(); WidthInput.InputName=TEXT("Width"); WidthInput.Input.Connect(0,Width);
        Mask->Code=TEXT(R"HLSL(
float2 p=(UV-0.5)*2.0;
float r=length(p);
float w=max(Width,0.003);
float aa=max(fwidth(r),0.002);
float mainRing=1-smoothstep(w,w+aa,abs(r-0.77));
float innerRing=(1-smoothstep(w*0.4,w*0.4+aa,abs(r-0.64)))*0.45;
float angle=atan2(p.y,p.x);
float segment=step(0.18,frac((angle+3.14159265)/6.2831853*12.0));
float outerRing=(1-smoothstep(w*0.55,w*0.55+aa,abs(r-0.89)))*segment*0.65;
float crossAxis=min(abs(p.x),abs(p.y));
float tick=(1-smoothstep(w*1.4,w*1.4+aa,crossAxis))*smoothstep(0.78,0.8,r)*(1-smoothstep(0.96,0.98,r));
float halo=exp(-abs(r-0.77)*65.0)*0.18;
return saturate(max(max(mainRing,innerRing),max(outerRing,tick))+halo)*(1-smoothstep(0.99,1.0,r));
)HLSL");
        auto* Color=Node<UMaterialExpressionVectorParameter>(Material,-550,-180); Color->ParameterName=TEXT("GoldColor"); Color->DefaultValue=FLinearColor(1.f,.5f,.025f,1.f);
        auto* Glow=Node<UMaterialExpressionScalarParameter>(Material,-550,-20); Glow->ParameterName=TEXT("GlowStrength"); Glow->DefaultValue=5.f;
        auto* Emission=Node<UMaterialExpressionMultiply>(Material,-200,-150); Emission->A.Connect(0,Color); Emission->B.Connect(0,Glow);
        Material->GetEditorOnlyData()->EmissiveColor.Connect(0,Emission);
        Material->GetEditorOnlyData()->Opacity.Connect(0,Mask);
        Material->PostEditChange(); Instance->SetParentEditorOnly(Material); Instance->PostEditChange();
        if(!Save(Material)||!Save(Instance)) { return 42; }
    }
    const TCHAR* Paths[]={TEXT("/Game/Pickups/BP_ShotgunPickup"),TEXT("/Game/Pickups/BP_RifleAmmoPickup"),TEXT("/Game/Pickups/BP_ShotgunAmmoPickup")};
    UBlueprint* Blueprints[3]={};
    for(int32 Index=0;Index<3;++Index)
    {
        auto* BP=Load<UBlueprint>(Paths[Index]); if(!BP||!BP->GeneratedClass) { return 43; } Blueprints[Index]=BP;
        auto* Default=Cast<AShooterPickup>(BP->GeneratedClass->GetDefaultObject()); if(!Default||!Default->GroundMarker) { return 44; }
        if(!bVerify)
        {
            Apply(Default,Plane,Instance); FBlueprintEditorUtils::MarkBlueprintAsModified(BP); FCompilerResultsLog Log;
            FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log); if(Log.NumErrors||!Save(BP)) { return 45; }
        }
        if(!Check(Cast<AShooterPickup>(BP->GeneratedClass->GetDefaultObject()),Instance)) { return 46; }
    }
    const FString Filename=FPackageName::LongPackageNameToFilename(TEXT("/Game/Maps/bloodstrike"),FPackageName::GetMapPackageExtension());
    if(!FEditorFileUtils::LoadMap(Filename,false,true)) { return 47; }
    auto* World=GEditor->GetEditorWorldContext().World(); int32 Count=0;
    for(TActorIterator<AShooterPickup> It(World);It;++It)
    {
        for(auto* BP:Blueprints)
        {
            if(!It->IsA(BP->GeneratedClass)) { continue; }
            if(!bVerify) { Apply(*It,Plane,Instance); }
            if(!Check(*It,Instance)) { return 48; }
            UE_LOG(LogTemp,Display,TEXT("Ring pickup %s actor=%s marker=%s visible=%d"),*It->GetActorLabel(),*It->GetActorLocation().ToString(),*It->GroundMarker->GetComponentLocation().ToString(),It->GroundMarker->IsVisible());
            ++Count;
        }
    }
    if(Count<3) { return 49; }
    if(!bVerify)
    {
        // 增量保存当前本地地图，保留用户摆放、名称、武器数据及其他关卡修改。
        World->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(World->GetOutermost(),World,*Filename,Args)) { return 50; }
    }
    UE_LOG(LogTemp,Display,TEXT("PASS: %d ground rings, original mesh materials, no marker collision"),Count); return 0;
}