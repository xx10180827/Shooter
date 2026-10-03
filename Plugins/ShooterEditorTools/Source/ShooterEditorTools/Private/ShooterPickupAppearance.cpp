#include "ShooterPickupAppearance.h"
#include "Pickups/ShooterPickup.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Pickups/ShooterAmmoPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
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
    void Apply(AShooterPickup* Pickup,UMaterialInterface* Material,int32 Index)
    {
        Pickup->ItemName=FText::FromString(Index==0?TEXT("霰弹枪"):Index==1?TEXT("步枪弹药（补满）"):TEXT("霰弹枪弹药（补满）"));
        // 只修改场景拾取展示组件的 OverrideMaterials，共享枪模和玩家手持材质不变。
        for(int32 Slot=0;Slot<Pickup->DisplayMesh->GetNumMaterials();++Slot) { Pickup->DisplayMesh->SetMaterial(Slot,Material); }
    }
}
int32 UpdateShooterPickupAppearance(bool bVerify)
{
    // 用户明确要求恢复原补给值：步枪 30/90，霰弹枪 8/32，备用上限同初始量。
    auto* Rifle=Load<UShooterWeaponDefinition>(TEXT("/Game/Weapons/Data/DA_Rifle"));
    auto* Shotgun=Load<UShooterWeaponDefinition>(TEXT("/Game/Weapons/Data/DA_Shotgun"));
    if(!Rifle||!Shotgun) { return 31; }
    if(!bVerify)
    {
        Rifle->MaxReserveAmmo=90; Shotgun->MaxReserveAmmo=32;
        if(!Save(Rifle)||!Save(Shotgun)) { return 32; }
    }
    if(Rifle->MagazineCapacity!=30||Rifle->MaxReserveAmmo!=90||Shotgun->MagazineCapacity!=8||Shotgun->MaxReserveAmmo!=32) { return 33; }
    constexpr auto MaterialPath=TEXT("/Game/Pickups/Materials/M_PickupGold");
    constexpr auto InstancePath=TEXT("/Game/Pickups/Materials/MI_PickupGold");
    UMaterial* Material=bVerify?Load<UMaterial>(MaterialPath):Asset<UMaterial>(MaterialPath);
    UMaterialInstanceConstant* Instance=bVerify?Load<UMaterialInstanceConstant>(InstancePath):Asset<UMaterialInstanceConstant>(InstancePath);
    if(!Material||!Instance) { return 20; }
    if(!bVerify)
    {
        // 金色基底保留体积明暗，Fresnel 让轮廓略亮；实例参数可独立调整颜色和发光强度。
        Material->GetExpressionCollection().Empty(); Material->SetShadingModel(MSM_DefaultLit);
        auto* Color=Node<UMaterialExpressionVectorParameter>(Material,-650,-200); Color->ParameterName=TEXT("GoldColor"); Color->DefaultValue=FLinearColor(1.f,.48f,.018f,1.f);
        auto* Glow=Node<UMaterialExpressionScalarParameter>(Material,-650,0); Glow->ParameterName=TEXT("GlowStrength"); Glow->DefaultValue=1.5f;
        auto* Rim=Node<UMaterialExpressionScalarParameter>(Material,-650,200); Rim->ParameterName=TEXT("RimStrength"); Rim->DefaultValue=3.f;
        auto* Fresnel=Node<UMaterialExpressionFresnel>(Material,-650,400); Fresnel->Exponent=3.f; Fresnel->BaseReflectFraction=.05f;
        auto* RimScale=Node<UMaterialExpressionMultiply>(Material,-400,220); RimScale->A.Connect(0,Fresnel); RimScale->B.Connect(0,Rim);
        auto* Strength=Node<UMaterialExpressionAdd>(Material,-200,100); Strength->A.Connect(0,Glow); Strength->B.Connect(0,RimScale);
        auto* Emissive=Node<UMaterialExpressionMultiply>(Material,0,0); Emissive->A.Connect(0,Color); Emissive->B.Connect(0,Strength);
        auto* Metallic=Node<UMaterialExpressionConstant>(Material,-200,-350); Metallic->R=.65f;
        auto* Roughness=Node<UMaterialExpressionConstant>(Material,-200,-250); Roughness->R=.3f;
        auto* Data=Material->GetEditorOnlyData(); Data->BaseColor.Connect(0,Color); Data->EmissiveColor.Connect(0,Emissive);
        Data->Metallic.Connect(0,Metallic); Data->Roughness.Connect(0,Roughness); Material->PostEditChange();
        Instance->SetParentEditorOnly(Material); Instance->PostEditChange();
        if(!Save(Material)||!Save(Instance)) { return 21; }
    }
    const TCHAR* Paths[]={TEXT("/Game/Pickups/BP_ShotgunPickup"),TEXT("/Game/Pickups/BP_RifleAmmoPickup"),TEXT("/Game/Pickups/BP_ShotgunAmmoPickup")};
    UBlueprint* Blueprints[3]={};
    for(int32 Index=0;Index<3;++Index)
    {
        auto* BP=Load<UBlueprint>(Paths[Index]); if(!BP) { return 22; } Blueprints[Index]=BP;
        auto* Default=Cast<AShooterPickup>(BP->GeneratedClass->GetDefaultObject()); if(!Default) { return 23; }
        if(!bVerify)
        {
            Apply(Default,Instance,Index); FBlueprintEditorUtils::MarkBlueprintAsModified(BP); FCompilerResultsLog Log;
            FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log); if(Log.NumErrors||!Save(BP)) { return 24; }
        }
        Default=Cast<AShooterPickup>(BP->GeneratedClass->GetDefaultObject());
        if(Default->DisplayMesh->GetMaterial(0)!=Instance) { return 25; }
    }
    const FString Filename=FPackageName::LongPackageNameToFilename(TEXT("/Game/Maps/bloodstrike"),FPackageName::GetMapPackageExtension());
    if(!FEditorFileUtils::LoadMap(Filename,false,true)) { return 26; }
    auto* World=GEditor->GetEditorWorldContext().World(); int32 Count=0;
    for(TActorIterator<AShooterPickup> It(World);It;++It)
    {
        for(int32 Index=0;Index<3;++Index)
        {
            if(!It->IsA(Blueprints[Index]->GeneratedClass)) { continue; }
            if(!bVerify) { Apply(*It,Instance,Index); }
            if(It->DisplayMesh->GetMaterial(0)!=Instance) { return 27; }
            if(Index>0&&!It->ItemName.ToString().Contains(TEXT("补满"))) { return 28; }
            ++Count;
        }
    }
    if(Count<3) { return 29; }
    if(!bVerify)
    {
        // 更新当前本地地图中的匹配拾取物材质/名称，位置、范围与其他场景资产保留。
        World->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(World->GetOutermost(),World,*Filename,Args)) { return 30; }
    }
    UE_LOG(LogTemp,Display,TEXT("PASS: gold material and full-refill names saved for %d pickups"),Count); return 0;
}
