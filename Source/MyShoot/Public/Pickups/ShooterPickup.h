#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterPickup.generated.h"
class USphereComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UShooterWeaponComponent;
class UShooterInteractionComponent;
class AMyShooter;

/** 物品公共流程：触发器提供候选，交互组件检查准星/遮挡，领取成功后才消耗。 */
UCLASS(Abstract, Blueprintable)
class MYSHOOT_API AShooterPickup : public AActor
{
    GENERATED_BODY()
public:
    AShooterPickup();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup") FText ItemName;
    // 玩家根位置到触发器中心的最大距离，单位 cm；范围球同时用于进入/离开事件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pickup") TObjectPtr<USphereComponent> ProximityTrigger;
    // 只用于准星几何检测，不 Block 玩家或子弹；与展示模型分离，旋转模型不影响拾取区域。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pickup") TObjectPtr<UBoxComponent> FocusBounds;
    // 后续缩放、旋转、光效都挂在此节点下，不修改范围触发器和领取规则。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pickup|Presentation") TObjectPtr<USceneComponent> PresentationRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pickup|Presentation") TObjectPtr<UStaticMeshComponent> DisplayMesh;
    // 地面标记独立于展示模型；未来模型旋转、缩放不会带动光圈或触发器。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pickup|Marker") TObjectPtr<UStaticMeshComponent> GroundMarker;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup|Marker", meta=(ClampMin="5", ClampMax="200", Units="cm")) float MarkerRadius=48.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup|Marker", meta=(ClampMin="0.5", ClampMax="10", Units="cm")) float MarkerGroundOffset=2.f;
    // 只投射静态地面，不把角色或敌人当作地板；无地面时隐藏标记。
    UFUNCTION(CallInEditor, Category="Pickup|Marker") void RefreshGroundMarker();
    virtual void OnConstruction(const FTransform& Transform) override;
    UFUNCTION(BlueprintPure, Category="Pickup") bool IsConsumed() const { return bConsumed; }
    bool IsPlayerInRange(const AMyShooter* Player) const;
    bool IntersectFocus(const FVector& Start,const FVector& End,FVector& HitPoint) const;
    virtual bool CanReceive(const UShooterWeaponComponent* Weapon,FText& Reason) const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual bool Grant(UShooterWeaponComponent* Weapon);
    UFUNCTION(BlueprintImplementableEvent, Category="Pickup|Presentation") void OnPickupGranted(AMyShooter* Player);
private:
    friend class UShooterInteractionComponent;
    bool TryCollect(AMyShooter* Player);
    bool bConsumed=false;
    bool bGranting=false;
    UFUNCTION() void OnEnter(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex,bool bSweep,const FHitResult& Hit);
    UFUNCTION() void OnLeave(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex);
};
