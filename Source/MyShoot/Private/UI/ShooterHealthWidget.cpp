#include "UI/ShooterHealthWidget.h"
#include "Characters/ShooterCharacterBase.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UShooterHealthWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ObserveCharacter(Cast<AShooterCharacterBase>(GetOwningPlayerPawn()));
}
void UShooterHealthWidget::ObserveCharacter(AShooterCharacterBase* Character)
{
    UnbindAttributes();
    ObservedCharacter = Character;
    UAbilitySystemComponent* ASC = IsValid(Character) ? Character->GetAbilitySystemComponent() : nullptr;
    if (ASC)
    {
        ObservedASC = ASC;
        HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UShooterAttributeSet::GetHealthAttribute())
            .AddUObject(this, &UShooterHealthWidget::HandleAttributeChanged);
        MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UShooterAttributeSet::GetMaxHealthAttribute())
            .AddUObject(this, &UShooterHealthWidget::HandleAttributeChanged);
    }
    // 先订阅再读初值；解绑旧 Pawn 后不保留旧角色的回调。
    RefreshHealth();
}
void UShooterHealthWidget::UnbindAttributes()
{
    if (UAbilitySystemComponent* ASC = ObservedASC.Get())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UShooterAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
        ASC->GetGameplayAttributeValueChangeDelegate(UShooterAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
    }
    HealthHandle.Reset();
    MaxHealthHandle.Reset();
    ObservedASC.Reset();
}
void UShooterHealthWidget::HandleAttributeChanged(const FOnAttributeChangeData& Data) { RefreshHealth(); }
float UShooterHealthWidget::GetDisplayedFraction() const
{
    return FMath::Clamp(DisplayedHealth / FMath::Max(1.0f, DisplayedMaxHealth), 0.0f, 1.0f);
}
void UShooterHealthWidget::RefreshHealth()
{
    if (AShooterCharacterBase* Character = ObservedCharacter.Get())
    {
        DisplayedMaxHealth = FMath::Max(1.0f, Character->GetGASMaxHealth());
        DisplayedHealth = FMath::Clamp(Character->GetGASHealth(), 0.0f, DisplayedMaxHealth);
    }
    else
    {
        // 死亡后 Pawn 被销毁仍保留空血条，直到控制器接管新的角色或退出关卡。
        DisplayedHealth = 0;
    }
    if (HealthFill)
    {
        HealthFill->SetPercent(GetDisplayedFraction());
        HealthFill->SetFillColorAndOpacity(GetDisplayedFraction() <= 0.3f
            ? FLinearColor(1.0f, 0.06f, 0.025f) : FLinearColor(0.02f, 0.7f, 1.0f));
    }
    if (HealthValue)
    {
        HealthValue->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),
            FMath::CeilToInt(DisplayedHealth), FMath::CeilToInt(DisplayedMaxHealth))));
    }
}
void UShooterHealthWidget::NativeDestruct()
{
    UnbindAttributes();
    ObservedCharacter.Reset();
    Super::NativeDestruct();
}
