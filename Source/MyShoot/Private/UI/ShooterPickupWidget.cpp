#include "UI/ShooterPickupWidget.h"
#include "Interaction/ShooterInteractionComponent.h"
#include "GameFramework/Pawn.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
TSharedRef<SWidget> UShooterPickupWidget::RebuildWidget()
{
    return SNew(SOverlay)+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0,130,0,0))
        [SAssignNew(PromptText,STextBlock).Justification(ETextJustify::Center)
        .Font(FCoreStyle::GetDefaultFontStyle("Bold",18)).ColorAndOpacity(FLinearColor::White)
        .ShadowColorAndOpacity(FLinearColor::Black).ShadowOffset(FVector2D(1,1))];
}
void UShooterPickupWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    auto* Pawn=GetOwningPlayerPawn(); auto* I=Pawn?Pawn->FindComponentByClass<UShooterInteractionComponent>():nullptr;
    if(PromptText) { PromptText->SetText(I?I->GetPrompt():FText::GetEmpty()); }
}
void UShooterPickupWidget::ReleaseSlateResources(bool bReleaseChildren)
{ Super::ReleaseSlateResources(bReleaseChildren); PromptText.Reset(); }
