#include "ShooterAIHUDTools.h"
#include "AI/ShooterAIController.h"
#include "Player/ShooterPlayerController.h"
#include "UI/ShooterHealthWidget.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "TextureCompiler.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/App.h"
#include "UObject/UnrealType.h"

namespace
{
    bool RenderPreview(UShooterHealthWidget* Widget, const FString& Name, FVector2D Size)
    {
        // 直接渲染实际 UMG 布局；背景只用于检查透明边缘，不是游戏内新增遮挡层。
        TSharedRef<SWidget> Slate = SNew(SOverlay)
            + SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.045f,0.065f,0.08f))]
            + SOverlay::Slot()[Widget->TakeWidget()];
        FWidgetRenderer Renderer(true);
        UTextureRenderTarget2D* Target = Renderer.DrawWidget(Slate, Size);
        if (!Target) { return false; }
        FlushRenderingCommands();
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags ReadFlags; ReadFlags.SetLinearToGamma(false);
        if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags)) { return false; }
        TArray<uint8> PNG;
        FImageUtils::CompressImageArray(static_cast<int32>(Size.X), static_cast<int32>(Size.Y), Pixels, PNG);
        return FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir() / TEXT("T06") / Name));
    }
}

bool ShooterAIHUDTools::Verify(FString& Result)
{
    if (!FApp::CanEverRender()) { Result = TEXT("Use -AllowCommandletRendering without NullRHI."); return false; }
    // Commandlet 不自动创建 Slate；允许 RHI 渲染后仍需显式初始化 UI 应用。
    if (!FSlateApplication::IsInitialized())
    {
        FSlateApplication::InitializeAsStandaloneApplication(
            FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer());
    }
    UBlueprint* AIAsset = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/Boot_Shooter_controller.Boot_Shooter_controller"));
    UBlueprint* EnemyAsset = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/Boot_Shooter_BP.Boot_Shooter_BP"));
    UBlueprint* ShooterAsset = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/Shooter.Shooter"));
    UBlueprint* PCAsset = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/BP_ShooterPlayerController.BP_ShooterPlayerController"));
    UBlueprint* GMAsset = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprints/Mygame_GM.Mygame_GM"));
    UClass* WidgetClass = LoadClass<UShooterHealthWidget>(nullptr, TEXT("/Game/UI/WBP_ShooterHealthBar.WBP_ShooterHealthBar_C"));
    if (!AIAsset || AIAsset->ParentClass != AShooterAIController::StaticClass() || !EnemyAsset || !ShooterAsset
        || !PCAsset || !GMAsset || !WidgetClass)
    {
        Result = TEXT("Saved AI/HUD assets missing or not reparented."); return false;
    }
    AGameModeBase* GM = Cast<AGameModeBase>(GMAsset->GeneratedClass->GetDefaultObject());
    FClassProperty* ClassProperty = FindFProperty<FClassProperty>(AShooterPlayerController::StaticClass(), TEXT("HealthWidgetClass"));
    if (!GM || GM->PlayerControllerClass != PCAsset->GeneratedClass || GM->DefaultPawnClass != ShooterAsset->GeneratedClass
        || !ClassProperty || ClassProperty->GetObjectPropertyValue_InContainer(PCAsset->GeneratedClass->GetDefaultObject()) != WidgetClass)
    {
        Result = TEXT("Saved GameMode/player controller/HUD class references wrong."); return false;
    }
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) { Result = TEXT("Cannot create verification world."); return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 SavedFrame = GFrameCounter;
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter = SavedFrame;
    };
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance = [World](float Seconds)
    {
        while (Seconds > KINDA_SMALL_NUMBER)
        {
            const float Step = FMath::Min(Seconds, 0.02f);
            ++GFrameCounter; World->Tick(LEVELTICK_All, Step); Seconds -= Step;
        }
    };
    Advance(0.01f);
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AMyShooter* Player = World->SpawnActor<AMyShooter>(ShooterAsset->GeneratedClass, FVector(180,0,0), FRotator::ZeroRotator, Params);
    AShooterCharacterBase* Enemy = World->SpawnActor<AShooterCharacterBase>(EnemyAsset->GeneratedClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Player || !Enemy) { Result = TEXT("Saved character spawn failed."); return false; }
    Player->GetCharacterMovement()->DisableMovement(); Enemy->GetCharacterMovement()->DisableMovement();
    UShooterHealthWidget* Widget = CreateWidget<UShooterHealthWidget>(World, WidgetClass);
    if (!Widget) { Result = TEXT("Cannot create health widget."); return false; }
    // 离屏测试也必须持有 Slate 根节点，防止临时引用释放触发 NativeDestruct/重新绑定。
    const TSharedRef<SWidget> LiveWidget = Widget->TakeWidget();
    Widget->ObserveCharacter(Player);
    UImage* Frame = Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("HealthFrame")));
    UTexture2D* Texture = Frame ? Cast<UTexture2D>(Frame->GetBrush().GetResourceObject()) : nullptr;
    if (!Texture || Texture->GetSizeX() < 1)
    {
        Result = TEXT("Health frame texture was not saved in the widget brush."); return false;
    }
    UTexture* RequiredTextures[] = { Texture };
    FTextureCompilingManager::Get().FinishCompilation(RequiredTextures);
    Texture->UpdateResource();
    FlushRenderingCommands();
    ON_SCOPE_EXIT { Widget->ObserveCharacter(nullptr); };
    UProgressBar* Bar = Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("HealthFill")));
    UTextBlock* Text = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("HealthValue")));
    UCanvasPanel* Panel = Cast<UCanvasPanel>(Widget->WidgetTree->FindWidget(TEXT("HealthPanel")));
    UCanvasPanelSlot* Slot = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr;
    if (!Bar || !Text || !Slot || Slot->GetAnchors().Minimum != FVector2D(0,1)
        || Bar->GetPercent() != 1.0f || Text->GetText().ToString() != TEXT("100 / 100"))
    {
        Result = TEXT("Actual health controls, initial values or screen anchors incorrect."); return false;
    }
    if (!RenderPreview(Widget, TEXT("HUD-100.png"), FVector2D(1280,720))) { Result = TEXT("HUD render failed."); return false; }

    // 使用真实敌人蓝图配置的 AIControllerClass，不在验证中手动替换控制器。
    Enemy->SpawnDefaultController();
    Enemy->GetCharacterMovement()->DisableMovement();
    AShooterAIController* AI = Cast<AShooterAIController>(Enemy->GetController());
    if (!AI || AI->GetClass() != AIAsset->GeneratedClass) { Result = TEXT("Enemy did not use saved AI controller."); return false; }
    AI->SetCombatTarget(Player);
    const float Before = Player->GetGASHealth();
    Advance(0.62f);
    if (Player->GetGASHealth() != Before - 10 || Widget->GetDisplayedHealth() != Before - 10 || Bar->GetPercent() != 0.9f)
    {
        Result = FString::Printf(TEXT("Real AI -> GAS -> HUD mismatch: HP %.1f display %.1f"), Player->GetGASHealth(), Widget->GetDisplayedHealth());
        return false;
    }
    AActor* Source = World->SpawnActor<AActor>();
    UShooterDamageLibrary::ApplyGASDamage(Source, Player, 65, Source, FHitResult());
    if (Widget->GetDisplayedHealth() != 25 || Bar->GetPercent() != 0.25f
        || Text->GetText().ToString() != TEXT("25 / 100"))
    {
        Result = TEXT("Low health did not update actual UMG controls."); return false;
    }
    if (!RenderPreview(Widget, TEXT("HUD-25.png"), FVector2D(1280,720))) { Result = TEXT("Low-health render failed."); return false; }
    UShooterDamageLibrary::ApplyGASDamage(Source, Player, 100, Source, FHitResult());
    if (Widget->GetDisplayedHealth() != 0 || Bar->GetPercent() != 0 || AI->GetCombatTarget())
    {
        Result = TEXT("Death failed to clear AI target or health display."); return false;
    }
    if (!RenderPreview(Widget, TEXT("HUD-0.png"), FVector2D(1920,1080))) { Result = TEXT("Dead HUD render failed."); return false; }
    Advance(0.5f);
    Result = TEXT("PASS T06: saved GameMode/PC/widget references; real enemy blueprint AI deals GAS damage; actual UMG initial/90/25/0 health; player death clears target; screen-anchored HUD rendered at 720p and 1080p.");
    return true;
}
