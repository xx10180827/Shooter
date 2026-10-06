#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShooterFeedbackConfig.generated.h"
class USoundBase;
class UTexture2D;
class UParticleSystem;

/** 可替换的打击感资源与参数，伤害结算不依赖此资产。 */
UCLASS(BlueprintType)
class MYSHOOT_API UShooterFeedbackConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Assets") TObjectPtr<UTexture2D> KillIcon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Assets") TObjectPtr<USoundBase> HitSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Assets") TObjectPtr<USoundBase> KillSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Assets") TObjectPtr<UParticleSystem> BloodEffect;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD") FLinearColor HitColor = FLinearColor(.95f,.98f,1.f);
    /** 四条命中线保留中心空隙；长度、粗细和描边可独立调整。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="1")) float MarkerInnerOffset = 9.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="1")) float MarkerLineLength = 13.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="1")) float MarkerThickness = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="0")) float MarkerOutline = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD") FLinearColor KillColor = FLinearColor(1.f,.65f,.12f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="0.01")) float HitDuration = .20f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="0.01")) float KillMarkerDuration = .30f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="0.1")) float KillIconDuration = .85f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="8")) float IconSize = 192.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="0", ClampMax="1")) float IconScreenY = .84f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MultiKill", meta=(ClampMin="0")) float MultiKillWindow = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0", ClampMax="1")) float HitVolume = .55f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0", ClampMax="1")) float KillVolume = .45f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0", ClampMax="8")) int32 MaxBloodBurstsPerShot = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="1", ClampMax="32")) int32 MaxActiveBloodBursts = 8;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0.1", ClampMax="3")) float BloodScale = 1.f;
};
