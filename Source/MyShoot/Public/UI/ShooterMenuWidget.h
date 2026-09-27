#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/ShooterGameMode.h"
#include "ShooterMenuWidget.generated.h"
class UButton;
class UTextBlock;
class UWidget;

/** 开始/暂停菜单沿用用户原图；结果页单独显示，按钮只调用 GameMode 规则入口。 */
UCLASS()
class MYSHOOT_API UShooterMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void ShowState(EShooterRoundState State);
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> StartPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> ResultPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> StartButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> EndButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> QuitButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RestartButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> MenuButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ResultQuitButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ResultTitle;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> MenuHint;
private:
    UFUNCTION() void StartClicked();
    UFUNCTION() void EndClicked();
    UFUNCTION() void QuitClicked();
    UFUNCTION() void RestartClicked();
    AShooterGameMode* GetShooterGameMode() const;
};
