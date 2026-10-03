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
        Old->OnInventoryChanged.RemoveDynamic(this,&UShooterAmmoWidget::RefreshAmmo);
        Old->OnWeaponChanged.RemoveDynamic(this, &UShooterAmmoWidget::OnWeaponChanged);
        Old->OnAmmoChanged.RemoveDynamic(this, &UShooterAmmoWidget::OnAmmoChanged);
        Old->OnReserveAmmoChanged.RemoveDynamic(this, &UShooterAmmoWidget::OnAmmoChanged);
        Old->OnReloadStarted.RemoveDynamic(this, &UShooterAmmoWidget::OnReloadStarted);
        Old->OnReloadFinished.RemoveDynamic(this, &UShooterAmmoWidget::OnReloadFinished);
    }
    ObservedWeapon = Weapon;
    if (Weapon)
    {
        Weapon->OnInventoryChanged.AddUniqueDynamic(this,&UShooterAmmoWidget::RefreshAmmo);
        Weapon->OnWeaponChanged.AddUniqueDynamic(this, &UShooterAmmoWidget::OnWeaponChanged);
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
            DisplayedAmmo == 0 ? (DisplayedReserve > 0 ? TEXT("R  RELOAD") : TEXT("NO AMMO")) : (Weapon&&Weapon->GetWeaponCount()>1?TEXT("1/2 SWITCH | R RELOAD"):TEXT("F PICKUP | R RELOAD"));
        AmmoStatus->SetText(FText::FromString(FString::Printf(TEXT("%s | %s"), Weapon ? *Weapon->GetWeaponDisplayName().ToString() : TEXT(""), Status)));
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

void UShooterAmmoWidget::OnWeaponChanged(int32 OldSlot, int32 NewSlot) { RefreshAmmo(); }
