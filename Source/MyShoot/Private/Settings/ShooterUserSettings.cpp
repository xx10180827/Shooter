#include "Settings/ShooterUserSettings.h"

namespace
{
    // 配置可能来自旧版本或手动编辑，入口统一处理非法值与越界。
    FVector2D SafeAxes(FVector2D Value, double Fallback, double Maximum)
    {
        auto Safe=[&](double V) { return FMath::IsFinite(V)?FMath::Clamp(V,.1,Maximum):Fallback; };
        return FVector2D(Safe(Value.X),Safe(Value.Y));
    }
}
UShooterUserSettings::UShooterUserSettings() { ResetMouseSettings(); }
UShooterUserSettings* UShooterUserSettings::Get() { return Cast<UShooterUserSettings>(GetGameUserSettings()); }
FName UShooterUserSettings::DefaultAimProfile() { return TEXT("Default"); }
FVector2D UShooterUserSettings::GetHipSensitivity() const { return SafeAxes(HipSensitivity,.8,3.); }
FVector2D UShooterUserSettings::GetAimSensitivity(FName Profile) const
{
    // 瞄具配置缺失时回退到 Default，使旧武器和新增瞄具都可用。
    const auto* Found=AimSensitivityProfiles.Find(Profile);
    if(!Found) { Found=AimSensitivityProfiles.Find(DefaultAimProfile()); }
    return Found?SafeAxes(*Found,.75,2.):FVector2D(.75,.75);
}
FVector2D UShooterUserSettings::ResolveSensitivity(FName Profile,float AimAlpha) const
{
    // 只依赖瞄准状态混合倍率；镜头 FOV、闪避反馈不参与转向计算。
    const auto Hip=GetHipSensitivity(); const auto Aim=GetAimSensitivity(Profile);
    const double Alpha=FMath::IsFinite(AimAlpha)?FMath::Clamp(double(AimAlpha),0.,1.):0.;
    return FVector2D(Hip.X*FMath::Lerp(1.,Aim.X,Alpha),Hip.Y*FMath::Lerp(1.,Aim.Y,Alpha));
}
void UShooterUserSettings::SetHipSensitivity(FVector2D Value) { HipSensitivity=SafeAxes(Value,.8,3.); }
void UShooterUserSettings::SetAimSensitivity(FName Profile,FVector2D Value)
{
    AimSensitivityProfiles.Add(Profile.IsNone()?DefaultAimProfile():Profile,SafeAxes(Value,.75,2.));
}
void UShooterUserSettings::ResetMouseSettings()
{
    // 恢复默认只重置鼠标配置，不触碰分辨率等引擎用户设置。
    HipSensitivity=FVector2D(.8,.8);
    AimSensitivityProfiles.Reset(); AimSensitivityProfiles.Add(DefaultAimProfile(),FVector2D(.75,.75));
}
void UShooterUserSettings::SanitizeMouseSettings()
{
    HipSensitivity=GetHipSensitivity();
    for(auto& Pair:AimSensitivityProfiles) { Pair.Value=SafeAxes(Pair.Value,.75,2.); }
    if(!AimSensitivityProfiles.Contains(DefaultAimProfile())) { AimSensitivityProfiles.Add(DefaultAimProfile(),FVector2D(.75,.75)); }
}
void UShooterUserSettings::SetToDefaults() { Super::SetToDefaults(); ResetMouseSettings(); }
void UShooterUserSettings::LoadSettings(bool bForceReload) { Super::LoadSettings(bForceReload); SanitizeMouseSettings(); }
void UShooterUserSettings::ValidateSettings() { Super::ValidateSettings(); SanitizeMouseSettings(); }