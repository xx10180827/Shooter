#include "ShooterRoundTools.h"
#include "Game/ShooterGameMode.h"
#include "UI/ShooterAmmoWidget.h"
#include "UI/ShooterMenuWidget.h"
#include "Player/ShooterPlayerController.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterBulletVisual.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "AssetCompilingManager.h"
#include "ObjectTools.h"
#include "Misc/ObjectThumbnail.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "TextureCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SOverlay.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/App.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"

namespace
{
    bool Render(UUserWidget* Widget,const FString& Name,FVector2D Size)
    {
        FWidgetRenderer Renderer(false);
        UTextureRenderTarget2D* Target=Renderer.DrawWidget(Widget->TakeWidget(),Size);
        if (!Target) { return false; }
        FlushRenderingCommands();
        TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
        if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)) { return false; }
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
        return FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("T07")/Name));
    }
}
bool ShooterRoundTools::Verify(FString& Result)
{
    if (!FApp::CanEverRender()) { Result=TEXT("Use -AllowCommandletRendering without NullRHI."); return false; }
    if (!FSlateApplication::IsInitialized())
    {
        FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer());
    }
    UClass* GMClass=LoadClass<AShooterGameMode>(nullptr,TEXT("/Game/Blueprints/Mygame_GM.Mygame_GM_C"));
    UClass* PCClass=LoadClass<AShooterPlayerController>(nullptr,TEXT("/Game/Blueprints/BP_ShooterPlayerController.BP_ShooterPlayerController_C"));
    UClass* PlayerClass=LoadClass<AMyShooter>(nullptr,TEXT("/Game/Blueprints/Shooter.Shooter_C"));
    UClass* MenuClass=LoadClass<UShooterMenuWidget>(nullptr,TEXT("/Game/UI/WBP_ShooterMenu.WBP_ShooterMenu_C"));
    UClass* AmmoClass=LoadClass<UShooterAmmoWidget>(nullptr,TEXT("/Game/UI/WBP_ShooterAmmo.WBP_ShooterAmmo_C"));
    UClass* BulletClass=LoadClass<AShooterBulletVisual>(nullptr,TEXT("/Game/Weapons/BP_ShooterBulletVisual.BP_ShooterBulletVisual_C"));
    if (!GMClass || !PCClass || !PlayerClass || !MenuClass || !AmmoClass || !BulletClass) { Result=TEXT("T07 saved classes missing."); return false; }
    auto ClassValue=[](UObject* Object,const TCHAR* Name) -> UObject*
    {
        FClassProperty* Property=FindFProperty<FClassProperty>(Object->GetClass(),Name);
        return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
    };
    if (CastChecked<AGameModeBase>(GMClass->GetDefaultObject())->DefaultPawnClass!=PlayerClass
        || CastChecked<AGameModeBase>(GMClass->GetDefaultObject())->PlayerControllerClass!=PCClass
        || ClassValue(PCClass->GetDefaultObject(),TEXT("MenuWidgetClass"))!=MenuClass
        || ClassValue(PCClass->GetDefaultObject(),TEXT("AmmoWidgetClass"))!=AmmoClass
        || ClassValue(CastChecked<AMyShooter>(PlayerClass->GetDefaultObject())->GetShooterWeapon(),TEXT("BulletVisualClass"))!=BulletClass)
    { Result=TEXT("Saved GameMode/PC/weapon references incorrect."); return false; }
    AShooterBulletVisual* BulletCDO=CastChecked<AShooterBulletVisual>(BulletClass->GetDefaultObject());
    if (!BulletCDO->GetBulletMesh()->GetStaticMesh() || BulletCDO->GetBulletMesh()->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
    { Result=TEXT("Bullet mesh or collision configuration incorrect."); return false; }
    UStaticMesh* BulletMesh = BulletCDO->GetBulletMesh()->GetStaticMesh();
    UObject* RequiredAssets[] = { BulletMesh, BulletMesh->GetMaterial(0) };
    FAssetCompilingManager::Get().FinishCompilationForObjects(RequiredAssets);
    FObjectThumbnail Thumbnail;
    ThumbnailTools::RenderThumbnail(BulletMesh,512,512,ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush,nullptr,&Thumbnail);
    const TArray<uint8>& Raw = Thumbnail.GetUncompressedImageData();
    if (Raw.Num() == Thumbnail.GetImageWidth()*Thumbnail.GetImageHeight()*sizeof(FColor) && Raw.Num()>0)
    {
        TArray64<uint8> PNG;
        FImageUtils::PNGCompressImageArray(Thumbnail.GetImageWidth(),Thumbnail.GetImageHeight(),
            TArrayView64<const FColor>(reinterpret_cast<const FColor*>(Raw.GetData()),Raw.Num()/sizeof(FColor)),PNG);
        FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("T07/BulletModel.png")));
    }
    UTexture2D* Texture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/T_ShooterStartMenu.T_ShooterStartMenu"));
    UTexture* Textures[]={Texture}; if (!Texture) { return false; }
    FTextureCompilingManager::Get().FinishCompilation(Textures); Texture->UpdateResource(); FlushRenderingCommands();
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game); WorldContext.SetCurrentWorld(World);
    UGameInstance* GI = NewObject<UGameInstance>(GEngine); World->SetGameInstance(GI); WorldContext.OwningGameInstance = GI;
    World->GetWorldSettings()->DefaultGameMode = GMClass;
    if (!World->SetGameMode(FURL())) { Result = TEXT("Saved GameMode cannot start."); return false; }
    const uint64 SavedFrame=GFrameCounter;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter=SavedFrame; };
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    auto Advance=[World](float Seconds) { while(Seconds>KINDA_SMALL_NUMBER) { const float Step=FMath::Min(Seconds,0.02f); ++GFrameCounter; World->Tick(LEVELTICK_All,Step); Seconds-=Step; } };
    Advance(0.02f);
    AMyShooter* Player=World->SpawnActor<AMyShooter>(PlayerClass);
    if (!Player) { return false; }
    Player->GetCharacterMovement()->DisableMovement();
    UShooterAmmoWidget* Ammo=NewObject<UShooterAmmoWidget>(World,AmmoClass);
    UShooterMenuWidget* Menu=NewObject<UShooterMenuWidget>(World,MenuClass);
    if (!Ammo || !Menu) { return false; }
    // 独立验证世界没有本地玩家；显式以 World 为 Outer 初始化，避免未初始化的测试 GameInstance 返回空 World。
    Ammo->Initialize(); Menu->Initialize();
    if (Menu->GetWorld()!=World) { Result=TEXT("Widget world binding failed."); return false; }
    const TSharedRef<SWidget> AmmoSlate=Ammo->TakeWidget(); const TSharedRef<SWidget> MenuSlate=Menu->TakeWidget();
    ON_SCOPE_EXIT { Ammo->ObserveWeapon(nullptr); };
    Ammo->ObserveWeapon(Player->GetShooterWeapon());
    UTextBlock* Value=Cast<UTextBlock>(Ammo->WidgetTree->FindWidget(TEXT("AmmoValue")));
    if (!Value || Value->GetText().ToString()!=TEXT("30 / 090")) { Result=TEXT("Actual ammo initial text wrong."); return false; }
    Menu->ShowState(EShooterRoundState::Menu);
    if (!Render(Menu,TEXT("StartMenu-720.png"),{1280,720}) || !Render(Menu,TEXT("StartMenu-1080.png"),{1920,1080})) { return false; }
    UButton* End=Cast<UButton>(Menu->WidgetTree->FindWidget(TEXT("EndButton")));
    if (!End || End->GetIsEnabled()) { Result=TEXT("End button must be disabled before starting."); return false; }
    Menu->ShowState(EShooterRoundState::Paused);
    if (!End->GetIsEnabled()) { Result=TEXT("End button must be enabled when paused."); return false; }
    Menu->ShowState(EShooterRoundState::Won); if (!Render(Menu,TEXT("Victory-720.png"),{1280,720})) { return false; }
    Menu->ShowState(EShooterRoundState::Lost); if (!Render(Menu,TEXT("Defeat-720.png"),{1280,720})) { return false; }
    AShooterGameMode* GameMode = CastChecked<AShooterGameMode>(World->GetAuthGameMode());
    UButton* Start = Cast<UButton>(Menu->WidgetTree->FindWidget(TEXT("StartButton")));
    if (!Start || GameMode->GetRoundState() != EShooterRoundState::Menu || Player->GetShooterWeapon()->StartFiring()) { Result = TEXT("Initial menu gate failed."); return false; }
    Start->OnClicked.Broadcast();
    if (GameMode->GetRoundState() != EShooterRoundState::Playing) { Result = TEXT("Actual Start button failed."); return false; }
    GameMode->TogglePause(); Start->OnClicked.Broadcast();
    if (GameMode->GetRoundState() != EShooterRoundState::Playing) { Result = TEXT("Actual Continue button failed."); return false; }
    if (!Player->GetShooterWeapon()->StartFiring()) { Result=TEXT("Actual saved player cannot fire."); return false; }
    Player->GetShooterWeapon()->StopFiring();
    int32 Bullets=0;
    for (TActorIterator<AShooterBulletVisual> It(World); It; ++It) { ++Bullets; }
    if (Bullets!=1 || Value->GetText().ToString()!=TEXT("29 / 090")) { Result=TEXT("One shot must create one cosmetic bullet and consume one ammo."); return false; }
    Player->GetShooterWeapon()->StartReloading();
    if (!Render(Ammo,TEXT("Ammo-Reloading-720.png"),{1280,720})) { return false; }
    Advance(1.6f);
    if (Value->GetText().ToString()!=TEXT("30 / 089")) { Result=TEXT("Actual ammo UI did not refresh after reload."); return false; }
    if (!Render(Ammo,TEXT("Ammo-Ready-720.png"),{1280,720})) { return false; }
    Result=TEXT("PASS T07: saved GameMode, menu/ammo classes and bullet references; menu/result UMG render at 720p/1080p; one real player shot creates one no-collision bullet and consumes one ammo; reload text updates to 30/089.");
    return true;
}
