#include "ShooterAIHUDTools.h"
#include "AI/ShooterAIController.h"
#include "Player/ShooterPlayerController.h"
#include "UI/ShooterHealthWidget.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameModeBase.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
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
    bool SaveAsset(UObject* Asset)
    {
        UPackage* Package = Asset->GetOutermost();
        Package->MarkPackageDirty();
        const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *File, Args);
    }
    bool Compile(UBlueprint* BP, FString& Result)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
        if (Log.NumErrors || BP->Status == BS_Error)
        {
            Result = FString::Printf(TEXT("Blueprint %s has %d errors; migration not saved."), *BP->GetName(), Log.NumErrors);
            return false;
        }
        return true;
    }
    UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, FVector2D Position, FVector2D Size)
    {
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
        Slot->SetPosition(Position);
        Slot->SetSize(Size);
        return Slot;
    }
}

bool ShooterAIHUDTools::Migrate(FString& Result)
{
    UBlueprint* AI = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/Boot_Shooter_controller.Boot_Shooter_controller"));
    UBlueprint* GM = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/Mygame_GM.Mygame_GM"));
    if (!AI || !GM || AI->ParentClass != AAIController::StaticClass()
        || AI->UbergraphPages.Num() != 1 || !GM->GeneratedClass->IsChildOf(AGameModeBase::StaticClass()))
    {
        Result = TEXT("Unexpected AI/GameMode baseline or migration already applied."); return false;
    }
    const TCHAR* TexturePath = TEXT("/Game/UI/T_ShooterHealthFrame");
    const TCHAR* WidgetPath = TEXT("/Game/UI/WBP_ShooterHealthBar");
    const TCHAR* ControllerPath = TEXT("/Game/Blueprints/BP_ShooterPlayerController");
    for (const TCHAR* Path : { TexturePath, WidgetPath, ControllerPath })
    {
        if (FPackageName::DoesPackageExist(Path)) { Result = FString::Printf(TEXT("Refusing to overwrite existing %s"), Path); return false; }
    }
    UEdGraph* Graph = AI->UbergraphPages[0];
    UEdGraphNode* Begin = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node->NodeGuid.ToString() == TEXT("9AE6AE7C4F6E9B31C818568D4E07C93D")) { Begin = Node; }
    }
    if (!Begin || !Begin->FindPin(TEXT("then"))) { Result = TEXT("Original AI BeginPlay missing."); return false; }
    AGameModeBase* GameMode = Cast<AGameModeBase>(GM->GeneratedClass->GetDefaultObject());
    if (!GameMode || GameMode->PlayerControllerClass != APlayerController::StaticClass())
    {
        Result = TEXT("GameMode has custom player controller; refusing to replace it blindly."); return false;
    }

    // 原图保存在 SourceArt；导入清理后的透明底框，所有数字和填充由实时 UMG 控件绘制。
    UTextureFactory* Factory = NewObject<UTextureFactory>();
    Factory->AutomatedImportData = NewObject<UAutomatedAssetImportData>();
    const FString Source = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("SourceArt/UI/ShooterHealthFrame.png"));
    UTexture2D* Texture = Cast<UTexture2D>(UFactory::StaticImportObject(UTexture2D::StaticClass(),
        CreatePackage(TexturePath), TEXT("T_ShooterHealthFrame"), RF_Public | RF_Standalone, *Source, nullptr, Factory));
    if (!Texture) { Result = TEXT("HUD texture import failed."); return false; }
    Texture->LODGroup = TEXTUREGROUP_UI;
    Texture->CompressionSettings = TC_EditorIcon;
    Texture->MipGenSettings = TMGS_NoMipmaps;
    Texture->NeverStream = true;
    Texture->SRGB = true;
    Texture->PostEditChange();
    FAssetRegistryModule::AssetCreated(Texture);

    UWidgetBlueprintFactory* WidgetFactory = NewObject<UWidgetBlueprintFactory>();
    WidgetFactory->ParentClass = UShooterHealthWidget::StaticClass();
    UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(WidgetFactory->FactoryCreateNew(UWidgetBlueprint::StaticClass(),
        CreatePackage(WidgetPath), TEXT("WBP_ShooterHealthBar"), RF_Public | RF_Standalone, nullptr, GWarn));
    if (!WidgetBP || !WidgetBP->WidgetTree) { Result = TEXT("Widget Blueprint creation failed."); return false; }
    UWidgetTree* Tree = WidgetBP->WidgetTree;
    UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScreenRoot"));
    Tree->RootWidget = Root;
    UCanvasPanel* Panel = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HealthPanel"));
    UCanvasPanelSlot* PanelSlot = Place(Root, Panel, FVector2D(32, -24), FVector2D(480, 160));
    PanelSlot->SetAnchors(FAnchors(0, 1));
    PanelSlot->SetAlignment(FVector2D(0, 1));
    UImage* Frame = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HealthFrame"));
    Frame->SetBrushFromTexture(Texture, false);
    Place(Panel, Frame, FVector2D::ZeroVector, FVector2D(480, 160));
    UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthLabel"));
    Label->SetText(FText::FromString(TEXT("HEALTH")));
    FSlateFontInfo SmallFont = Label->GetFont(); SmallFont.Size = 11;
    Label->SetFont(SmallFont);
    Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.8f, 0.87f)));
    Place(Panel, Label, FVector2D(120, 47), FVector2D(235, 16));
    UTextBlock* Value = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthValue"));
    Value->bIsVariable = true;
    Value->SetText(FText::FromString(TEXT("100 / 100")));
    FSlateFontInfo ValueFont = Value->GetFont(); ValueFont.Size = 20; ValueFont.TypefaceFontName = TEXT("Bold");
    Value->SetFont(ValueFont);
    Value->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Place(Panel, Value, FVector2D(120, 64), FVector2D(235, 28));
    UProgressBar* Fill = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthFill"));
    Fill->bIsVariable = true;
    Fill->SetPercent(1.0f);
    Fill->SetBarFillStyle(EProgressBarFillStyle::Scale);
    FProgressBarStyle Style = Fill->GetWidgetStyle();
    Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.035f, 0.045f, 0.06f));
    Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
    Fill->SetWidgetStyle(Style);
    Fill->SetFillColorAndOpacity(FLinearColor(0.02f, 0.7f, 1.0f));
    Place(Panel, Fill, FVector2D(120, 97), FVector2D(240, 9));
    if (!Compile(WidgetBP, Result)) { return false; }
    FAssetRegistryModule::AssetCreated(WidgetBP);

    UBlueprint* PC = FKismetEditorUtilities::CreateBlueprint(AShooterPlayerController::StaticClass(),
        CreatePackage(ControllerPath), TEXT("BP_ShooterPlayerController"), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("ShooterAIHUD"));
    if (!PC || !Compile(PC, Result)) { return false; }
    FClassProperty* WidgetClass = FindFProperty<FClassProperty>(AShooterPlayerController::StaticClass(), TEXT("HealthWidgetClass"));
    if (!WidgetClass) { Result = TEXT("HealthWidgetClass property missing."); return false; }
    WidgetClass->SetObjectPropertyValue_InContainer(PC->GeneratedClass->GetDefaultObject(), WidgetBP->GeneratedClass);
    FAssetRegistryModule::AssetCreated(PC);

    // 仅断开旧循环的执行入口，保留 AI MoveTo、Delay 和所有内部连线便于对比。
    Begin->FindPin(TEXT("then"))->BreakAllPinLinks();
    FGraphNodeCreator<UEdGraphNode_Comment> Creator(*Graph);
    UEdGraphNode_Comment* Comment = Creator.CreateNode(); Creator.Finalize();
    Comment->NodeComment = TEXT("旧蓝图追踪保留对比：BeginPlay 入口已断开。当前由 C++ ShooterAIController 的定时决策、寻路和 GAS 攻击驱动。");
    Comment->NodePosX = -180; Comment->NodePosY = -330;
    Comment->NodeWidth = 1130; Comment->NodeHeight = 820;
    Comment->CommentColor = FLinearColor(0.55f, 0.3f, 0.08f);
    AI->ParentClass = AShooterAIController::StaticClass();
    FBlueprintEditorUtils::RefreshAllNodes(AI);
    if (!Compile(AI, Result)) { return false; }
    GameMode->PlayerControllerClass = PC->GeneratedClass;
    FBlueprintEditorUtils::MarkBlueprintAsModified(GM);
    if (!Compile(GM, Result)) { return false; }
    for (UObject* Asset : TArray<UObject*>{Texture, WidgetBP, PC, AI, GM})
    {
        if (!SaveAsset(Asset)) { Result = FString::Printf(TEXT("Saving failed: %s"), *Asset->GetName()); return false; }
    }
    Result = TEXT("T06 assets saved: AI C++ parent with legacy nodes retained; screen health Widget Blueprint and PlayerController; original DefaultPawn and crosshair untouched.");
    return true;
}
