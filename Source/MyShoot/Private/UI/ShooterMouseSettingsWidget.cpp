#include "UI/ShooterMouseSettingsWidget.h"
#include "Settings/ShooterUserSettings.h"
#include "Player/ShooterPlayerController.h"
#include "UI/ShooterMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"

TSharedRef<SWidget> UShooterMouseSettingsWidget::RebuildWidget()
{
    if(!WidgetTree->RootWidget)
    {
        auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("SettingsRoot"));
        WidgetTree->RootWidget=Root;
        auto Text=[&](const TCHAR* Value,int32 Size)
        {
            auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Value));
            T->SetFont(FCoreStyle::GetDefaultFontStyle("Regular",Size)); T->SetColorAndOpacity(FSlateColor(FLinearColor(.9,.96,1)));
            return T;
        };
        // 设置入口与模态面板独立于旧菜单蓝图，后续可以增加其他设置页。
        SettingsButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("SettingsButton"));
        SettingsButton->SetBackgroundColor(FLinearColor(.06,.20,.28,1));
        SettingsButton->AddChild(Text(TEXT("SETTINGS"),18));
        auto* Entry=Root->AddChildToCanvas(SettingsButton); Entry->SetAnchors(FAnchors(1,0));
        Entry->SetAlignment(FVector2D(1,0)); Entry->SetPosition(FVector2D(-28,28)); Entry->SetSize(FVector2D(166,44));
        Backdrop=WidgetTree->ConstructWidget<UBorder>(); Backdrop->SetBrushColor(FLinearColor(.015,.025,.04,.97)); Backdrop->SetPadding(FMargin(0));
        auto* Full=Root->AddChildToCanvas(Backdrop); Full->SetAnchors(FAnchors(0,0,1,1)); Full->SetOffsets(FMargin(0));
        auto* Panel=WidgetTree->ConstructWidget<UCanvasPanel>(); Backdrop->AddChild(Panel);
        auto* Card=WidgetTree->ConstructWidget<UBorder>(); Card->SetBrushColor(FLinearColor(.035,.07,.10,1)); Card->SetPadding(FMargin(28));
        auto* CardSlot=Panel->AddChildToCanvas(Card); CardSlot->SetAnchors(FAnchors(.5,.5)); CardSlot->SetAlignment(FVector2D(.5,.5)); CardSlot->SetSize(FVector2D(560,450));
        auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>(); Card->AddChild(Rows);
        Rows->AddChildToVerticalBox(Text(TEXT("MOUSE SETTINGS"),28))->SetPadding(FMargin(0,0,0,25));
        auto Row=[&](const TCHAR* Label,const TCHAR* Name,float Max,USlider*& Slider,UTextBlock*& Value)
        {
            auto* Labels=WidgetTree->ConstructWidget<UHorizontalBox>();
            auto* LabelSlot=Labels->AddChildToHorizontalBox(Text(Label,19)); LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            Value=Text(TEXT(""),19); Labels->AddChildToHorizontalBox(Value);
            Rows->AddChildToVerticalBox(Labels)->SetPadding(FMargin(0,0,0,12));
            Slider=WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),FName(Name));
            Slider->SetMinValue(.1f); Slider->SetMaxValue(Max); Slider->SetStepSize(.01f);
            Slider->SetSliderHandleColor(FLinearColor(.1,.85,1));
            Rows->AddChildToVerticalBox(Slider)->SetPadding(FMargin(0,0,0,28));
        };
        USlider* Hip=nullptr; USlider* Aim=nullptr; UTextBlock* HV=nullptr; UTextBlock* AV=nullptr;
        Row(TEXT("Mouse sensitivity"),TEXT("HipSensitivitySlider"),3.f,Hip,HV);
        Row(TEXT("Aim multiplier"),TEXT("AimSensitivitySlider"),2.f,Aim,AV);
        HipSlider=Hip; AimSlider=Aim; HipValue=HV; AimValue=AV;
        auto* Hint=Text(TEXT("Changes apply immediately and are saved automatically.\nESC / P: back to menu"),13);
        Rows->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0,0,0,28));
        auto* Buttons=WidgetTree->ConstructWidget<UHorizontalBox>(); Rows->AddChildToVerticalBox(Buttons);
        auto* Reset=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("ResetMouseButton")); Reset->AddChild(Text(TEXT("RESTORE DEFAULTS"),16));
        Reset->SetBackgroundColor(FLinearColor(.06,.20,.28,1));
        auto* ResetSlot=Buttons->AddChildToHorizontalBox(Reset); ResetSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ResetSlot->SetPadding(FMargin(0,0,14,0));
        auto* Back=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CloseSettingsButton")); Back->AddChild(Text(TEXT("BACK"),16));
        Back->SetBackgroundColor(FLinearColor(.06,.20,.28,1));
        Buttons->AddChildToHorizontalBox(Back)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Reset->OnClicked.AddUniqueDynamic(this,&UShooterMouseSettingsWidget::ResetClicked);
        Back->OnClicked.AddUniqueDynamic(this,&UShooterMouseSettingsWidget::CloseSettings);
    }
    return Super::RebuildWidget();
}
void UShooterMouseSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct(); SetIsFocusable(true);
    SettingsButton->OnClicked.AddUniqueDynamic(this,&UShooterMouseSettingsWidget::OpenSettings);
    HipSlider->OnValueChanged.AddUniqueDynamic(this,&UShooterMouseSettingsWidget::HipChanged);
    AimSlider->OnValueChanged.AddUniqueDynamic(this,&UShooterMouseSettingsWidget::AimChanged);
    RefreshValues(); Backdrop->SetVisibility(ESlateVisibility::Collapsed);
}
void UShooterMouseSettingsWidget::ShowForState(EShooterRoundState State)
{
    // 开始与暂停阶段提供入口，退出面板只返回所在菜单，不恢复对局。
    CurrentState=State;
    if(bOpen) { CloseSettings(); }
    const bool bAllowed=State==EShooterRoundState::Menu||State==EShooterRoundState::Paused;
    SetVisibility(bAllowed?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
}
void UShooterMouseSettingsWidget::OpenSettings()
{
    if(CurrentState!=EShooterRoundState::Menu&&CurrentState!=EShooterRoundState::Paused) { return; }
    bOpen=true; RefreshValues(); Backdrop->SetVisibility(ESlateVisibility::Visible); SettingsButton->SetVisibility(ESlateVisibility::Collapsed);
    if(auto* PC=Cast<AShooterPlayerController>(GetOwningPlayer()))
    {
        if(auto* Menu=PC->GetMenuWidget()) { Menu->SetIsEnabled(false); }
        FInputModeUIOnly Mode; Mode.SetWidgetToFocus(TakeWidget()); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(Mode);
    }
    SetKeyboardFocus();
}
void UShooterMouseSettingsWidget::CloseSettings()
{
    bOpen=false; Backdrop->SetVisibility(ESlateVisibility::Collapsed); SettingsButton->SetVisibility(ESlateVisibility::Visible);
    if(auto* PC=Cast<AShooterPlayerController>(GetOwningPlayer()))
    {
        if(auto* Menu=PC->GetMenuWidget()) { Menu->SetIsEnabled(true); if(CurrentState!=EShooterRoundState::Playing) { Menu->SetKeyboardFocus(); } }
    }
}
void UShooterMouseSettingsWidget::RefreshValues()
{
    const auto* Settings=UShooterUserSettings::Get(); if(!Settings) { return; }
    // 程序同步滑条不能再次触发保存，避免 UI 回调递归。
    TGuardValue<bool> Guard(bRefreshing,true);
    const float Hip=float(Settings->GetHipSensitivity().X), Aim=float(Settings->GetAimSensitivity(UShooterUserSettings::DefaultAimProfile()).X);
    HipSlider->SetValue(Hip); AimSlider->SetValue(Aim);
    HipValue->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"),Hip)));
    AimValue->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"),Aim)));
}
void UShooterMouseSettingsWidget::HipChanged(float Value)
{
    if(bRefreshing) { return; }
    if(auto* Settings=UShooterUserSettings::Get()) { Settings->SetHipSensitivity(FVector2D(Value,Value)); Settings->SaveSettings(); RefreshValues(); }
}
void UShooterMouseSettingsWidget::AimChanged(float Value)
{
    if(bRefreshing) { return; }
    if(auto* Settings=UShooterUserSettings::Get()) { Settings->SetAimSensitivity(UShooterUserSettings::DefaultAimProfile(),FVector2D(Value,Value)); Settings->SaveSettings(); RefreshValues(); }
}
void UShooterMouseSettingsWidget::ResetClicked()
{
    if(auto* Settings=UShooterUserSettings::Get()) { Settings->ResetMouseSettings(); Settings->SaveSettings(); RefreshValues(); }
}
FReply UShooterMouseSettingsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    // Slider 拥有焦点时也先收到预览事件，防止 Esc 直接恢复战斗。
    if(bOpen&&(Event.GetKey()==EKeys::Escape||Event.GetKey()==EKeys::P)) { CloseSettings(); return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
void UShooterMouseSettingsWidget::NativeDestruct()
{
    if(bOpen) { CloseSettings(); }
    Super::NativeDestruct();
}