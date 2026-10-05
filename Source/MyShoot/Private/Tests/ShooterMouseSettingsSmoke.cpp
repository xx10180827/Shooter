#include "Tests/ShooterWeaponSmokeSubsystem.h"
#include "Settings/ShooterUserSettings.h"
#include "UI/ShooterMouseSettingsWidget.h"
#include "UI/ShooterMenuWidget.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerInput.h"
#include "Game/ShooterGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformTime.h"

bool UShooterWeaponSmokeSubsystem::StepMouseSettings(float)
{
    if(bFinished) { return false; }
    const double Now=FPlatformTime::Seconds(); if(Now>Deadline) { Finish(false,TEXT("Settings UI timeout")); return false; }
    if(Now<NextTime) { return true; }
    auto* PC=GetWorld()?Cast<AShooterPlayerController>(GetWorld()->GetFirstPlayerController()):nullptr;
    auto* UI=PC?PC->GetMouseSettingsWidget():nullptr; auto* Settings=UShooterUserSettings::Get();
    auto* GM=GetWorld()?Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()):nullptr;
    if(!UI||!UI->WidgetTree||!Settings||!GM||!PC->GetMenuWidget()) { return true; }
    auto Click=[&](const TCHAR* Name) { auto* B=Cast<UButton>(UI->WidgetTree->FindWidget(FName(Name))); if(!Check(B!=nullptr,TEXT("Settings button exists"))) { return false; } B->OnClicked.Broadcast(); return true; };
    switch(Phase)
    {
    case 0:
        UE_LOG(LogTemp,Display,TEXT("MouseSettings config path: %s"),*GGameUserSettingsIni);
        if(!Check(Cast<UShooterPlayerInput>(PC->PlayerInput)!=nullptr,TEXT("Real controller uses mouse-only input adapter"))) { return false; }
        if(FParse::Param(FCommandLine::Get(),TEXT("ShooterMouseSettingsVerify")))
        {
            if(!Check(Settings->GetHipSensitivity().Equals(FVector2D(.63,.63),.001)&&Settings->GetAimSensitivity(TEXT("Default")).Equals(FVector2D(.52,.52),.001),TEXT("Separate process reloads saved slider values"))) { return false; }
            Click(TEXT("SettingsButton")); Next(6,.5f); break;
        }
        Capture(TEXT("01-MenuEntry.png")); Next(1,.4f); break;
    case 1:
        if(!Click(TEXT("SettingsButton"))||!Check(UI->IsSettingsOpen()&&!PC->GetMenuWidget()->GetIsEnabled(),TEXT("Settings opens over start menu and blocks underlying buttons"))) { return false; }
        Click(TEXT("ResetMouseButton"));
        if(!Check(Settings->GetHipSensitivity().Equals(FVector2D(.8,.8))&&Settings->GetAimSensitivity(TEXT("Default")).Equals(FVector2D(.75,.75)),TEXT("Reset button restores mouse defaults"))) { return false; }
        Capture(TEXT("02-Defaults.png")); Next(2,.5f); break;
    case 2:
    {
        auto* Hip=Cast<USlider>(UI->WidgetTree->FindWidget(TEXT("HipSensitivitySlider"))); auto* Aim=Cast<USlider>(UI->WidgetTree->FindWidget(TEXT("AimSensitivitySlider")));
        if(!Check(Hip&&Aim,TEXT("Both sliders exist"))) { return false; }
        Hip->SetValue(.63f); Hip->OnValueChanged.Broadcast(.63f); Aim->SetValue(.52f); Aim->OnValueChanged.Broadcast(.52f);
        if(!Check(Settings->GetHipSensitivity().Equals(FVector2D(.63,.63),.001)&&Settings->GetAimSensitivity(TEXT("Default")).Equals(FVector2D(.52,.52),.001),TEXT("Slider callbacks apply X/Y and ADS immediately"))) { return false; }
        Capture(TEXT("03-Changed.png")); Hip->SetKeyboardFocus();
        FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
        if(!Check(!UI->IsSettingsOpen()&&GM->GetRoundState()==EShooterRoundState::Menu&&PC->GetMenuWidget()->GetIsEnabled(),TEXT("Escape from focused slider returns to menu without starting round"))) { return false; }
        GM->StartRound(); Next(3,.4f); break;
    }
    case 3:
        if(!Check(UI->GetVisibility()==ESlateVisibility::Collapsed,TEXT("Settings entry hidden during gameplay"))) { return false; }
        GM->TogglePause(); Click(TEXT("SettingsButton"));
        if(!Check(UI->IsSettingsOpen()&&GM->GetRoundState()==EShooterRoundState::Paused,TEXT("Pause menu opens settings while staying paused"))) { return false; }
        Capture(TEXT("04-PausedSettings.png")); Next(4,.4f); break;
    case 4:
        Click(TEXT("CloseSettingsButton"));
        if(!Check(!UI->IsSettingsOpen()&&GM->GetRoundState()==EShooterRoundState::Paused,TEXT("Back returns to paused menu"))) { return false; }
        GM->TogglePause(); Next(5,.3f); break;
    case 5:
        Finish(true,TEXT("Start/pause settings, sliders, reset, Escape and resume passed; saved values ready for restart verification")); return false;
    case 6:
        Capture(TEXT("05-RestartLoaded.png")); Next(7,.4f); break;
    case 7:
        Finish(true,TEXT("Settings persisted across separate game processes")); return false;
    }
    return true;
}