#include "ShooterRoundTools.h"
#include "Game/ShooterGameMode.h"
#include "UI/ShooterAmmoWidget.h"
#include "UI/ShooterMenuWidget.h"
#include "Player/ShooterPlayerController.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "Factories/TextureFactory.h"
#include "AutomatedAssetImportData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
    bool Save(UObject* Asset)
    {
        UPackage* Package = Asset->GetOutermost(); Package->MarkPackageDirty();
        const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *File, Args);
    }
    bool Compile(UBlueprint* BP)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
        return !Log.NumErrors && BP->Status != BS_Error;
    }
    UCanvasPanelSlot* Place(UCanvasPanel* Parent, UWidget* Widget, FVector2D P, FVector2D Size)
    {
        UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Widget); Slot->SetPosition(P); Slot->SetSize(Size); return Slot;
    }
    UCanvasPanelSlot* Fill(UCanvasPanel* Parent, UWidget* Widget, FMargin Margin = FMargin(0))
    {
        UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Widget);
        Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(Margin); return Slot;
    }
    UTextBlock* Text(UWidgetTree* Tree, FName Name, const TCHAR* Value, int Size, FLinearColor Color = FLinearColor::White)
    {
        UTextBlock* Widget = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Widget->bIsVariable = true; Widget->SetText(FText::FromString(Value));
        FSlateFontInfo Font = Widget->GetFont(); Font.Size = Size; Font.TypefaceFontName = TEXT("Bold"); Widget->SetFont(Font);
        Widget->SetColorAndOpacity(FSlateColor(Color)); return Widget;
    }
    UWidgetBlueprint* NewWidget(const TCHAR* Path, UClass* Parent)
    {
        UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>(); Factory->ParentClass = Parent;
        return Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(), CreatePackage(Path),
            FName(*FPackageName::GetShortName(Path)), RF_Public | RF_Standalone, nullptr, GWarn));
    }
    UButton* Button(UWidgetTree* Tree, FName Name, const TCHAR* Label = nullptr)
    {
        UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name); B->bIsVariable = true;
        FButtonStyle Style = B->GetStyle();
        Style.Normal.TintColor = FSlateColor(Label ? FLinearColor(0.06f,0.13f,0.19f,1) : FLinearColor(1,1,1,0));
        Style.Hovered.TintColor = FSlateColor(Label ? FLinearColor(0.08f,0.28f,0.38f,1) : FLinearColor(0.1f,0.65f,1,0.16f));
        Style.Pressed.TintColor = FSlateColor(FLinearColor(0.1f,0.55f,1,0.28f));
        Style.Disabled.TintColor = FSlateColor(FLinearColor(0.2f,0.2f,0.2f,0.35f));
        B->SetStyle(Style);
        if (Label) { B->AddChild(Text(Tree, FName(*(Name.ToString()+TEXT("Label"))), Label, 20)); }
        return B;
    }
    bool SetClass(UObject* Object, FName Property, UClass* Class)
    {
        FClassProperty* P = FindFProperty<FClassProperty>(Object->GetClass(), Property);
        if (!P) { return false; }
        P->SetObjectPropertyValue_InContainer(Object, Class); return true;
    }
    UStaticMesh* MakeBullet(UMaterial* Material)
    {
        UStaticMesh* Mesh = NewObject<UStaticMesh>(CreatePackage(TEXT("/Game/Weapons/SM_ShooterBullet")),
            TEXT("SM_ShooterBullet"), RF_Public | RF_Standalone);
        FMeshDescription Desc; FStaticMeshAttributes Attr(Desc); Attr.Register();
        auto Positions = Attr.GetVertexPositions();
        auto Normals = Attr.GetVertexInstanceNormals(); auto UV = Attr.GetVertexInstanceUVs(); UV.SetNumChannels(1);
        const FPolygonGroupID Group = Desc.CreatePolygonGroup();
        Attr.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Bullet");
        // 沿 X 轴旋转截面生成低面数弹头：8 cm 长、直径 1.3 cm，方便近距离观察。
        const FVector2f Profile[] = {{-4,0},{-4,0.58f},{-3.6f,0.65f},{1.2f,0.65f},{2.6f,0.45f},{3.6f,0.18f},{4,0}};
        constexpr int32 Sides = 16;
        for (int32 Ring = 0; Ring < UE_ARRAY_COUNT(Profile)-1; ++Ring)
        {
            for (int32 Side = 0; Side < Sides; ++Side)
            {
                TArray<FVertexInstanceID> IDs;
                FVector3f Points[4];
                for (int32 Corner = 0; Corner < 4; ++Corner)
                {
                    const int32 R = Ring + ((Corner == 1 || Corner == 2) ? 1 : 0);
                    const float Angle = 2*PI*(Side + (Corner >= 2 ? 1 : 0))/Sides;
                    Points[Corner] = FVector3f(Profile[R].X, Profile[R].Y*FMath::Cos(Angle), Profile[R].Y*FMath::Sin(Angle));
                }
                // 反转顺序使表面朝外；端部零半径通过三角形收口，避免退化四边形。
                TArray<int32> Corners = Ring == 0 ? TArray<int32>{0,2,1} :
                    Ring == UE_ARRAY_COUNT(Profile)-2 ? TArray<int32>{0,3,1} : TArray<int32>{0,3,2,1};
                const FVector3f Normal = FVector3f::CrossProduct(Points[Corners[1]]-Points[Corners[0]], Points[Corners[2]]-Points[Corners[0]]).GetSafeNormal();
                for (int32 C : Corners)
                {
                    const FVertexID V = Desc.CreateVertex(); Positions[V] = Points[C];
                    const FVertexInstanceID I = Desc.CreateVertexInstance(V);
                    Normals[I] = Normal; UV.Set(I,0,FVector2f((Points[C].X+4)/8,static_cast<float>(Side)/Sides)); IDs.Add(I);
                }
                Desc.CreatePolygon(Group, IDs);
            }
        }
        Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Bullet")));
        UStaticMesh::FBuildMeshDescriptionsParams Params; Params.bBuildSimpleCollision = false; Params.bFastBuild = false;
        if (!Mesh->BuildFromMeshDescriptions({&Desc}, Params)) { return nullptr; }
        return Mesh;
    }
}
bool ShooterRoundTools::Migrate(FString& Result)
{
    UBlueprint* GM = LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/Mygame_GM.Mygame_GM"));
    UBlueprint* PC = LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/BP_ShooterPlayerController.BP_ShooterPlayerController"));
    UBlueprint* Player = LoadObject<UBlueprint>(nullptr,TEXT("/Game/Blueprints/Shooter.Shooter"));
    if (!GM || !PC || !Player || GM->ParentClass != AGameModeBase::StaticClass()) { Result=TEXT("Unexpected T06 baseline or T07 already migrated."); return false; }
    const TCHAR* MenuPath = TEXT("/Game/UI/WBP_ShooterMenu");
    const TCHAR* AmmoPath = TEXT("/Game/UI/WBP_ShooterAmmo");
    for (const TCHAR* Path : {MenuPath,AmmoPath,TEXT("/Game/UI/T_ShooterStartMenu"),TEXT("/Game/Weapons/SM_ShooterBullet"),TEXT("/Game/Weapons/M_ShooterBullet"),TEXT("/Game/Weapons/BP_ShooterBulletVisual")})
    {
        if (FPackageName::DoesPackageExist(Path)) { Result=FString::Printf(TEXT("Refusing to overwrite %s"),Path); return false; }
    }
    UTextureFactory* Factory = NewObject<UTextureFactory>(); Factory->AutomatedImportData = NewObject<UAutomatedAssetImportData>();
    const FString ImageFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("SourceArt/UI/BeginPlay_reference.jpg"));
    UTexture2D* Texture = Cast<UTexture2D>(UFactory::StaticImportObject(UTexture2D::StaticClass(),
        CreatePackage(TEXT("/Game/UI/T_ShooterStartMenu")),TEXT("T_ShooterStartMenu"),RF_Public|RF_Standalone,*ImageFile,nullptr,Factory));
    if (!Texture) { Result=TEXT("Menu image import failed."); return false; }
    Texture->LODGroup=TEXTUREGROUP_UI; Texture->CompressionSettings=TC_EditorIcon;
    Texture->MipGenSettings=TMGS_NoMipmaps; Texture->NeverStream=true; Texture->PostEditChange();

    UWidgetBlueprint* Menu = NewWidget(MenuPath,UShooterMenuWidget::StaticClass());
    UWidgetTree* Tree = Menu->WidgetTree;
    UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(); Tree->RootWidget=Root;
    UBorder* Shade = Tree->ConstructWidget<UBorder>(); Shade->SetBrushColor(FLinearColor(0.018f,0.025f,0.038f,0.97f)); Fill(Root,Shade);
    UScaleBox* Scale = Tree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit); Fill(Root,Scale,FMargin(24));
    USizeBox* StartSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("StartPanel")); StartSize->bIsVariable=true;
    StartSize->SetWidthOverride(586); StartSize->SetHeightOverride(1024); Scale->AddChild(StartSize);
    UCanvasPanel* Start = Tree->ConstructWidget<UCanvasPanel>(); StartSize->AddChild(Start);
    UImage* Image = Tree->ConstructWidget<UImage>(); Image->SetBrushFromTexture(Texture); Place(Start,Image,{0,0},{586,1024});
    Place(Start,Button(Tree,TEXT("StartButton")),{142,429},{305,81});
    Place(Start,Button(Tree,TEXT("EndButton")),{142,544},{305,80});
    Place(Start,Button(Tree,TEXT("QuitButton")),{142,656},{305,82});
    UTextBlock* Hint=Text(Tree,TEXT("MenuHint"),TEXT("START GAME"),12,FLinearColor(0.75f,0.84f,0.9f));
    auto* HintSlot=Place(Root,Hint,{0,-22},{900,20}); HintSlot->SetAnchors(FAnchors(0.5f,1)); HintSlot->SetAlignment({0.5f,1}); Hint->SetJustification(ETextJustify::Center);
    UCanvasPanel* ResultPanel=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("ResultPanel")); ResultPanel->bIsVariable=true;
    auto* ResultSlot=Place(Root,ResultPanel,{0,0},{560,390}); ResultSlot->SetAnchors(FAnchors(0.5f)); ResultSlot->SetAlignment({0.5f,0.5f});
    UBorder* Card=Tree->ConstructWidget<UBorder>(); Card->SetBrushColor(FLinearColor(0.035f,0.06f,0.085f)); Fill(ResultPanel,Card);
    UTextBlock* Title=Text(Tree,TEXT("ResultTitle"),TEXT("VICTORY"),42); Title->SetJustification(ETextJustify::Center); Place(ResultPanel,Title,{30,30},{500,65});
    Place(ResultPanel,Button(Tree,TEXT("RestartButton"),TEXT("RESTART")),{80,135},{400,56});
    Place(ResultPanel,Button(Tree,TEXT("MenuButton"),TEXT("MAIN MENU")),{80,210},{400,56});
    Place(ResultPanel,Button(Tree,TEXT("ResultQuitButton"),TEXT("QUIT GAME")),{80,285},{400,56});
    ResultPanel->SetVisibility(ESlateVisibility::Collapsed);

    UWidgetBlueprint* Ammo=NewWidget(AmmoPath,UShooterAmmoWidget::StaticClass()); Tree=Ammo->WidgetTree;
    Root=Tree->ConstructWidget<UCanvasPanel>(); Tree->RootWidget=Root;
    UCanvasPanel* Panel=Tree->ConstructWidget<UCanvasPanel>();
    auto* PanelSlot=Place(Root,Panel,{-32,-28},{310,114}); PanelSlot->SetAnchors(FAnchors(1,1)); PanelSlot->SetAlignment({1,1});
    UBorder* Back=Tree->ConstructWidget<UBorder>(); Back->SetBrushColor(FLinearColor(0.018f,0.04f,0.055f,0.88f)); Fill(Panel,Back);
    Place(Panel,Text(Tree,TEXT("AmmoLabel"),TEXT("AMMUNITION"),12,FLinearColor(0.3f,0.75f,0.9f)),{20,12},{270,20});
    Place(Panel,Text(Tree,TEXT("AmmoValue"),TEXT("30 / 090"),30),{20,34},{270,43});
    Place(Panel,Text(Tree,TEXT("AmmoStatus"),TEXT("R  RELOAD  |  ESC  MENU"),11),{20,83},{280,20});
    if (!Compile(Menu) || !Compile(Ammo)) { Result=TEXT("Widget compile failed."); return false; }

    UMaterial* Material=NewObject<UMaterial>(CreatePackage(TEXT("/Game/Weapons/M_ShooterBullet")),TEXT("M_ShooterBullet"),RF_Public|RF_Standalone);
    auto* Gold=NewObject<UMaterialExpressionConstant3Vector>(Material); Gold->Constant=FLinearColor(0.9f,0.4f,0.04f);
    auto* Metal=NewObject<UMaterialExpressionConstant>(Material); Metal->R=0.7f;
    Material->GetExpressionCollection().AddExpression(Gold); Material->GetExpressionCollection().AddExpression(Metal);
    Material->GetEditorOnlyData()->BaseColor.Expression=Gold;
    Material->GetEditorOnlyData()->EmissiveColor.Expression=Gold;
    Material->GetEditorOnlyData()->Metallic.Expression=Metal;
    Material->PostEditChange();
    UStaticMesh* Mesh=MakeBullet(Material); if (!Mesh) { Result=TEXT("Bullet mesh build failed."); return false; }
    UBlueprint* Bullet=FKismetEditorUtilities::CreateBlueprint(AShooterBulletVisual::StaticClass(),
        CreatePackage(TEXT("/Game/Weapons/BP_ShooterBulletVisual")),TEXT("BP_ShooterBulletVisual"),BPTYPE_Normal,
        UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass(),TEXT("ShooterRound"));
    if (!Compile(Bullet)) { Result=TEXT("Bullet blueprint compile failed."); return false; }
    CastChecked<AShooterBulletVisual>(Bullet->GeneratedClass->GetDefaultObject())->GetBulletMesh()->SetStaticMesh(Mesh);
    if (!SetClass(PC->GeneratedClass->GetDefaultObject(),TEXT("AmmoWidgetClass"),Ammo->GeneratedClass)
        || !SetClass(PC->GeneratedClass->GetDefaultObject(),TEXT("MenuWidgetClass"),Menu->GeneratedClass)
        || !SetClass(CastChecked<AMyShooter>(Player->GeneratedClass->GetDefaultObject())->GetShooterWeapon(),TEXT("BulletVisualClass"),Bullet->GeneratedClass))
    { Result=TEXT("Class configuration missing."); return false; }
    // 只更换 GameMode 父类与新增默认资源引用；现有射击/换弹图节点完全保留。
    GM->ParentClass=AShooterGameMode::StaticClass(); FBlueprintEditorUtils::RefreshAllNodes(GM);
    if (!Compile(GM) || !Compile(PC) || !Compile(Player)) { Result=TEXT("Gameplay blueprint compile failed."); return false; }
    for (UObject* Asset : TArray<UObject*>{Texture,Menu,Ammo,Material,Mesh,Bullet,GM,PC,Player})
    {
        if (Asset!=GM && Asset!=PC && Asset!=Player) { FAssetRegistryModule::AssetCreated(Asset); }
        if (!Save(Asset)) { Result=FString::Printf(TEXT("Save failed: %s"),*Asset->GetName()); return false; }
    }
    Result=TEXT("T07 saved: menu image and hit targets, result page, ammo HUD, native GameMode, cosmetic bullet mesh/material/Blueprint. Existing graph nodes retained.");
    return true;
}
