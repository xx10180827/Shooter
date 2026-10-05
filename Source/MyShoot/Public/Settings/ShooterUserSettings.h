#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ShooterUserSettings.generated.h"

/** 本机用户偏好：与角色、武器运行状态分离；X/Y 独立，瞄准设置按稳定配置名保存。 */
UCLASS(Config=GameUserSettings)
class MYSHOOT_API UShooterUserSettings : public UGameUserSettings
{
    GENERATED_BODY()
public:
    UShooterUserSettings();
    static UShooterUserSettings* Get();
    static FName DefaultAimProfile();
    FVector2D GetHipSensitivity() const;
    FVector2D GetAimSensitivity(FName Profile) const;
    FVector2D ResolveSensitivity(FName Profile, float AimAlpha) const;
    // UI 首版传相同的两个值；未来双滑条直接分别传 X/Y，无需更改存储结构。
    void SetHipSensitivity(FVector2D Value);
    void SetAimSensitivity(FName Profile, FVector2D Value);
    void ResetMouseSettings();
    virtual void SetToDefaults() override;
    virtual void LoadSettings(bool bForceReload=false) override;
    virtual void ValidateSettings() override;
private:
    void SanitizeMouseSettings();
    UPROPERTY(Config) FVector2D HipSensitivity=FVector2D(.8,.8);
    UPROPERTY(Config) TMap<FName,FVector2D> AimSensitivityProfiles;
};