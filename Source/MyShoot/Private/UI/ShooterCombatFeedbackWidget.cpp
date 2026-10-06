#include "UI/ShooterCombatFeedbackWidget.h"
#include "Combat/ShooterCombatFeedbackComponent.h"
#include "Combat/ShooterFeedbackConfig.h"
#include "Engine/Texture2D.h"
#include "Player/ShooterPlayerController.h"
#include "UI/ShooterDashWidget.h"
#include "Widgets/Layout/SSpacer.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UShooterCombatFeedbackWidget::RebuildWidget() { ForceVolatile(true); return SNew(SSpacer); }
void UShooterCombatFeedbackWidget::ObserveFeedback(UShooterCombatFeedbackComponent* Value)
{
    Feedback = Value; IconBrush.DrawAs = ESlateBrushDrawType::Image;
    IconBrush.SetResourceObject(Value && Value->GetConfig() ? Value->GetConfig()->KillIcon : nullptr);
}
int32 UShooterCombatFeedbackWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
    const FWidgetStyle& Style, bool bParentEnabled) const
{
    const int32 Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled) + 1;
    const auto* Config = Feedback ? Feedback->GetConfig() : nullptr;
    if (!Config) { return Layer; }
    const FVector2D Size = Geometry.GetLocalSize(), Center = Size * .5f;
    const float HitAlpha = Feedback->GetMarkerAlpha();
    if (HitAlpha > 0.f)
    {
        FLinearColor Color = Feedback->IsKillMarker() ? Config->KillColor : Config->HitColor; Color.A *= HitAlpha;
        // 中心留空；与腰射准星/瞄准红点并存，不覆盖中心落点。
        for (int32 X : {-1, 1}) { for (int32 Y : {-1, 1})
        {
            const FVector2D Direction(X, Y);
            const float Inner = FMath::Max(1.f, Config->MarkerInnerOffset);
            const float Outer = Inner + FMath::Max(1.f, Config->MarkerLineLength);
            TArray<FVector2D> Points = {Center + Direction * Inner, Center + Direction * Outer};
            const float Thickness = FMath::Max(1.f, Config->MarkerThickness);
            // 深色描边保证亮墙和血雾背景上仍能辨认，中心红点保持无遮挡。
            if (Config->MarkerOutline > 0.f)
            {
                FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points,
                    ESlateDrawEffect::None, FLinearColor(.015f,.015f,.015f,HitAlpha * .9f), true,
                    Thickness + Config->MarkerOutline * 2.f);
            }
            FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
        } }
    }
    const float Alpha = Feedback->GetIconAlpha();
    if (Alpha > 0.f)
    {
        float StaminaBottom = Size.Y * .64f + 90.f;
        if (const auto* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
        {
            const auto* Dash = PC->GetDashWidget();
            if (Dash && Dash->GetCachedGeometry().GetLocalSize().Y > 0.f)
            { StaminaBottom = Geometry.AbsoluteToLocal(Dash->GetStaminaBottomInAbsoluteSpace()).Y; }
        }
        // 图标放在体力条下方；超小窗口按可用空间缩小，弹出最大帧也不穿过屏幕底边。
        const float DesiredSide = FMath::Clamp(Config->IconSize, 8.f, 384.f) * Feedback->GetIconScale();
        const float Side = FMath::Min(DesiredSide, FMath::Max(8.f, float(Size.Y - StaminaBottom - 24.f)));
        const float MinY = StaminaBottom + 12.f + Side * .5f, MaxY = Size.Y - 12.f - Side * .5f;
        const float CenterY = FMath::Min(FMath::Max(float(Size.Y) * FMath::Clamp(Config->IconScreenY, 0.f, 1.f), MinY), MaxY);
        const FVector2D IconCenter(Size.X * .5f, CenterY);
        if (IconBrush.GetResourceObject())
        {
            FSlateDrawElement::MakeBox(Elements, Layer + 1,
                Geometry.ToPaintGeometry(FVector2D(Side, Side), FSlateLayoutTransform(IconCenter - FVector2D(Side * .5f))),
                &IconBrush, ESlateDrawEffect::None, FLinearColor(1, 1, 1, Alpha));
        }
        // 倍数单独绘制；未来更换单杀/多杀图标不会改变计数规则。
        const FString Label = Feedback->GetMultiKillCount() > 1 ? FString::Printf(TEXT("\u00d7%d"), Feedback->GetMultiKillCount()) : TEXT("KILL");
        FLinearColor Color = Config->KillColor; Color.A *= Alpha;
        FSlateDrawElement::MakeText(Elements, Layer + 2,
            Geometry.ToPaintGeometry(FVector2D(120, 28), FSlateLayoutTransform(IconCenter + FVector2D(Side * .5f + 6, -16))),
            Label, FCoreStyle::GetDefaultFontStyle("Bold", 28), ESlateDrawEffect::None, Color);
    }
    return Layer + 2;
}
