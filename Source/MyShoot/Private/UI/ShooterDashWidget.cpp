#include "UI/ShooterDashWidget.h"
#include "Movement/ShooterDashComponent.h"
#include "Player/ShooterPlayerController.h"
#include "UI/ShooterHealthWidget.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/Pawn.h"
#include "Widgets/Layout/SSpacer.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Materials/MaterialInstanceDynamic.h"
UShooterDashWidget::UShooterDashWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    StaminaMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/UI/M_StaminaEnergy.M_StaminaEnergy")));
}

TSharedRef<SWidget> UShooterDashWidget::RebuildWidget() { return SNew(SSpacer); }

void UShooterDashWidget::NativeConstruct()
{
    Super::NativeConstruct(); ForceVolatile(true); LastFraction=-1.f;
    if(auto* Material=StaminaMaterial.LoadSynchronous())
    {
        EnergyMaterial=UMaterialInstanceDynamic::Create(Material,this);
        EnergyBrush.SetResourceObject(EnergyMaterial); EnergyBrush.DrawAs=ESlateBrushDrawType::Image;
        EnergyBrush.ImageSize=FVector2D(270,90);
    }
}

void UShooterDashWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    const auto* Pawn=GetOwningPlayerPawn(); const auto* Dash=Pawn?Pawn->FindComponentByClass<UShooterDashComponent>():nullptr;
    const float Fraction=Dash?FMath::Clamp(Dash->GetStamina()/FMath::Max(1.f,Dash->GetMaxStamina()),0.f,1.f):0.f;
    if(EnergyMaterial&&!FMath::IsNearlyEqual(LastFraction,Fraction))
    { EnergyMaterial->SetScalarParameterValue(TEXT("StaminaFraction"),Fraction); LastFraction=Fraction; }
}
void UShooterDashWidget::NativeDestruct()
{
    EnergyBrush.SetResourceObject(nullptr); EnergyMaterial=nullptr; Super::NativeDestruct();
}

int32 UShooterDashWidget::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& Elements,int32 LayerId,const FWidgetStyle& Style,bool bParentEnabled) const
{
    const int32 Base=Super::NativePaint(Args,Geometry,CullingRect,Elements,LayerId,Style,bParentEnabled)+1;
    const auto* Pawn=GetOwningPlayerPawn(); const auto* Dash=Pawn?Pawn->FindComponentByClass<UShooterDashComponent>():nullptr;
    if(!Dash) { return Base; }
    const FVector2D Size=Geometry.GetLocalSize();
    const float Fraction=FMath::Clamp(Dash->GetStamina()/FMath::Max(1.f,Dash->GetMaxStamina()),0.f,1.f);
    const float Cooldown=Dash->GetCooldownRemaining();
    const FLinearColor Cyan(.08f,.8f,1.f,1.f),Dim(.04f,.075f,.10f,.85f),Amber(1.f,.55f,.1f,1.f);
    auto Lines=[&](const TArray<FVector2D>& Points,FLinearColor Color,float Thickness,int32 Layer)
    { FSlateDrawElement::MakeLines(Elements,Layer,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Thickness); };
    auto Text=[&](FVector2D Position,const FString& Value,int32 FontSize,FLinearColor Color)
    { FSlateDrawElement::MakeText(Elements,Base+3,Geometry.ToPaintGeometry(FVector2D(140,24),FSlateLayoutTransform(Position)),Value,FCoreStyle::GetDefaultFontStyle("Bold",FontSize),ESlateDrawEffect::None,Color); };
    // 弧形属于玩家屏幕 HUD，不挂模型；按视口相对位置放在准星下方。
    const FVector2D Center(Size.X*.5f,Size.Y*.64f);
    if(EnergyMaterial)
    {
        // 用户提供的弧形素材，透明背景；材质仅按体力比例熄灭蓝色区域，外框始终保留。
        FSlateDrawElement::MakeBox(Elements,Base+1,Geometry.ToPaintGeometry(FVector2D(270,90),FSlateLayoutTransform(Center+FVector2D(-135,0))),&EnergyBrush);
        Text(Center+FVector2D(-13,44),FString::Printf(TEXT("%d"),FMath::RoundToInt(Dash->GetStamina())),12,FLinearColor::White);
    }
    else
    {
        // 资源缺失时保留原生弧线兜底，旧版本绘制也留在这里便于对比。
        TArray<FVector2D> Background,Fill;
        for(int32 Index=0;Index<=64;++Index)
        {
            const float Angle=FMath::DegreesToRadians(160.f-140.f*Index/64.f);
            Background.Add(Center+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*76.f);
            const float FillAngle=FMath::DegreesToRadians(160.f-140.f*Fraction*Index/64.f);
            Fill.Add(Center+FVector2D(FMath::Cos(FillAngle),FMath::Sin(FillAngle))*76.f);
        }
        Lines(Background,Dim,8.f,Base); if(Fraction>0.f) { Lines(Fill,Fraction<.25f?Amber:Cyan,4.f,Base+1); }
        Text(Center+FVector2D(-16,40),FString::Printf(TEXT("%d"),FMath::RoundToInt(Dash->GetStamina())),12,FLinearColor::White);
    }
    // 使用现有 HealthPanel 的实际几何定位，随血条和 DPI 缩放保持在其右侧。
    FVector2D Icon(526,Size.Y-124);
    if(const auto* PC=Cast<AShooterPlayerController>(GetOwningPlayer()))
    {
        if(const auto* Health=PC->GetHealthWidget())
        {
            if(const auto* Panel=Health->WidgetTree?Health->WidgetTree->FindWidget(TEXT("HealthPanel")):nullptr)
            {
                const auto& PanelGeometry=Panel->GetCachedGeometry();
                if(PanelGeometry.GetLocalSize().X>0)
                { Icon=Geometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(FVector2D(PanelGeometry.GetLocalSize().X+12,50))); }
            }
        }
    }
    const bool bReady=Cooldown<=0.f&&Dash->GetStamina()>=25.f&&!Dash->IsDashing();
    const FLinearColor Color=bReady?Cyan:Amber;
    FSlateDrawElement::MakeBox(Elements,Base,Geometry.ToPaintGeometry(FVector2D(64,64),FSlateLayoutTransform(Icon)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Dim);
    Lines({Icon,Icon+FVector2D(64,0),Icon+FVector2D(64,64),Icon+FVector2D(0,64),Icon},Color,2.f,Base+1);
    // 双箭头是首版闪避图标；冷却数值来自实际 GE，而不是另一套 UI 计时器。
    Lines({Icon+FVector2D(13,11),Icon+FVector2D(23,19),Icon+FVector2D(13,27)},Color,3.f,Base+2);
    Lines({Icon+FVector2D(29,11),Icon+FVector2D(39,19),Icon+FVector2D(29,27)},Color,3.f,Base+2);
    Text(Icon+FVector2D(6,35),Dash->IsDashing()?TEXT("DASH"):Cooldown>0.f?FString::Printf(TEXT("%.1fs"),Cooldown):Dash->GetStamina()<25.f?TEXT("LOW"):TEXT("READY"),11,Color);
    Text(Icon+FVector2D(10,69),TEXT("SHIFT"),11,FLinearColor::White);
    return Base+4;
}