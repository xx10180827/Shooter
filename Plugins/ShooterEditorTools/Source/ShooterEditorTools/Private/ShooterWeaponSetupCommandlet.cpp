#include "ShooterWeaponSetupCommandlet.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Characters/MyShooter.h"
#include "Animation/AnimMontage.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "AssetImportTask.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "K2Node_CallFunction.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterWeaponSetup,Log,All);
namespace
{
    template<class T> T* Load(const TCHAR* Path) { return LoadObject<T>(nullptr,*(FString(Path)+TEXT(".")+FPackageName::GetShortName(Path))); }
    bool Save(UObject* Object)
    {
        UPackage* Package=Object->GetOutermost(); Package->MarkPackageDirty();
        const FString File=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        return UPackage::SavePackage(Package,Object,*File,Args);
    }
    template<class T> T* Create(const TCHAR* Path)
    {
        if (FPackageName::DoesPackageExist(Path)) { return Load<T>(Path); }
        auto* Object=NewObject<T>(CreatePackage(Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone);
        FAssetRegistryModule::AssetCreated(Object); return Object;
    }
    template<class T> T* Import(const FString& File,const TCHAR* Path,UFactory* Factory)
    {
        if (FPackageName::DoesPackageExist(Path)) { return Load<T>(Path); }
        auto* Task=NewObject<UAssetImportTask>(); Task->bAutomated=true; Factory->SetAssetImportTask(Task);
        bool bCancelled=false;
        auto* Object=Cast<T>(Factory->ImportObject(T::StaticClass(),CreatePackage(Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone,File,nullptr,bCancelled));
        if (Object) { FAssetRegistryModule::AssetCreated(Object); } return Object;
    }
    UK2Node_CallFunction* Call(UEdGraph* Graph,UClass* Class,const TCHAR* Name,int32 X,int32 Y,bool bSelf=false)
    {
        FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph); auto* Node=Creator.CreateNode();
        Node->SetFromFunction(Class->FindFunctionByName(Name));
        if(bSelf) { Node->FunctionReference.SetSelfMember(Name); }
        Node->NodePosX=X; Node->NodePosY=Y; Creator.Finalize(); return Node;
    }
}
UShooterWeaponSetupCommandlet::UShooterWeaponSetupCommandlet() { IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }
int32 UShooterWeaponSetupCommandlet::Main(const FString& Params)
{
    // 单独配置步枪后坐力：只保存既有 DA_Rifle，不执行模型导入或蓝图改线。
    // VerifyRifleRecoil 在新进程中读取磁盘资源，确认参数已经持久化。
    if(FParse::Param(*Params,TEXT("RifleRecoil"))||FParse::Param(*Params,TEXT("VerifyRifleRecoil")))
    {
        auto* Rifle=Load<UShooterWeaponDefinition>(TEXT("/Game/Weapons/Data/DA_Rifle"));
        if(!Rifle) { UE_LOG(LogShooterWeaponSetup,Error,TEXT("DA_Rifle missing")); return 15; }
        if(FParse::Param(*Params,TEXT("VerifyRifleRecoil")))
        {
            const bool bValid=FMath::IsNearlyEqual(Rifle->RecoilPitch,.65f)
                &&FMath::IsNearlyEqual(Rifle->AimRecoilMultiplier,.65f)
                &&FMath::IsNearlyEqual(Rifle->RecoilKickDuration,.06f);
            UE_LOG(LogShooterWeaponSetup,Display,TEXT("Rifle recoil verification: %s Pitch=%.4f ADS=%.4f Duration=%.4f"),
                bValid?TEXT("PASS"):TEXT("FAIL"),Rifle->RecoilPitch,Rifle->AimRecoilMultiplier,Rifle->RecoilKickDuration);
            return bValid?0:16;
        }
        Rifle->RecoilPitch=.65f;
        Rifle->AimRecoilMultiplier=.65f;
        Rifle->RecoilKickDuration=.06f;
        return Save(Rifle)?0:17;
    }

    // 导出 X/Z 侧视线框检查枪口方向；只读取导入网格，不修改用户美术源文件。
    if(FParse::Param(*Params,TEXT("MeshProfile")))
    {
        auto* Mesh=Load<UStaticMesh>(TEXT("/Game/Assets/Weapons/Shotgun/SM_Shotgun"));
        if(!Mesh||!Mesh->GetRenderData()||Mesh->GetRenderData()->LODResources.IsEmpty()) { return 13; }
        const auto& LOD=Mesh->GetRenderData()->LODResources[0];
        FString SVG=TEXT("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1100\" height=\"360\" viewBox=\"0 0 1100 360\"><rect width=\"1100\" height=\"360\" fill=\"white\"/><text x=\"20\" y=\"25\" fill=\"black\">Imported FBX side view: X negative (left), X positive (right), Z up</text><g fill=\"none\" stroke=\"#333\" stroke-width=\"0.2\">");
        const auto& V=LOD.VertexBuffers.PositionVertexBuffer;
        for(int32 I=0;I+2<LOD.IndexBuffer.GetNumIndices();I+=3)
        {
            const FVector3f A=V.VertexPosition(LOD.IndexBuffer.GetIndex(I));
            const FVector3f B=V.VertexPosition(LOD.IndexBuffer.GetIndex(I+1));
            const FVector3f C=V.VertexPosition(LOD.IndexBuffer.GetIndex(I+2));
            SVG+=FString::Printf(TEXT("<path d=\"M %.2f %.2f L %.2f %.2f L %.2f %.2f Z\"/>"),550+A.X*5,180-A.Z*5,550+B.X*5,180-B.Z*5,550+C.X*5,180-C.Z*5);
        }
        SVG+=TEXT("</g></svg>");
        return FFileHelper::SaveStringToFile(SVG,*(FPaths::ProjectSavedDir()/TEXT("T14/ShotgunProfile.svg")))?0:14;
    }
    // 校准只写霰弹枪配置，不重新导入贴图或保存蓝图；源模型保持原始单位与轴向。
    if(FParse::Param(*Params,TEXT("Calibrate")))
    {
        auto* Definition=Load<UShooterWeaponDefinition>(TEXT("/Game/Weapons/Data/DA_Shotgun"));
        if(!Definition) { return 11; }
        float Yaw=-90,Pitch=0,Roll=0,Scale=.47346f,X=0,Y=20,Z=5,MX=0,MY=65,MZ=10.5f;
        FParse::Value(*Params,TEXT("Yaw="),Yaw); FParse::Value(*Params,TEXT("Pitch="),Pitch); FParse::Value(*Params,TEXT("Roll="),Roll);
        FParse::Value(*Params,TEXT("Scale="),Scale); FParse::Value(*Params,TEXT("X="),X); FParse::Value(*Params,TEXT("Y="),Y); FParse::Value(*Params,TEXT("Z="),Z);
        FParse::Value(*Params,TEXT("MX="),MX); FParse::Value(*Params,TEXT("MY="),MY); FParse::Value(*Params,TEXT("MZ="),MZ);
        Definition->MeshTransform=FTransform(FRotator(Pitch,Yaw,Roll),FVector(X,Y,Z),FVector(Scale));
        Definition->MuzzleLocation=FVector(MX,MY,MZ);
        return Save(Definition)?0:12;
    }
    const FString Source=FPaths::ProjectDir()/TEXT("SourceArt/Weapons/Shotgun/Meshy_AI_Rustborn_Rifle_0929100508_texture_fbx/Meshy_AI_Rustborn_Rifle_0929100508_texture");
    if (!FPaths::FileExists(Source+TEXT(".fbx"))) { UE_LOG(LogShooterWeaponSetup,Error,TEXT("Missing user FBX")); return 1; }
    auto* Factory=NewObject<UFbxFactory>();
    Factory->SetDetectImportTypeOnImport(false);
    Factory->ImportUI->bAutomatedImportShouldDetectType=false;
    Factory->ImportUI->MeshTypeToImport=FBXIT_StaticMesh;
    Factory->ImportUI->bImportAsSkeletal=false;
    Factory->ImportUI->bImportMaterials=false; Factory->ImportUI->bImportTextures=false;
    Factory->ImportUI->StaticMeshImportData->bCombineMeshes=true;
    Factory->ImportUI->StaticMeshImportData->bAutoGenerateCollision=false;
    auto* Mesh=Import<UStaticMesh>(Source+TEXT(".fbx"),TEXT("/Game/Assets/Weapons/Shotgun/SM_Shotgun"),Factory);
    if (!Mesh) { return 2; }
    auto* Base=Import<UTexture2D>(Source+TEXT(".png"),TEXT("/Game/Assets/Weapons/Shotgun/T_Shotgun_BaseColor"),NewObject<UTextureFactory>());
    auto* Normal=Import<UTexture2D>(Source+TEXT("_normal.png"),TEXT("/Game/Assets/Weapons/Shotgun/T_Shotgun_Normal"),NewObject<UTextureFactory>());
    auto* Rough=Import<UTexture2D>(Source+TEXT("_roughness.png"),TEXT("/Game/Assets/Weapons/Shotgun/T_Shotgun_Roughness"),NewObject<UTextureFactory>());
    auto* Metal=Import<UTexture2D>(Source+TEXT("_metallic.png"),TEXT("/Game/Assets/Weapons/Shotgun/T_Shotgun_Metallic"),NewObject<UTextureFactory>());
    if (!Base||!Normal||!Rough||!Metal) { return 3; }
    Base->SRGB=true; Normal->SRGB=false; Normal->CompressionSettings=TC_Normalmap;
    Rough->SRGB=false; Metal->SRGB=false; Rough->CompressionSettings=TC_Masks; Metal->CompressionSettings=TC_Masks;
    for (auto* Texture : {Base,Normal,Rough,Metal}) { Texture->PostEditChange(); if(!Save(Texture)) { return 4; } }
    auto* Material=Create<UMaterial>(TEXT("/Game/Assets/Weapons/Shotgun/M_Shotgun"));
    Material->GetExpressionCollection().Empty();
    auto Sample=[Material](UTexture2D* Texture,EMaterialSamplerType Type,int Y)
    {
        auto* Node=NewObject<UMaterialExpressionTextureSample>(Material);
        Node->Texture=Texture; Node->SamplerType=Type; Node->MaterialExpressionEditorX=-350; Node->MaterialExpressionEditorY=Y;
        Material->GetExpressionCollection().AddExpression(Node); return Node;
    };
    auto* Data=Material->GetEditorOnlyData();
    Data->BaseColor.Connect(0,Sample(Base,SAMPLERTYPE_Color,0));
    Data->Normal.Connect(0,Sample(Normal,SAMPLERTYPE_Normal,250));
    Data->Roughness.Connect(1,Sample(Rough,SAMPLERTYPE_Masks,500));
    Data->Metallic.Connect(1,Sample(Metal,SAMPLERTYPE_Masks,750));
    Material->PostEditChange();
    if(Mesh->GetStaticMaterials().IsEmpty()) { Mesh->GetStaticMaterials().Add(FStaticMaterial(Material)); }
    for(int32 I=0; I<Mesh->GetStaticMaterials().Num(); ++I) { Mesh->SetMaterial(I,Material); }
    Mesh->PostEditChange();
    UE_LOG(LogShooterWeaponSetup,Display,TEXT("Imported FBX bounds=%s vertices=%d triangles=%d"),*Mesh->GetBounds().ToString(),Mesh->GetNumVertices(0),Mesh->GetNumTriangles(0));
    if(!Save(Material)||!Save(Mesh)) { return 5; }
    if(FParse::Param(*Params,TEXT("ImportOnly"))) { return 0; }
    auto* BP=Load<UBlueprint>(TEXT("/Game/Blueprints/Shooter"));
    if(!BP) { return 6; }
    // 保存原蓝图作为本地回滚证据；不触碰用户地图与菜单资产。
    const FString Backup=FPaths::ProjectSavedDir()/TEXT("T14/Backup/Shooter.uasset");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
    if(!FPaths::FileExists(Backup)) { IFileManager::Get().Copy(*Backup,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),TEXT(".uasset"))); }
    auto* Character=Cast<AMyShooter>(BP->GeneratedClass->GetDefaultObject());
    auto* Rifle=Create<UShooterWeaponDefinition>(TEXT("/Game/Weapons/Data/DA_Rifle"));
    auto* Shotgun=Create<UShooterWeaponDefinition>(TEXT("/Game/Weapons/Data/DA_Shotgun"));
    // 首次迁移读取旧组件数值；以后运行工具不覆盖用户已经调整的数据资产数值。
    if(Rifle->DisplayName.IsEmpty())
    {
        auto* Old=Character->GetShooterWeapon();
        for(TFieldIterator<FProperty> It(UShooterWeaponDefinition::StaticClass());It;++It)
        {
            if(FProperty* SourceProperty=FindFProperty<FProperty>(Old->GetClass(),It->GetFName()))
            { if(It->SameType(SourceProperty)) { It->CopyCompleteValue(It->ContainerPtrToValuePtr<void>(Rifle),SourceProperty->ContainerPtrToValuePtr<void>(Old)); } }
        }
        Rifle->DisplayName=FText::FromString(TEXT("RIFLE"));
        Rifle->FireMontage=Load<UAnimMontage>(TEXT("/Game/Blueprints/Shooter_fire_Montage"));
        Rifle->AimFireMontage=Load<UAnimMontage>(TEXT("/Game/Animations/AM_PlayerAimFire"));
        Rifle->ReloadMontage=Load<UAnimMontage>(TEXT("/Game/Blueprints/Shooter_reload_GAS_Montage"));
    }
    if(Shotgun->DisplayName.IsEmpty())
    {
        Shotgun->DisplayName=FText::FromString(TEXT("SHOTGUN"));
        Shotgun->Damage=12.f; Shotgun->PelletCount=8; Shotgun->SpreadHalfAngle=4.f;
        Shotgun->FireInterval=.85f; Shotgun->Range=3500.f; Shotgun->bAutomatic=false;
        Shotgun->MagazineCapacity=8; Shotgun->InitialReserveAmmo=32; Shotgun->ReloadDuration=2.4f;
        Shotgun->FireSound=Rifle->FireSound; Shotgun->FireSoundVolume=.8f;
        Shotgun->BulletVisualClass=Rifle->BulletVisualClass;
        Shotgun->FireMontage=Rifle->FireMontage; Shotgun->AimFireMontage=Rifle->AimFireMontage; Shotgun->ReloadMontage=Rifle->ReloadMontage;
        // 按已核实的负 X 枪口方向转向正 Y，并校准握把和枪口；原 FBX 不做破坏性变换。
        const float Extent=Mesh->GetBounds().BoxExtent.GetMax();
        Shotgun->MeshTransform=FTransform(FRotator(0,-90,0),FVector(0,20,5),FVector(45.f/FMath::Max(Extent,.01f)));
        Shotgun->MuzzleLocation=FVector(0,65,10.5f);
    }
    Shotgun->WeaponMesh=Mesh;
    if(!Save(Rifle)||!Save(Shotgun)) { return 7; }
    auto* ArrayProperty=FindFProperty<FArrayProperty>(UShooterWeaponComponent::StaticClass(),TEXT("WeaponDefinitions"));
    FScriptArrayHelper Array(ArrayProperty,ArrayProperty->ContainerPtrToValuePtr<void>(Character->GetShooterWeapon()));
    Array.Resize(2); auto* Inner=CastFieldChecked<FObjectPropertyBase>(ArrayProperty->Inner);
    Inner->SetObjectPropertyValue(Array.GetRawPtr(0),Rifle); Inner->SetObjectPropertyValue(Array.GetRawPtr(1),Shotgun);
    // 只替换旧 T05 蒙太奇常量的输入，保留节点和其余连线供对比。
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); int32 Rewired=0;
    for(auto* Graph : Graphs)
    {
        const TArray<UEdGraphNode*> Nodes=Graph->Nodes;
        for(auto* Node : Nodes)
        {
            for(auto* Pin : Node->Pins)
            {
                if(Pin->Direction!=EGPD_Input || !Pin->LinkedTo.IsEmpty() || Pin->DefaultObject!=Rifle->ReloadMontage) { continue; }
                auto* GetWeapon=Call(Graph,AMyShooter::StaticClass(),TEXT("GetShooterWeapon"),Node->NodePosX-650,Node->NodePosY+200,true);
                auto* GetMontage=Call(Graph,UShooterWeaponComponent::StaticClass(),TEXT("GetReloadMontage"),Node->NodePosX-350,Node->NodePosY+200);
                if(!Graph->GetSchema()->TryCreateConnection(GetWeapon->FindPin(TEXT("ReturnValue")),GetMontage->FindPin(TEXT("self")))
                    || !Graph->GetSchema()->TryCreateConnection(GetMontage->FindPin(TEXT("ReturnValue")),Pin)) { return 8; }
                ++Rewired;
            }
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
    if(Log.NumErrors||BP->Status==BS_Error) { return 9; }
    if(!Save(BP)) { return 10; }
    UE_LOG(LogShooterWeaponSetup,Display,TEXT("T14 setup passed. Two definitions, imported mesh/PBR textures, reload inputs rewired=%d"),Rewired);
    return 0;
}
