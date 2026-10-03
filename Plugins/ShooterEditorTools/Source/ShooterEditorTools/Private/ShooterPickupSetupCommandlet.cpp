#include "ShooterPickupSetupCommandlet.h"
#include "ShooterPickupAppearance.h"
#include "ShooterPickupRingSetup.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Pickups/ShooterWeaponPickup.h"
#include "Pickups/ShooterAmmoPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/PlayerStart.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
DEFINE_LOG_CATEGORY_STATIC(LogShooterPickupSetup,Log,All);
namespace
{
    template<class T> T* Load(const TCHAR* Path) { return LoadObject<T>(nullptr,*(FString(Path)+TEXT(".")+FPackageName::GetShortName(Path))); }
    bool Save(UObject* Object)
    {
        auto* Package=Object->GetOutermost(); Package->MarkPackageDirty();
        const FString File=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        return UPackage::SavePackage(Package,Object,*File,Args);
    }
    UBlueprint* Make(const TCHAR* Path,UClass* Parent)
    {
        if(FPackageName::DoesPackageExist(Path)) { return Load<UBlueprint>(Path); }
        auto* BP=FKismetEditorUtilities::CreateBlueprint(Parent,CreatePackage(Path),*FPackageName::GetShortName(Path),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
        if(BP) { FAssetRegistryModule::AssetCreated(BP); } return BP;
    }
    bool CompileSave(UBlueprint* BP)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(BP); FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
        return !Log.NumErrors&&Save(BP);
    }
}
UShooterPickupSetupCommandlet::UShooterPickupSetupCommandlet() { IsEditor=true; IsClient=false; IsServer=false; LogToConsole=true; }
int32 UShooterPickupSetupCommandlet::Main(const FString& Params)
{
    const bool bVerify=FParse::Param(*Params,TEXT("Verify"));
    if(FParse::Param(*Params,TEXT("Ring"))) { return UpdateShooterPickupRing(bVerify); }
    if(FParse::Param(*Params,TEXT("RefillGold"))) { return UpdateShooterPickupAppearance(bVerify); }
    auto* Rifle=Load<UShooterWeaponDefinition>(TEXT("/Game/Weapons/Data/DA_Rifle"));
    auto* Shotgun=Load<UShooterWeaponDefinition>(TEXT("/Game/Weapons/Data/DA_Shotgun"));
    auto* PlayerBP=Load<UBlueprint>(TEXT("/Game/Blueprints/Shooter"));
    if(!Rifle||!Shotgun||!PlayerBP) { return 1; }
    auto* Player=Cast<AMyShooter>(PlayerBP->GeneratedClass->GetDefaultObject());
    auto* Count=FindFProperty<FIntProperty>(UShooterWeaponComponent::StaticClass(),TEXT("InitialWeaponCount"));
    if(!Player||!Count) { return 2; }
    if(!bVerify)
    {
        Count->SetPropertyValue_InContainer(Player->GetShooterWeapon(),1);
        Rifle->MaxReserveAmmo=90; Shotgun->MaxReserveAmmo=32;
        if(!Save(Rifle)||!Save(Shotgun)||!CompileSave(PlayerBP)) { return 3; }
    }
    Player=Cast<AMyShooter>(PlayerBP->GeneratedClass->GetDefaultObject());
    if(Count->GetPropertyValue_InContainer(Player->GetShooterWeapon())!=1) { return 4; }
    const TCHAR* Paths[]={TEXT("/Game/Pickups/BP_ShotgunPickup"),TEXT("/Game/Pickups/BP_RifleAmmoPickup"),TEXT("/Game/Pickups/BP_ShotgunAmmoPickup")};
    UBlueprint* Blueprints[3]={};
    for(int32 Index=0;Index<3;++Index)
    {
        Blueprints[Index]=bVerify?Load<UBlueprint>(Paths[Index]):Make(Paths[Index],Index==0?AShooterWeaponPickup::StaticClass():AShooterAmmoPickup::StaticClass());
        if(!Blueprints[Index]) { return 5; }
        if(!bVerify)
        {
            auto* Pickup=CastChecked<AShooterPickup>(Blueprints[Index]->GeneratedClass->GetDefaultObject());
            Pickup->ItemName=FText::FromString(Index==0?TEXT("霰弹枪"):Index==1?TEXT("步枪弹药（补满）"):TEXT("霰弹枪弹药（补满）"));
            if(auto* Gun=Cast<AShooterWeaponPickup>(Pickup))
            {
                Gun->WeaponDefinition=Shotgun; Gun->DisplayMesh->SetStaticMesh(Shotgun->WeaponMesh);
                const float Scale=.47346f; Gun->DisplayMesh->SetRelativeScale3D(FVector(Scale));
                // 横向展示枪身便于出生点观察；表现旋转不会移动范围触发器。
                const FRotator DisplayRotation(0,90,0); Gun->DisplayMesh->SetRelativeRotation(DisplayRotation);
                Gun->DisplayMesh->SetRelativeLocation(-DisplayRotation.RotateVector(Shotgun->WeaponMesh->GetBounds().Origin*Scale));
                Gun->FocusBounds->SetBoxExtent(FVector(25,55,25));
            }
            else
            {
                auto* Ammo=CastChecked<AShooterAmmoPickup>(Pickup); Ammo->WeaponDefinition=Index==1?Rifle:Shotgun; Ammo->Amount=Index==1?30:8;
                Ammo->DisplayMesh->SetStaticMesh(Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube")));
                Ammo->DisplayMesh->SetRelativeScale3D(FVector(.36f,.24f,.22f)); Ammo->FocusBounds->SetBoxExtent(FVector(22,18,16));
            }
            if(!CompileSave(Blueprints[Index])) { return 6; }
        }
    }
    const FString Filename=FPackageName::LongPackageNameToFilename(TEXT("/Game/Maps/bloodstrike"),FPackageName::GetMapPackageExtension());
    if(!FEditorFileUtils::LoadMap(Filename,false,true)) { return 7; }
    UWorld* World=GEditor->GetEditorWorldContext().World(); APlayerStart* Start=nullptr;
    for(TActorIterator<APlayerStart> It(World);It;++It) { Start=*It; break; }
    if(!Start) { return 8; }
    int32 Created=0;
    for(int32 Index=0;Index<3;++Index)
    {
        const FName Tag(*FString::Printf(TEXT("T15_Pickup_%d"),Index)); AShooterPickup* Existing=nullptr;
        for(TActorIterator<AShooterPickup> It(World);It;++It) { if(It->ActorHasTag(Tag)) { Existing=*It; break; } }
        if(!Existing)
        {
            if(bVerify) { return 9; }
            const FRotator Yaw(0,Start->GetActorRotation().Yaw,0);
            const FVector Right=FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
            FVector Position=Start->GetActorLocation()+Yaw.Vector()*180.f+Right*((Index-1)*110.f);
            FHitResult Floor; FCollisionQueryParams Query;
            if(World->LineTraceSingleByChannel(Floor,Position+FVector(0,0,200),Position-FVector(0,0,500),ECC_Visibility,Query)) { Position.Z=Floor.ImpactPoint.Z+100; }
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Existing=World->SpawnActor<AShooterPickup>(Blueprints[Index]->GeneratedClass,Position,Yaw,Spawn);
            if(!Existing) { return 10; }
            Existing->Tags.Add(Tag); Existing->SetActorLabel(Index==0?TEXT("T15_Shotgun_Pickup"):Index==1?TEXT("T15_Rifle_Ammo"):TEXT("T15_Shotgun_Ammo")); ++Created;
        }
        UE_LOG(LogShooterPickupSetup,Display,TEXT("Pickup %d %s at %s mesh=%s"),Index,*Existing->ItemName.ToString(),*Existing->GetActorLocation().ToString(),*GetNameSafe(Existing->DisplayMesh->GetStaticMesh()));
        if(!Existing->DisplayMesh->GetStaticMesh()) { return 11; }
    }
    if(Created>0)
    {
        // 加载并保存当前本地地图，只添加带 T15 标记的物品，保留原有玩家改动和所有场景 Actor。
        World->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(World->GetOutermost(),World,*Filename,Args)) { return 12; }
    }
    UE_LOG(LogShooterPickupSetup,Display,TEXT("PASS: initial owned=1, reserve caps=%d/%d, pickups=3, created=%d"),Rifle->MaxReserveAmmo,Shotgun->MaxReserveAmmo,Created);
    return 0;
}
