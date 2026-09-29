#include "UI/ShooterAimReticleWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SSpacer.h"

TSharedRef<SWidget> UShooterAimReticleWidget::RebuildWidget()
{
    return SNew(SSpacer);
}

int32 UShooterAimReticleWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const int32 LastLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    // 使用当前玩家视口的几何中心，分辨率变化时仍与摄像机射线中心对齐。
    // 直径为 6 个 UI 单位，无十字线、外圈或额外贴图，也不拦截鼠标操作。
    const FVector2D DotSize(6.f, 6.f);
    const FVector2D DotPosition = (AllottedGeometry.GetLocalSize() - DotSize) * 0.5f;
    static const FSlateRoundedBoxBrush DotBrush(FLinearColor::White, 3.f);
    FSlateDrawElement::MakeBox(OutDrawElements, LastLayer + 1,
        AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotPosition)),
        &DotBrush, ESlateDrawEffect::None, FLinearColor(1.f, 0.f, 0.f, 1.f));
    return LastLayer + 1;
}