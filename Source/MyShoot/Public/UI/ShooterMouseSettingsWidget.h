#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/ShooterGameMode.h"
#include "ShooterMouseSettingsWidget.generated.h"
class UButton;
class USlider;
class UTextBlock;
class UBorder;
/** 独立设置面板：界面只读写用户偏好；不持有武器数值或修改游戏对局规则。 */
UCLASS()
class MYSHOOT_API UShooterMouseSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void ShowForState(EShooterRoundState State);
    bool IsSettingsOpen() const { return bOpen; }
    UFUNCTION() void OpenSettings();
    UFUNCTION() void CloseSettings();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    virtual void NativeDestruct() override;
private:
    void RefreshValues();
    UFUNCTION() void HipChanged(float Value);
    UFUNCTION() void AimChanged(float Value);
    UFUNCTION() void ResetClicked();
    UPROPERTY(Transient) TObjectPtr<UButton> SettingsButton;
    UPROPERTY(Transient) TObjectPtr<UBorder> Backdrop;
    UPROPERTY(Transient) TObjectPtr<USlider> HipSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> AimSlider;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HipValue;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> AimValue;
    bool bRefreshing=false;
    bool bOpen=false;
    EShooterRoundState CurrentState=EShooterRoundState::Menu;
};