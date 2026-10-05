// 覆盖独立 X/Y、瞄具配置持久化，以及真实鼠标处理路径与后坐力/FOV 的隔离。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Settings/ShooterUserSettings.h"
#include "Player/ShooterPlayerInput.h"
#include "Player/ShooterPlayerController.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "Weapons/ShooterAimComponent.h"
#include "Game/ShooterGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMouseSettingsTest,"MyShoot.Settings.MouseSensitivity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShooterMouseSettingsTest::RunTest(const FString&)
{
    auto* Local=NewObject<UShooterUserSettings>(); Local->ResetMouseSettings();
    Local->SetHipSensitivity(FVector2D(.4,1.2)); Local->SetAimSensitivity(TEXT("Scope4x"),FVector2D(.3,.6));
    const FString File=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("T19_Mouse/TestSettings.ini"));
    Local->SaveConfig(CPF_Config,*File);
    auto* Loaded=NewObject<UShooterUserSettings>(); Loaded->LoadConfig(nullptr,*File);
    TestTrue(TEXT("Separate X/Y survive config reload"),Loaded->GetHipSensitivity().Equals(FVector2D(.4,1.2)));
    TestTrue(TEXT("Named aim profile survives config reload"),Loaded->GetAimSensitivity(TEXT("Scope4x")).Equals(FVector2D(.3,.6)));
    TestTrue(TEXT("Unknown profile falls back to default"),Loaded->GetAimSensitivity(TEXT("Unknown")).Equals(FVector2D(.75,.75)));
    Loaded->SetHipSensitivity(FVector2D(-10,100)); TestTrue(TEXT("Input values clamped"),Loaded->GetHipSensitivity().Equals(FVector2D(.1,3.)));
    Loaded->ResetMouseSettings(); TestTrue(TEXT("Reset restores 80 percent and default ADS"),Loaded->GetHipSensitivity().Equals(FVector2D(.8,.8))&&Loaded->GetAimSensitivity(TEXT("Scope4x")).Equals(FVector2D(.75,.75)));
    auto* Settings=UShooterUserSettings::Get(); if(!TestNotNull(TEXT("Engine uses custom user settings"),Settings)) { return false; }
    const auto SavedHip=Settings->GetHipSensitivity(),SavedAim=Settings->GetAimSensitivity(UShooterUserSettings::DefaultAimProfile());
    auto* World=UWorld::CreateWorld(EWorldType::Game,false); auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    auto* GI=NewObject<UGameInstance>(GEngine); World->SetGameInstance(GI); Context.OwningGameInstance=GI;
    const uint64 Frame=GFrameCounter;
    ON_SCOPE_EXIT { Settings->SetHipSensitivity(SavedHip); Settings->SetAimSensitivity(UShooterUserSettings::DefaultAimProfile(),SavedAim); World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter=Frame; };
    World->GetWorldSettings()->DefaultGameMode=AShooterGameMode::StaticClass(); World->SetGameMode(FURL()); World->InitializeActorsForPlay(FURL());
    auto* Definition=NewObject<UShooterWeaponDefinition>(); Definition->RecoilPitch=2.f; Definition->bAutomatic=false;
    auto* Player=World->SpawnActorDeferred<AMyShooter>(AMyShooter::StaticClass(),FTransform::Identity); Player->GetShooterWeapon()->ConfigureLoadout({Definition});
    auto* Camera=NewObject<UCameraComponent>(Player); Camera->SetupAttachment(Player->GetRootComponent()); Camera->SetFieldOfView(90); Camera->RegisterComponent(); Player->FinishSpawning(FTransform::Identity);
    auto* PC=World->SpawnActor<APlayerController>(); PC->SetPlayer(NewObject<ULocalPlayer>(GEngine)); PC->Possess(Player);
    auto* Input=NewObject<UShooterPlayerInput>(PC);
    World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay(); CastChecked<AShooterGameMode>(World->GetAuthGameMode())->StartRound(); Player->GetCharacterMovement()->DisableMovement();
    auto Advance=[&](float Time) { for(int32 I=0;I<FMath::CeilToInt(Time/.01f);++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.01f); } };
    Settings->SetHipSensitivity(FVector2D(1,1)); const float X=Input->MassageAxisInput(EKeys::MouseX,10),Y=Input->MassageAxisInput(EKeys::MouseY,10);
    TestTrue(TEXT("Mouse base input nonzero"),FMath::Abs(X)>.001f&&FMath::Abs(Y)>.001f);
    Settings->SetHipSensitivity(FVector2D(.4,1.2));
    TestTrue(TEXT("Mouse X scales independently"),FMath::IsNearlyEqual(Input->MassageAxisInput(EKeys::MouseX,10),X*.4f));
    TestTrue(TEXT("Mouse Y scales independently"),FMath::IsNearlyEqual(Input->MassageAxisInput(EKeys::MouseY,10),Y*1.2f));
    TestEqual(TEXT("Keyboard axis unchanged"),Input->MassageAxisInput(EKeys::W,1),1.f);
    Camera->SetFieldOfView(110); PC->PlayerCameraManager->SetFOV(110);
    TestTrue(TEXT("Camera FOV does not change sensitivity"),FMath::IsNearlyEqual(Input->MassageAxisInput(EKeys::MouseX,10),X*.4f));
    Settings->SetAimSensitivity(UShooterUserSettings::DefaultAimProfile(),FVector2D(.5,.25));
    TestTrue(TEXT("Aim can start"),Player->GetShooterAim()->StartAiming()); Advance(.25f);
    TestTrue(TEXT("ADS applies own X multiplier"),FMath::IsNearlyEqual(Input->MassageAxisInput(EKeys::MouseX,10),X*.4f*.5f));
    TestTrue(TEXT("ADS applies own Y multiplier"),FMath::IsNearlyEqual(Input->MassageAxisInput(EKeys::MouseY,10),Y*1.2f*.25f));
    Player->GetShooterAim()->ResetAiming();
    for(double Value:{.1,3.})
    {
        Settings->SetHipSensitivity(FVector2D(Value,Value)); PC->SetControlRotation(FRotator::ZeroRotator);
        TestTrue(TEXT("Real shot accepted at sensitivity extremes"),Player->GetShooterWeapon()->StartFiring()); Player->GetShooterWeapon()->StopFiring(); Advance(.2f);
        TestTrue(TEXT("Weapon recoil stays two degrees at all sensitivities"),FMath::IsNearlyEqual(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch),2.f,.02f));
    }
    TestEqual(TEXT("Project controller installs only mouse input adapter"),GetDefault<AShooterPlayerController>()->GetOverridePlayerInputClass().Get(),UShooterPlayerInput::StaticClass());
    return true;
}
#endif