#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShooterWeaponDefinition.generated.h"
class UStaticMesh;
class USoundBase;
class UAnimMontage;
class AShooterBulletVisual;

/** 只保存可共享的武器配置。弹药、冷却与当前装备槽属于角色实例，绝不写回资源。 */
UCLASS(BlueprintType)
class MYSHOOT_API UShooterWeaponDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    FText DisplayName;
    /** 成功发射后的视角上抬角度；0 保持已有武器行为，不受鼠标灵敏度/反转影响。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0", ClampMax="15"))
    float RecoilPitch=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0", ClampMax="1"))
    float AimRecoilMultiplier=.65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0.01", ClampMax="0.2"))
    float RecoilKickDuration=.06f;
    /** 左手支撑点相对 Right_Weapon 插槽；仅配置该项的武器启用握持 IK。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip")
    bool bUseLeftHandIK=false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip")
    FVector LeftHandGripLocation=FVector::ZeroVector;
    /** 肘部朝向参考点，与握持点使用相同坐标系，避免手肘反折。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Grip")
    FVector LeftElbowHint=FVector::ZeroVector;
    /** 每颗弹丸的伤害；霰弹枪会先汇总同一目标的命中，再提交一次 GAS 伤害。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.01"))
    float Damage = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.01"))
    float FireInterval = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="1"))
    float Range = 10000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
    bool bAutomatic = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="1", ClampMax="32"))
    int32 PelletCount = 1;
    /** 散布圆锥半角，单位度。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0", ClampMax="30"))
    float SpreadHalfAngle = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1"))
    int32 MagazineCapacity = 30;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
    int32 InitialReserveAmmo = 90;
    /** 对应武器的备用弹药上限；拾取不改变弹匣数量。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
    int32 MaxReserveAmmo = 90;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0.01"))
    float ReloadDuration = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    TObjectPtr<USoundBase> FireSound;
    /** 可取消的整段换弹音；切枪、死亡、暂停及换弹结束时由表现组件停止。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    TObjectPtr<USoundBase> ReloadSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ClampMin="0", ClampMax="2"))
    float FireSoundVolume = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    TSubclassOf<AShooterBulletVisual> BulletVisualClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ClampMin="100"))
    float BulletVisualSpeed = 18000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    TObjectPtr<UAnimMontage> FireMontage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    TObjectPtr<UAnimMontage> AimFireMontage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    TObjectPtr<UAnimMontage> ReloadMontage;
    /** 为空时使用原蓝图步枪；静态模型挂在原武器节点下，继续跟随手部动画。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    TObjectPtr<UStaticMesh> WeaponMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    FTransform MeshTransform;
    /** 相对原 Weapon_mesh 的枪口位置，单位厘米，与伤害射线起点无关。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    FVector MuzzleLocation = FVector::ZeroVector;
};
