#include "UI/ShooterAmmoWidget.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

void UShooterAmmoWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ObserveWeapon(GetOwningPlayerPawn() ? GetOwningPlayerPawn()->FindComponentByClass<UShooterWeaponComponent>() : nullptr);
}
void UShooterAmmoWidget::ObserveWeapon(UShooterWeaponComponent* Weapon)
{
    if (UShooterWeaponComponent* Old = ObservedWeapon.Get())
    {
        Old->OnAmmoChanged.RemoveDynamic(this, &UShooterAmmoWidget::OnAmmoChanged);
        Old->OnReserveAmmoChanged.RemoveDynamic(this, &UShooterAmmoWidget::OnAmmoChanged);
        Old->OnReloadStarted.RemoveDynamic(this, &UShooterAmmoWidget::OnReloadStarted);
        Old->OnReloadFinished.RemoveDynamic(this, &UShooterAmmoWidget::OnReloadFinished);
    }
    ObservedWeapon = Weapon;
    if (Weapon)
    {
        Weapon->OnAmmoChanged.AddUniqueDynamic(this, &UShooterAmmoWidget::OnAmmoChanged);
        Weapon->OnReserveAmmoChanged.AddUniqueDynamic(this, &UShooterAmmoWidget::OnAmmoChanged);
        Weapon->OnReloadStarted.AddUniqueDynamic(this, &UShooterAmmoWidget::OnReloadStarted);
        Weapon->OnReloadFinished.AddUniqueDynamic(this, &UShooterAmmoWidget::OnReloadFinished);
    }
    RefreshAmmo();
}
void UShooterAmmoWidget::RefreshAmmo()
{
    const UShooterWeaponComponent* Weapon = ObservedWeapon.Get();
    DisplayedAmmo = Weapon ? Weapon->GetCurrentAmmo() : 0;
    DisplayedReserve = Weapon ? Weapon->GetReserveAmmo() : 0;
    bDisplayedReloading = Weapon && Weapon->IsReloading();
    if (AmmoValue) { AmmoValue->SetText(FText::FromString(FString::Printf(TEXT("%02d / %03d"), DisplayedAmmo, DisplayedReserve))); }
    if (AmmoStatus)
    {
        const TCHAR* Status = bDisplayedReloading ? TEXT("RELOADING...") :
            DisplayedAmmo == 0 ? (DisplayedReserve > 0 ? TEXT("R  RELOAD") : TEXT("NO AMMO")) : TEXT("R  RELOAD  |  P  MENU");
        AmmoStatus->SetText(FText::FromString(Status));
        AmmoStatus->SetColorAndOpacity(FSlateColor(DisplayedAmmo == 0 ? FLinearColor(1,0.2f,0.08f) : FLinearColor(0.5f,0.8f,0.9f)));
    }
}
void UShooterAmmoWidget::OnAmmoChanged(int32 OldValue, int32 NewValue) { RefreshAmmo(); }
void UShooterAmmoWidget::OnReloadStarted(float Duration) { RefreshAmmo(); }
void UShooterAmmoWidget::OnReloadFinished(bool bSucceeded) { RefreshAmmo(); }
void UShooterAmmoWidget::NativeDestruct()
{
    ObserveWeapon(nullptr);
    Super::NativeDestruct();
}
